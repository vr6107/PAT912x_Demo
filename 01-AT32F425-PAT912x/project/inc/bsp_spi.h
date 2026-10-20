#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include "at32f425.h"

/* ----------------------------------------------------------------
 * 硬件引脚定义
 * ---------------------------------------------------------------- */
#define BMI_SPI                  SPI1
#define BMI_SPI_CLK              CRM_SPI1_PERIPH_CLOCK
#define BMI_SPI_GPIO_CLK         CRM_GPIOA_PERIPH_CLOCK
#define BMI_SPI_GPIO             GPIOA
#define BMI_SCK_PIN              GPIO_PINS_5
#define BMI_MISO_PIN             GPIO_PINS_6
#define BMI_MOSI_PIN             GPIO_PINS_7

#define BMI_CS_GPIO_CLK          CRM_GPIOB_PERIPH_CLOCK
#define BMI_CS_GPIO              GPIOB
#define BMI_CS_PIN               GPIO_PINS_2

#define BMI_CS_LOW()             gpio_bits_reset(BMI_CS_GPIO, BMI_CS_PIN)
#define BMI_CS_HIGH()            gpio_bits_set(BMI_CS_GPIO,   BMI_CS_PIN)

/* ----------------------------------------------------------------
 * DMA 缓冲区大小
 *
 * BMI430 最大单次读取：
 *   ACC(12) + GYR(12) + TEMP(2) + SENSORTIME(3) = 29 字节数据
 *   + 1(地址) + 2(BMI430 dummy) = 32 字节 → 刚好够，建议扩到 40
 * ---------------------------------------------------------------- */
#define SPI_DMA_BUF_SIZE         40    // ← 从 32 扩大到 40，留余量

/* ----------------------------------------------------------------
 * dummy 字节偏移定义（集中管理，避免硬编码）
 *
 *  BMI32x 读时序：[addr] [dummy×1] [data...]
 *                  [0]    [1]       [2..]      → 数据从 index 2 开始
 *
 *  BMI430 读时序：[addr] [dummy×2] [data...]
 *                  [0]    [1][2]    [3..]      → 数据从 index 3 开始
 * ---------------------------------------------------------------- */
#define SPI_RX_DATA_OFFSET_BMI32X    2U   /* BMI32x: 1地址 + 1dummy */
#define SPI_RX_DATA_OFFSET_BMI430    3U   /* BMI430: 1地址 + 2dummy */

typedef enum {
    SPI_OP_READ  = 0,
    SPI_OP_WRITE = 1
} spi_op_type_t;

extern volatile spi_op_type_t spi_current_op;
extern uint8_t  bmi_spi_tx_buf[SPI_DMA_BUF_SIZE];
extern uint8_t  bmi_spi_rx_buf[SPI_DMA_BUF_SIZE];
extern volatile uint8_t spi_dma_done;

/* ----------------------------------------------------------------
 * 函数声明
 * ---------------------------------------------------------------- */
void bsp_spi_dma_init(void);

/* BMI32x：1个协议dummy */
void bmi32x_read_regs_dma(uint8_t reg, uint16_t actual_data_len);
void bmi32x_write_regs_dma(uint8_t reg, uint8_t *data, uint16_t len);

/* BMI430：2个协议dummy */
void bmi430_read_regs_dma(uint8_t reg, uint16_t actual_data_len);
void bmi430_write_regs_dma(uint8_t reg, uint8_t *data, uint16_t len);

#endif
