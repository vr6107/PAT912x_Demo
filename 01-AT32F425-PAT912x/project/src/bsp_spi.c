#include "bsp_spi.h"
#include <string.h>

uint8_t bmi_spi_tx_buf[SPI_DMA_BUF_SIZE];
uint8_t bmi_spi_rx_buf[SPI_DMA_BUF_SIZE];
volatile uint8_t spi_dma_done = 0;
volatile spi_op_type_t spi_current_op = SPI_OP_READ;

/* ----------------------------------------------------------------
 * SPI1 与 DMA 初始化
 * 通道分配（按官方例程）：
 *   CH3 → SPI1_TX
 *   CH4 → SPI1_RX  (中断在 DMA1_Channel7_4_IRQHandler)
 * ---------------------------------------------------------------- */
void bsp_spi_dma_init(void)
{
    gpio_init_type gpio_init_struct;
    spi_init_type  spi_init_struct;
    dma_init_type  dma_init_struct;

    /* 1. 使能时钟 */
    crm_periph_clock_enable(BMI_SPI_GPIO_CLK,      TRUE);
    crm_periph_clock_enable(BMI_CS_GPIO_CLK,       TRUE);
    crm_periph_clock_enable(BMI_SPI_CLK,           TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    /* 2. CS 引脚 (PB2) */
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode           = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins           = BMI_CS_PIN;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init(BMI_CS_GPIO, &gpio_init_struct);
    BMI_CS_HIGH();

    /* 3. SPI 引脚 (PA5=SCK, PA6=MISO, PA7=MOSI) */
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init_struct.gpio_pins = BMI_SCK_PIN | BMI_MOSI_PIN;
    gpio_init(BMI_SPI_GPIO, &gpio_init_struct);
    gpio_pin_mux_config(BMI_SPI_GPIO, GPIO_PINS_SOURCE5, GPIO_MUX_0);
    gpio_pin_mux_config(BMI_SPI_GPIO, GPIO_PINS_SOURCE7, GPIO_MUX_0);

    gpio_init_struct.gpio_pull = GPIO_PULL_UP;
    gpio_init_struct.gpio_pins = BMI_MISO_PIN;
    gpio_init(BMI_SPI_GPIO, &gpio_init_struct);
    gpio_pin_mux_config(BMI_SPI_GPIO, GPIO_PINS_SOURCE6, GPIO_MUX_0);

    /* 4. SPI1 参数配置 (Mode 0) */
    spi_default_para_init(&spi_init_struct);
    spi_init_struct.transmission_mode      = SPI_TRANSMIT_FULL_DUPLEX;
    spi_init_struct.master_slave_mode      = SPI_MODE_MASTER;
    spi_init_struct.mclk_freq_division     = SPI_MCLK_DIV_8;
    spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB;
    spi_init_struct.frame_bit_num          = SPI_FRAME_8BIT;
    spi_init_struct.clock_polarity         = SPI_CLOCK_POLARITY_LOW;
    spi_init_struct.clock_phase            = SPI_CLOCK_PHASE_1EDGE;
    spi_init_struct.cs_mode_selection      = SPI_CS_SOFTWARE_MODE;
    spi_init(BMI_SPI, &spi_init_struct);

    /* 5. DMA 通道配置 */
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable     = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.loop_mode_enable      = FALSE;

    /* TX: DMA1 Channel3 → SPI1_TX */
    dma_reset(DMA1_CHANNEL3);
    dma_init_struct.peripheral_base_addr = (uint32_t)&BMI_SPI->dt;
    dma_init_struct.memory_base_addr     = (uint32_t)bmi_spi_tx_buf;
    dma_init_struct.direction            = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.priority             = DMA_PRIORITY_MEDIUM;
    dma_init(DMA1_CHANNEL3, &dma_init_struct);

    /* RX: DMA1 Channel4 → SPI1_RX */
    dma_reset(DMA1_CHANNEL4);
    dma_init_struct.peripheral_base_addr = (uint32_t)&BMI_SPI->dt;
    dma_init_struct.memory_base_addr     = (uint32_t)bmi_spi_rx_buf;
    dma_init_struct.direction            = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.priority             = DMA_PRIORITY_HIGH;
    dma_init(DMA1_CHANNEL4, &dma_init_struct);

    /* 6. Flexible DMA 绑定（AT32F425 必须！） */
    dma_flexible_config(DMA1, FLEX_CHANNEL3, DMA_FLEXIBLE_SPI1_TX);
    dma_flexible_config(DMA1, FLEX_CHANNEL4, DMA_FLEXIBLE_SPI1_RX);

    /* 7. 开启 RX DMA 完成中断 (CH4 在 Channel7_4 向量) */
    dma_interrupt_enable(DMA1_CHANNEL4, DMA_FDT_INT, TRUE);
    nvic_irq_enable(DMA1_Channel7_4_IRQn, 1, 0);

    /* 8. TX 缓冲区初始化 */
    memset(bmi_spi_tx_buf, 0xFF, SPI_DMA_BUF_SIZE);

    /* 9. 使能 SPI DMA 请求 + SPI */
    spi_i2s_dma_receiver_enable(BMI_SPI,    TRUE);
    spi_i2s_dma_transmitter_enable(BMI_SPI, TRUE);
    spi_enable(BMI_SPI, TRUE);
}

/* ----------------------------------------------------------------
 * 发起 DMA 异步读寄存器 (非阻塞)
 * ---------------------------------------------------------------- */
void bmi32x_read_regs_dma(uint8_t reg, uint16_t actual_data_len)
{
    uint16_t total_len = actual_data_len + 2;

    bmi_spi_tx_buf[0] = reg | 0x80;
    memset(&bmi_spi_tx_buf[1], 0xFF, total_len - 1);

    dma_channel_enable(DMA1_CHANNEL4, FALSE);
    dma_channel_enable(DMA1_CHANNEL3, FALSE);

    dma_flag_clear(DMA1_FDT4_FLAG);
    dma_flag_clear(DMA1_FDT3_FLAG);

    dma_data_number_set(DMA1_CHANNEL4, total_len);
    dma_data_number_set(DMA1_CHANNEL3, total_len);

    spi_current_op = SPI_OP_READ;
    spi_dma_done   = 0;
    BMI_CS_LOW();

    dma_channel_enable(DMA1_CHANNEL4, TRUE);
    dma_channel_enable(DMA1_CHANNEL3, TRUE);
}

/* ----------------------------------------------------------------
 * 发起 DMA 异步写寄存器 (非阻塞)
 * ---------------------------------------------------------------- */
void bmi32x_write_regs_dma(uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t total_len = len + 1;

    bmi_spi_tx_buf[0] = reg & 0x7F;
    memcpy(&bmi_spi_tx_buf[1], data, len);

    dma_channel_enable(DMA1_CHANNEL4, FALSE);
    dma_channel_enable(DMA1_CHANNEL3, FALSE);

    dma_flag_clear(DMA1_FDT4_FLAG);
    dma_flag_clear(DMA1_FDT3_FLAG);

    dma_data_number_set(DMA1_CHANNEL4, total_len);
    dma_data_number_set(DMA1_CHANNEL3, total_len);

    spi_current_op = SPI_OP_WRITE;
    spi_dma_done   = 0;
    BMI_CS_LOW();

    dma_channel_enable(DMA1_CHANNEL4, TRUE);
    dma_channel_enable(DMA1_CHANNEL3, TRUE);
}
/* ================================================================
 * BMI430 专用 DMA 读写
 * 读：total_len = actual_data_len + 3  (1地址 + 2dummy)
 * 写：与 BMI32x 完全相同
 * ================================================================ */
void bmi430_read_regs_dma(uint8_t reg, uint16_t actual_data_len)
{
    uint16_t total_len = actual_data_len + 3;  /* +3: 1地址 + 2dummy */

    bmi_spi_tx_buf[0] = reg | 0x80U;
    memset(&bmi_spi_tx_buf[1], 0xFF, total_len - 1);

    dma_channel_enable(DMA1_CHANNEL4, FALSE);
    dma_channel_enable(DMA1_CHANNEL3, FALSE);
    dma_flag_clear(DMA1_FDT4_FLAG);
    dma_flag_clear(DMA1_FDT3_FLAG);

    dma_data_number_set(DMA1_CHANNEL4, total_len);
    dma_data_number_set(DMA1_CHANNEL3, total_len);

    spi_current_op = SPI_OP_READ;
    spi_dma_done   = 0;
    BMI_CS_LOW();

    dma_channel_enable(DMA1_CHANNEL4, TRUE);
    dma_channel_enable(DMA1_CHANNEL3, TRUE);
}

void bmi430_write_regs_dma(uint8_t reg, uint8_t *data, uint16_t len)
{
    uint16_t total_len = len + 1;

    bmi_spi_tx_buf[0] = reg & 0x7FU;
    memcpy(&bmi_spi_tx_buf[1], data, len);

    dma_channel_enable(DMA1_CHANNEL4, FALSE);
    dma_channel_enable(DMA1_CHANNEL3, FALSE);
    dma_flag_clear(DMA1_FDT4_FLAG);
    dma_flag_clear(DMA1_FDT3_FLAG);

    dma_data_number_set(DMA1_CHANNEL4, total_len);
    dma_data_number_set(DMA1_CHANNEL3, total_len);

    spi_current_op = SPI_OP_WRITE;
    spi_dma_done   = 0;
    BMI_CS_LOW();

    dma_channel_enable(DMA1_CHANNEL4, TRUE);
    dma_channel_enable(DMA1_CHANNEL3, TRUE);
}

/* ----------------------------------------------------------------
 * DMA1_Channel7_4_IRQHandler
 * 注意：此函数与 bsp_usart.c 中的同名函数合并！
 *       SPI RX(CH4) 和 USART TX(CH5) 共用此向量
 *       完整实现在 bsp_usart.c 中统一定义，此处不重复定义
 * ---------------------------------------------------------------- */
