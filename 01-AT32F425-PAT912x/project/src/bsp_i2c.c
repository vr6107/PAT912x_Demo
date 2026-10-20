#include "bsp_i2c.h"
#include "at32f425_board.h"

volatile uint8_t g_i2c_last_err = 0u;

/* 等 flag 到期望值；超时返回 1 */
static uint8_t _wait(i2c_type *i2cx, uint32_t flag,
                     flag_status expect, uint32_t timeout)
{
    while (i2c_flag_get(i2cx, flag) != expect) {
        if (timeout-- == 0u) return 1u;
    }
    return 0u;
}

/* 等 flag 置位，期间出现 ACKFAIL 立即返回。0=成功 1=超时 2=无应答 */
static uint8_t _wait_set(i2c_type *i2cx, uint32_t flag)
{
    uint32_t t = PAT9126_I2C_TIMEOUT;
    while (i2c_flag_get(i2cx, flag) != SET) {
        if (i2c_flag_get(i2cx, I2C_ACKFAIL_FLAG) == SET) return 2u;
        if (t-- == 0u) return 1u;
    }
    return 0u;
}

/* 统一失败处理：释放总线、清标志、记录错误码 */
static uint8_t _fail(i2c_type *i2cx, uint8_t step, uint8_t kind)
{
    if (i2c_flag_get(i2cx, I2C_BUSYF_FLAG) == SET &&
        i2c_flag_get(i2cx, I2C_STOPF_FLAG) == RESET)
    {
        i2c_stop_generate(i2cx);
        _wait(i2cx, I2C_STOPF_FLAG, SET, PAT9126_I2C_TIMEOUT);
    }
    i2c_flag_clear(i2cx, I2C_ACKFAIL_FLAG);
    i2c_flag_clear(i2cx, I2C_STOPF_FLAG);

    if (i2c_flag_get(i2cx, I2C_BUSYF_FLAG) == SET) {   /* 仍卡住：重启外设 */
        i2c_enable(i2cx, FALSE);
        i2c_enable(i2cx, TRUE);
    }
    g_i2c_last_err = (uint8_t)((step << 4) | kind);
    return g_i2c_last_err;
}

/* 每次传输前：等空闲并清掉残留标志 */
static uint8_t _begin(i2c_type *i2cx)
{
    if (_wait(i2cx, I2C_BUSYF_FLAG, RESET, PAT9126_I2C_TIMEOUT))
        return _fail(i2cx, 0u, 1u);
    i2c_flag_clear(i2cx, I2C_ACKFAIL_FLAG);
    i2c_flag_clear(i2cx, I2C_STOPF_FLAG);
    return 0u;
}

void bsp_i2c_init(void)
{
    gpio_init_type g;

    crm_periph_clock_enable(PAT9126_I2C_GPIO_CLK, TRUE);
    crm_periph_clock_enable(PAT9126_I2C_CLK,      TRUE);

    gpio_pin_mux_config(PAT9126_I2C_GPIO, PAT9126_I2C_SCL_SOURCE, PAT9126_I2C_GPIO_MUX);
    gpio_pin_mux_config(PAT9126_I2C_GPIO, PAT9126_I2C_SDA_SOURCE, PAT9126_I2C_GPIO_MUX);

    gpio_default_para_init(&g);
    g.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    g.gpio_mode           = GPIO_MODE_MUX;
    g.gpio_out_type       = GPIO_OUTPUT_OPEN_DRAIN;
    g.gpio_pull           = GPIO_PULL_UP;   /* 辅助上拉，外部 4.7k 仍然需要 */
    g.gpio_pins           = PAT9126_I2C_SCL_PIN | PAT9126_I2C_SDA_PIN;
    gpio_init(PAT9126_I2C_GPIO, &g);

    i2c_reset(PAT9126_I2C);
    i2c_init(PAT9126_I2C, 0x0Fu, PAT9126_I2C_CLKCTRL);
    i2c_own_address1_set(PAT9126_I2C, I2C_ADDRESS_MODE_7BIT, 0x00u);
    i2c_enable(PAT9126_I2C, TRUE);
}

