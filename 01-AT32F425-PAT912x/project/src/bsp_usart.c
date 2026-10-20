#include "bsp_usart.h"
#include "bsp_spi.h"
#include "at32f425_board.h"
#include <string.h>
#include <stdarg.h>
#include "bsp_config.h"
#include <stdlib.h>

uint8_t  usart_rx_buffer[USART_BUFFER_SIZE];
volatile uint16_t hw_usart_rx_index   = 0;
volatile uint16_t hw_usart_read_index = 0;

volatile uint8_t g_debug_mode   = 1;
volatile uint8_t g_is_streaming = 0;

static uint8_t  usart_tx_dma_buf[USART_TX_DMA_BUF_SIZE];
volatile uint8_t g_usart_tx_busy = 0;

static void bsp_usart_init(uint32_t baudrate);
static void bsp_usart_dma_init(void);
static void execute_cmd(const char *cmd);

extern volatile uint8_t  g_pulse_output_en;
extern volatile uint32_t g_pulse_start_us;

/* ----------------------------------------------------------------
 * 外部接口自适应初始化
 * ---------------------------------------------------------------- */
void bsp_external_interface_init(uint32_t baudrate)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_8;
    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init(GPIOA, &gpio_init_struct);

    bsp_delay_ms(5);

    if (gpio_input_data_bit_read(GPIOA, GPIO_PINS_8) != RESET)
    {
        bsp_usart_init(baudrate);
        bsp_usart_dma_init();
    }
}

/* ----------------------------------------------------------------
 * USART1 初始化
 * ---------------------------------------------------------------- */
static void bsp_usart_init(uint32_t baudrate)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK,  TRUE);
    crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK,   TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins           = GPIO_PINS_9 | GPIO_PINS_10;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init(GPIOA, &gpio_init_struct);

    gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE9,  GPIO_MUX_1);
    gpio_pin_mux_config(GPIOA, GPIO_PINS_SOURCE10, GPIO_MUX_1);

    usart_init(USART1, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(USART1, TRUE);
    usart_receiver_enable(USART1, TRUE);

    nvic_irq_enable(USART1_IRQn, 0, 0);
    usart_interrupt_enable(USART1, USART_RDBF_INT, TRUE);
    usart_enable(USART1, TRUE);

    while (usart_flag_get(USART1, USART_TDBE_FLAG) == RESET);
    usart_data_transmit(USART1, 0x55);
    while (usart_flag_get(USART1, USART_TDC_FLAG) == RESET);
}

/* ----------------------------------------------------------------
 * USART1 TX DMA 初始化
 * ---------------------------------------------------------------- */
static void bsp_usart_dma_init(void)
{
    dma_init_type dma_init_struct;

    dma_reset(DMA1_CHANNEL5);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.direction             = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.memory_base_addr      = (uint32_t)usart_tx_dma_buf;
    dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.memory_inc_enable     = TRUE;
    dma_init_struct.peripheral_base_addr  = (uint32_t)&USART1->dt;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority              = DMA_PRIORITY_MEDIUM;
    dma_init_struct.loop_mode_enable      = FALSE;
    dma_init_struct.buffer_size           = USART_TX_DMA_BUF_SIZE;
    dma_init(DMA1_CHANNEL5, &dma_init_struct);

    dma_channel_enable(DMA1_CHANNEL5, FALSE);
    dma_flexible_config(DMA1, FLEX_CHANNEL5, DMA_FLEXIBLE_UART1_TX);
    dma_interrupt_enable(DMA1_CHANNEL5, DMA_FDT_INT, TRUE);
    nvic_irq_enable(DMA1_Channel7_4_IRQn, 2, 0);
    usart_dma_transmitter_enable(USART1, TRUE);
}

/* ----------------------------------------------------------------
 * 格式化发送
 * ---------------------------------------------------------------- */
