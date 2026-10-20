/**
 * @file    pat9126.h
 * @brief   PAT9126JA Optical Tracking Sensor Driver
 */

#ifndef PAT9126_H
#define PAT9126_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * I2C Address
 * ============================================================ */
// #define PAT9126_I2C_ADDR            0x75u
// #define PAT9126_I2C_ADDR_WRITE     (PAT9126_I2C_ADDR << 1)
// #define PAT9126_I2C_ADDR_READ      (PAT9126_I2C_ADDR_WRITE | 0x01u)

/* ============================================================
 * Register Map
 * ============================================================ */
#define PAT9126_REG_PRODUCT_ID1     0x00u
#define PAT9126_REG_REVISION_ID     0x01u
#define PAT9126_REG_MOTION          0x02u
#define PAT9126_REG_DELTA_X_L       0x03u
#define PAT9126_REG_DELTA_Y_L       0x04u
#define PAT9126_REG_OPERATION_MODE  0x05u
#define PAT9126_REG_CONFIGURATION   0x06u
#define PAT9126_REG_WRITE_PROTECT   0x09u
#define PAT9126_REG_SLEEP1          0x0Au
#define PAT9126_REG_SLEEP2          0x0Bu
#define PAT9126_REG_RES_X           0x0Du
#define PAT9126_REG_RES_Y           0x0Eu
#define PAT9126_REG_DELTA_XY_H      0x12u
#define PAT9126_REG_SHUTTER         0x14u
#define PAT9126_REG_IMG_QUALITY     0x17u
#define PAT9126_REG_OPERATION_MODE2 0x19u
#define PAT9126_REG_BANK_SELECT     0x7Fu

/* ============================================================
 * Product ID
 * ============================================================ */
#define PAT9126_PRODUCT_ID1         0x31u
#define PAT9126_PRODUCT_ID2         0x92u

/* ============================================================
 * Register Values & Bit Definitions
 * ============================================================ */

/* REG_MOTION (0x02) */
#define PAT9126_MOTION_MOT          (1u << 7)
#define PAT9126_MOTION_OVF          (1u << 4)

/* REG_CONFIGURATION (0x06) */
#define PAT9126_CONFIG_RESET        (1u << 7)
#define PAT9126_CONFIG_PD           (1u << 3)

/* REG_WRITE_PROTECT (0x09) */
#define PAT9126_WP_DISABLE          0x5Au
#define PAT9126_WP_ENABLE           0x00u

/* REG_OPERATION_MODE (0x05) */
#define PAT9126_MODE_NORMAL         0x00u
#define PAT9126_MODE_SLEEP1         0x01u
#define PAT9126_MODE_SLEEP2         0x02u

/* Resolution */
#define PAT9126_CPI_PER_LSB         5u      /* 每 LSB = 5 CPI，需对照数据手册确认 */
#define PAT9126_RES_DEFAULT         0xC8u
#define PAT9126_RES_MAX             0xFFu
#define PAT9126_RES_SHAFT_COARSE    0x06u

/* 当前配置对应的实际 CPI（0xC8 × 5 = 1000） */
#define PAT9126_CPI                 ((uint32_t)PAT9126_RES_DEFAULT * PAT9126_CPI_PER_LSB)


/* ============================================================
 * Types
 * ============================================================ */
typedef struct {
    uint8_t res_x;
    uint8_t res_y;
    uint8_t operation_mode;
    bool    xy_swap;
    bool    x_invert;
    bool    y_invert;
} PAT9126_Config_t;

typedef struct {
    bool    motion_detected;
    bool    overflow;
    int16_t delta_x;
    int16_t delta_y;
    uint8_t image_quality;
    uint8_t shutter;
    //     /* 临时调试字段 */
    // uint8_t  raw_xl;
    // uint8_t  raw_yl;
    // uint8_t  raw_xyh;
} PAT9126_Motion_t;

typedef enum {
    PAT9126_OK          =  0,
    PAT9126_ERR_I2C     = -1,
    PAT9126_ERR_ID      = -2,
    PAT9126_ERR_TIMEOUT = -3,
    PAT9126_ERR_PARAM   = -4,
} PAT9126_Status_t;

/* ============================================================
 * Platform Abstraction — Burst 签名
 * sensor_id : 传感器编号（1 或 2）
 * reg       : 起始寄存器地址
 * data      : 数据缓冲区
 * len       : 字节数
 * ============================================================ */
extern int8_t PAT9126_I2C_Write(uint8_t sensor_id, uint8_t reg,
                                 const uint8_t *data, uint32_t len);
extern int8_t PAT9126_I2C_Read (uint8_t sensor_id, uint8_t reg,
                                 uint8_t *data, uint32_t len);
extern void   PAT9126_Delay_ms (uint32_t ms);

/* ============================================================
 * Public API
 * sensor_id 贯穿所有接口，支持双传感器独立操作
 * ============================================================ */
PAT9126_Status_t PAT9126_Init        (uint8_t sensor_id);
PAT9126_Status_t PAT9126_Init_Config (uint8_t sensor_id, const PAT9126_Config_t *cfg);
PAT9126_Status_t PAT9126_Reset       (uint8_t sensor_id);
PAT9126_Status_t PAT9126_ReadMotion  (uint8_t sensor_id, PAT9126_Motion_t *motion);
bool             PAT9126_IsMotionReady(uint8_t sensor_id);
PAT9126_Status_t PAT9126_SetResolution(uint8_t sensor_id, uint8_t res_x, uint8_t res_y);
PAT9126_Status_t PAT9126_ReadImageQuality(uint8_t sensor_id, uint8_t *quality);
PAT9126_Status_t PAT9126_Sleep       (uint8_t sensor_id, uint8_t mode);
PAT9126_Status_t PAT9126_Wakeup      (uint8_t sensor_id);

#endif /* PAT9126_H */