/* START+ADDR+W → REG → DATA... → STOP */
uint8_t pat9126_burst_write(uint8_t sensor_id, uint8_t reg,
                            const uint8_t *data, uint8_t len)
{
    i2c_type *i2cx = PAT9126_I2C;
    uint16_t  total = (uint16_t)len + 1u;
    uint8_t   i, k;
    (void)sensor_id;

    if (total > 255u) return 0xF1u;
    if (_begin(i2cx)) return g_i2c_last_err;

    i2c_transmit_set(i2cx, PAT9126_I2C_ADDR_WRITE, (uint8_t)total,
                     I2C_AUTO_STOP_MODE, I2C_GEN_START_WRITE);

    k = _wait_set(i2cx, I2C_TDIS_FLAG);
    if (k) return _fail(i2cx, 1u, k);          /* 1x：地址阶段 */
    i2c_data_send(i2cx, reg);

    for (i = 0u; i < len; i++) {
        k = _wait_set(i2cx, I2C_TDIS_FLAG);
        if (k) return _fail(i2cx, 2u, k);      /* 2x：数据阶段 */
        i2c_data_send(i2cx, data[i]);
    }

    k = _wait_set(i2cx, I2C_STOPF_FLAG);
    if (k == 0u && i2c_flag_get(i2cx, I2C_ACKFAIL_FLAG) == SET) k = 2u;
    if (k) return _fail(i2cx, 3u, k);          /* 3x：等 STOP */

    i2c_flag_clear(i2cx, I2C_STOPF_FLAG);
    g_i2c_last_err = 0u;
    return 0u;
}

/* START+ADDR+W → REG → RESTART+ADDR+R → DATA... → STOP */
uint8_t pat9126_burst_read(uint8_t sensor_id, uint8_t reg,
                           uint8_t *data, uint8_t len)
{
    i2c_type *i2cx = PAT9126_I2C;
    uint8_t   i, k;
    (void)sensor_id;

    if (len == 0u) return 0xF1u;
    if (_begin(i2cx)) return g_i2c_last_err;

    i2c_transmit_set(i2cx, PAT9126_I2C_ADDR_WRITE, 1u,
                     I2C_SOFT_STOP_MODE, I2C_GEN_START_WRITE);

    k = _wait_set(i2cx, I2C_TDIS_FLAG);
    if (k) return _fail(i2cx, 4u, k);         /* 4x：写寄存器地址 */
    i2c_data_send(i2cx, reg);

    k = _wait_set(i2cx, I2C_TDC_FLAG);
    if (k) return _fail(i2cx, 5u, k);         /* 5x：等发送完成 */

    i2c_transmit_set(i2cx, PAT9126_I2C_ADDR_READ, len,
                     I2C_AUTO_STOP_MODE, I2C_GEN_START_READ);

    for (i = 0u; i < len; i++) {
        k = _wait_set(i2cx, I2C_RDBF_FLAG);
        if (k) return _fail(i2cx, 6u, k);     /* 6x：读数据 */
        data[i] = i2c_data_receive(i2cx);
    }

    k = _wait_set(i2cx, I2C_STOPF_FLAG);
    if (k) return _fail(i2cx, 7u, k);         /* 7x：等 STOP */

    i2c_flag_clear(i2cx, I2C_STOPF_FLAG);
    g_i2c_last_err = 0u;
    return 0u;
}

int8_t PAT9126_I2C_Write(uint8_t sensor_id, uint8_t reg,
                         const uint8_t *data, uint32_t len)
{
    if (len > 254u) return -1;
    return (pat9126_burst_write(sensor_id, reg, data, (uint8_t)len) == 0u) ? 0 : -1;
}

int8_t PAT9126_I2C_Read(uint8_t sensor_id, uint8_t reg,
                        uint8_t *data, uint32_t len)
{
    if (len > 255u) return -1;
    return (pat9126_burst_read(sensor_id, reg, data, (uint8_t)len) == 0u) ? 0 : -1;
}

void PAT9126_Delay_ms(uint32_t ms) { delay_ms(ms); }