void bsp_usart_send(const char *fmt, ...)
{
    uint32_t t0 = system_us();
    while (g_usart_tx_busy)
    {
        if (elapsed_us(t0) > 10000U) { g_usart_tx_busy = 0; break; }
    }

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf((char*)usart_tx_dma_buf, USART_TX_DMA_BUF_SIZE, fmt, args);
    va_end(args);

    if (len <= 0) return;
    if (len > USART_TX_DMA_BUF_SIZE) len = USART_TX_DMA_BUF_SIZE;

    g_usart_tx_busy = 1;
    dma_channel_enable(DMA1_CHANNEL5, FALSE);
    dma_data_number_set(DMA1_CHANNEL5, (uint32_t)len);
    dma_channel_enable(DMA1_CHANNEL5, TRUE);
}

/* ----------------------------------------------------------------
 * DMA IRQ
 * ---------------------------------------------------------------- */
void DMA1_Channel7_4_IRQHandler(void)
{
    if (dma_interrupt_flag_get(DMA1_FDT4_FLAG) != RESET)
    {
        BMI_CS_HIGH();
        dma_flag_clear(DMA1_FDT4_FLAG);
        spi_dma_done = 1;
    }
    if (dma_interrupt_flag_get(DMA1_FDT5_FLAG) != RESET)
    {
        dma_flag_clear(DMA1_FDT5_FLAG);
        g_usart_tx_busy = 0;
    }
}

/* ----------------------------------------------------------------
 * 命令处理
 * ---------------------------------------------------------------- */
void usart_process_cmd(void)
{
    static char    line_buf[128];
    static uint8_t line_idx = 0;

    while (hw_usart_read_index != hw_usart_rx_index)
    {
        char c = usart_rx_buffer[hw_usart_read_index];
        hw_usart_read_index = (hw_usart_read_index + 1) & (USART_BUFFER_SIZE - 1);

        if (c == '\n' || c == '\r')
        {
            if (line_idx > 0)
            {
                line_buf[line_idx] = '\0';
                execute_cmd(line_buf);
                line_idx = 0;
            }
        }
        else if (line_idx < sizeof(line_buf) - 1)
        {
            line_buf[line_idx++] = c;
        }
    }
}

