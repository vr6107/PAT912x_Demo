#ifndef __BSP_I2C_H
#define __BSP_I2C_H

#include "at32f425.h"
#include "at32f425_board.h"

#define PAT9126_I2C_CLKCTRL     0x20F03333u
#define PAT9126_I2C_TIMEOUT     100000u

/* 唯一一路：I2C2  SCL→PA0  SDA→PA1 */
#define PAT9126_I2C             I2C1
#define PAT9126_I2C_CLK         CRM_I2C1_PERIPH_CLOCK
#define PAT9126_I2C_GPIO_CLK    CRM_GPIOB_PERIPH_CLOCK
#define PAT9126_I2C_GPIO        GPIOB
#define PAT9126_I2C_SCL_PIN     GPIO_PINS_6
#define PAT9126_I2C_SDA_PIN     GPIO_PINS_7
#define PAT9126_I2C_SCL_SOURCE  GPIO_PINS_SOURCE6
#define PAT9126_I2C_SDA_SOURCE  GPIO_PINS_SOURCE7
#define PAT9126_I2C_GPIO_MUX    GPIO_MUX_1      /* ⚠ 请对照数据手册核对 */

#define I2C_SENSOR_1            1u

/* 7-bit = 0x75（ID_SEL GND）；GND=0x74，VDD=0x76 */
#define PAT9126_I2C_ADDR        0x75u
#define PAT9126_I2C_ADDR_WRITE  (PAT9126_I2C_ADDR << 1)
#define PAT9126_I2C_ADDR_READ   ((PAT9126_I2C_ADDR << 1) | 0x01u)

/* 最近一次 I2C 错误码：0=正常，高4位=步骤，低4位=1超时/2无应答 */
extern volatile uint8_t g_i2c_last_err;

void    bsp_i2c_init(void);
uint8_t pat9126_burst_write(uint8_t sensor_id, uint8_t reg,
                            const uint8_t *data, uint8_t len);
uint8_t pat9126_burst_read (uint8_t sensor_id, uint8_t reg,
                            uint8_t *data, uint8_t len);

int8_t PAT9126_I2C_Write(uint8_t sensor_id, uint8_t reg,
                         const uint8_t *data, uint32_t len);
int8_t PAT9126_I2C_Read (uint8_t sensor_id, uint8_t reg,
                         uint8_t *data, uint32_t len);
void   PAT9126_Delay_ms(uint32_t ms);

#endif