static void execute_cmd(const char *cmd)
{
    /* ── 帮助 ──────────────────────────────────────────────────── */
    if (strcmp(cmd, "help") == 0)
    {
        LOG_INFO(
            "Commands:\r\n"
            "  help / ver\r\n"
            "  start / stop\r\n"
            "  debug on|off\r\n"
            "  odr=50|100|200|400 / odr?\r\n"
            "  mode 0|1 / mode?\r\n"
            "  reset_settings\r\n"
            "  PULSE_ON / PULSE_OFF\r\n"
            "  calib start   -- run full calibration (auto)\r\n"
            "  calib status  -- print current calib params\r\n"
            "  calib reset   -- clear calib data\r\n"
        );
    }
    /* ── 版本 ──────────────────────────────────────────────────── */
    else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "ver") == 0)
    {
        LOG_INFO("HW:%s SW:%s-%s %s\r\n",
                config_get_hw_version(), SW_VERSION, __DATE__, __TIME__);
    }
    /* 出厂写入硬件版本：set hw FY-33-425430TTL */
    else if (strncmp(cmd, "set hw ", 7) == 0)
    {
        const char *ver = cmd + 7;
        if (config_set_hw_version(ver))
            LOG_INFO("[CFG] HW version set: %s\r\n", ver);
        else
            LOG_INFO("[CFG] FAIL: invalid version string\r\n");
    }
    else if (strcmp(cmd, "get hw") == 0)
    {
        LOG_INFO("[CFG] HW version: %s\r\n", config_get_hw_version());
    }
    /* ── 数据流 ────────────────────────────────────────────────── */
    else if (strcmp(cmd, "start") == 0)
    {
        g_is_streaming = 1;
        LOG_INFO("Stream ON ODR=%lu\r\n", config_get_odr());
    }
    else if (strcmp(cmd, "stop") == 0)
    {
        g_is_streaming = 0;
        LOG_INFO("Stream OFF\r\n");
    }
    /* ── 调试模式 ──────────────────────────────────────────────── */
    else if (strcmp(cmd, "debug on") == 0)
    {
        g_debug_mode = 1;
        LOG_INFO("DBG ON\r\n");
    }
    else if (strcmp(cmd, "debug off") == 0)
    {
        g_debug_mode = 0;
        LOG_INFO("DBG OFF\r\n");
    }
    /* ── ODR ───────────────────────────────────────────────────── */
    else if (strncmp(cmd, "odr=", 4) == 0)
    {
        uint32_t new_odr = (uint32_t)atoi(cmd + 4);
        if (config_set_odr(new_odr))
        {
            uint8_t was = g_is_streaming;
            g_is_streaming = 0;
            imu_set_odr(new_odr);
            imu_fusion_init((float)new_odr);
            g_is_streaming = was;
            LOG_INFO("ODR=%lu saved\r\n", new_odr);
        }
        else
        {
            LOG_INFO("ERR: 50/100/200/400 only\r\n");
        }
    }
    else if (strcmp(cmd, "odr?") == 0)
    {
        LOG_INFO("ODR=%lu\r\n", config_get_odr());
    }
    /* ── 恢复默认 ──────────────────────────────────────────────── */
    else if (strcmp(cmd, "reset_settings") == 0)
    {
        config_reset();
        uint32_t def = config_get_odr();
        imu_set_odr(def);
        imu_fusion_init((float)def);
        LOG_INFO("Reset ODR=%lu\r\n", def);
    }
    /* ── 脉冲输出 ──────────────────────────────────────────────── */
    else if (strncmp(cmd, "PULSE_ON", 8) == 0)
    {
        g_pulse_output_en = 1;
        LOG_INFO("PULSE ON\r\n");
    }
    else if (strncmp(cmd, "PULSE_OFF", 9) == 0)
    {
        g_pulse_output_en = 0;
        gpio_bits_reset(GPIOA, GPIO_PINS_2);
        g_pulse_start_us  = 0;
        LOG_INFO("PULSE OFF\r\n");
    }
    /* ── 融合模式 ──────────────────────────────────────────────── */
    else if (strncmp(cmd, "mode 0", 6) == 0)
    {
        imu_fusion_set_mode(FUSION_MODE_ACC);
        config_set_fusion_mode(0);
        LOG_INFO("Mode->ACC+LPF\r\n");
    }
    else if (strncmp(cmd, "mode 1", 6) == 0)
    {
        imu_fusion_set_mode(FUSION_MODE_VQF);
        config_set_fusion_mode(1);
        LOG_INFO("Mode->VQF\r\n");
    }
    else if (strncmp(cmd, "mode?", 5) == 0)
    {
        LOG_INFO("Mode:%d(%s)\r\n",
                 imu_fusion_get_mode(),
                 imu_fusion_get_mode() == FUSION_MODE_ACC ? "ACC" : "VQF");
    }
    /* ── 校准 ──────────────────────────────────────────────────── */
    else if (strcmp(cmd, "calib start")  == 0) { config_calib_start();        }
    else if (strcmp(cmd, "calib status") == 0) { config_calib_print_status(); }
    else if (strcmp(cmd, "calib reset")  == 0) { config_calib_reset();        }
    /* ── 未知命令 ──────────────────────────────────────────────── */
    else
    {
        LOG_INFO("?'%s'\r\n", cmd);
    }
}

/* ----------------------------------------------------------------
 * USART1 RX IRQ
 * ---------------------------------------------------------------- */
void USART1_IRQHandler(void)
{
    if (usart_interrupt_flag_get(USART1, USART_RDBF_FLAG) != RESET)
    {
        usart_rx_buffer[hw_usart_rx_index] = usart_data_receive(USART1);
        hw_usart_rx_index = (hw_usart_rx_index + 1) & (USART_BUFFER_SIZE - 1);
    }
}
