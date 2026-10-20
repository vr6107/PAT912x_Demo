/**
 * @file    pat9126.c
 * @brief   PAT9126JA Optical Tracking Sensor Driver
 *          All I2C calls use burst signature with sensor_id.
 */

#include "pat9126.h"
#include <stddef.h>

/* ============================================================
 * Internal helpers
 * ============================================================ */

static inline PAT9126_Status_t _rd(uint8_t id, uint8_t reg, uint8_t *v)
{
    return PAT9126_I2C_Read(id, reg, v, 1u) == 0 ? PAT9126_OK : PAT9126_ERR_I2C;
}

static inline PAT9126_Status_t _wr(uint8_t id, uint8_t reg, uint8_t v)
{
    return PAT9126_I2C_Write(id, reg, &v, 1u) == 0 ? PAT9126_OK : PAT9126_ERR_I2C;
}

static inline PAT9126_Status_t _rd_b(uint8_t id, uint8_t reg,
                                      uint8_t *buf, uint32_t len)
{
    return PAT9126_I2C_Read(id, reg, buf, len) == 0 ? PAT9126_OK : PAT9126_ERR_I2C;
}

static inline PAT9126_Status_t _wr_b(uint8_t id, uint8_t reg,
                                      const uint8_t *buf, uint32_t len)
{
    return PAT9126_I2C_Write(id, reg, buf, len) == 0 ? PAT9126_OK : PAT9126_ERR_I2C;
}

/* Write-protect wrapper — single byte only */
static PAT9126_Status_t _wp_write(uint8_t id, uint8_t reg, uint8_t val)
{
    PAT9126_Status_t ret;

    ret = _wr(id, PAT9126_REG_WRITE_PROTECT, PAT9126_WP_DISABLE);
    if (ret != PAT9126_OK) return ret;

    ret = _wr(id, reg, val);
    if (ret != PAT9126_OK) return ret;

    return _wr(id, PAT9126_REG_WRITE_PROTECT, PAT9126_WP_ENABLE);
}

/* ============================================================
 * PAT9126_Reset
 * ============================================================ */
PAT9126_Status_t PAT9126_Reset(uint8_t sensor_id)
{
    uint8_t          val   = 0u;
    uint8_t          retry = 0u;
    PAT9126_Status_t ret;

    /* 1. Disable write protection */
    ret = _wr(sensor_id, PAT9126_REG_WRITE_PROTECT, PAT9126_WP_DISABLE);
    if (ret != PAT9126_OK) return ret;

    /* 2. Trigger reset — PAT9126 will NACK this, do NOT check return value */
    _wr(sensor_id, PAT9126_REG_CONFIGURATION, PAT9126_CONFIG_RESET);

    /* 3. Poll until reset bit self-clears, 1ms x 100 = 100ms max */
    for (retry = 0u; retry < 100u; retry++) {
        PAT9126_Delay_ms(1u);
        if (_rd(sensor_id, PAT9126_REG_CONFIGURATION, &val) == PAT9126_OK) {
            if ((val & PAT9126_CONFIG_RESET) == 0u) {
                PAT9126_Delay_ms(2u);   /* stabilization after reset */
                return PAT9126_OK;
            }
        }
    }

    return PAT9126_ERR_TIMEOUT;
}

/* ============================================================
 * PAT9126_Init
 * ============================================================ */
PAT9126_Status_t PAT9126_Init(uint8_t sensor_id)
{
    PAT9126_Config_t default_cfg = {
        .res_x          = PAT9126_RES_DEFAULT,
        .res_y          = PAT9126_RES_DEFAULT,
        .operation_mode = PAT9126_MODE_NORMAL,
        .xy_swap        = false,
        .x_invert       = false,
        .y_invert       = false,
    };
    return PAT9126_Init_Config(sensor_id, &default_cfg);
}

/* ============================================================
 * PAT9126_Init_Config
 * ============================================================ */
PAT9126_Status_t PAT9126_Init_Config(uint8_t sensor_id,
                                      const PAT9126_Config_t *cfg)
{
    PAT9126_Status_t ret;
    uint8_t          id[2]  = {0u, 0u};
    uint8_t          orient = 0u;

    if (cfg == NULL) return PAT9126_ERR_PARAM;

    /* 1. Power-on stabilization */
    PAT9126_Delay_ms(10u);

    /* 2. Software reset */
    ret = PAT9126_Reset(sensor_id);
    if (ret != PAT9126_OK) return ret;

    /* 3. Verify product ID */
    ret = _rd(sensor_id, PAT9126_REG_PRODUCT_ID1, &id[0]);
    if (ret != PAT9126_OK) return ret;

    ret = _rd(sensor_id, PAT9126_REG_REVISION_ID, &id[1]);
    if (ret != PAT9126_OK) return ret;

    if (id[0] != PAT9126_PRODUCT_ID1 || id[1] != PAT9126_PRODUCT_ID2)
        return PAT9126_ERR_ID;

    /* 4. Disable write protection for bulk config */
    ret = _wr(sensor_id, PAT9126_REG_WRITE_PROTECT, PAT9126_WP_DISABLE);
    if (ret != PAT9126_OK) return ret;

    /* 5. Resolution — burst write RES_X(0x0D) + RES_Y(0x0E) */
    {
        uint8_t res[2] = { cfg->res_x, cfg->res_y };
        ret = _wr_b(sensor_id, PAT9126_REG_RES_X, res, 2u);
        if (ret != PAT9126_OK) return ret;
    }

    /* 6. Operation mode */
    ret = _wr(sensor_id, PAT9126_REG_OPERATION_MODE, cfg->operation_mode);
    if (ret != PAT9126_OK) return ret;

    /* 7. Orientation (OPERATION_MODE2)
     *    Bit[2] = XY_SWAP, Bit[1] = Y_INV, Bit[0] = X_INV */
    if (cfg->xy_swap)  orient |= (1u << 2);
    if (cfg->y_invert) orient |= (1u << 1);
    if (cfg->x_invert) orient |= (1u << 0);

    ret = _wr(sensor_id, PAT9126_REG_OPERATION_MODE2, orient);
    if (ret != PAT9126_OK) return ret;

    /* 8. Re-enable write protection */
    ret = _wr(sensor_id, PAT9126_REG_WRITE_PROTECT, PAT9126_WP_ENABLE);
    if (ret != PAT9126_OK) return ret;

    PAT9126_Delay_ms(1u);

    return PAT9126_OK;
}

/* ============================================================
 * PAT9126_ReadMotion
 *
 * REG_DELTA_XY_H (0x12) layout:
 *   Bit[7:4] = Delta_Y [11:8]
 *   Bit[3:0] = Delta_X [11:8]
 * ============================================================ */
PAT9126_Status_t PAT9126_ReadMotion(uint8_t sensor_id,
                                     PAT9126_Motion_t *motion)
{
    PAT9126_Status_t ret;
    uint8_t  mot     = 0u;
    uint8_t  xy[2]   = {0u, 0u};  /* [0]=Delta_X_L  [1]=Delta_Y_L */
    uint8_t  dxy_h   = 0u;
    uint8_t  qual    = 0u;
    uint8_t  shutter = 0u;
    int16_t  raw_x, raw_y;

    if (motion == NULL) return PAT9126_ERR_PARAM;

    /* 1. Read MOTION — latches delta registers */
    ret = _rd(sensor_id, PAT9126_REG_MOTION, &mot);
    if (ret != PAT9126_OK) return ret;

    motion->motion_detected = (mot & PAT9126_MOTION_MOT) != 0u;
    motion->overflow        = (mot & PAT9126_MOTION_OVF) != 0u;

    /* No motion: clear deltas and return early — avoids stale register reads */
    if (!motion->motion_detected) {
        motion->delta_x       = 0;
        motion->delta_y       = 0;
        motion->image_quality = 0;
        motion->shutter       = 0;
        return PAT9126_OK;
    }

    /* 2. Burst read Delta_X_L(0x03) + Delta_Y_L(0x04) */
    ret = _rd_b(sensor_id, PAT9126_REG_DELTA_X_L, xy, 2u);
    if (ret != PAT9126_OK) return ret;

    /* 3. Delta XY high bits (0x12) */
    ret = _rd(sensor_id, PAT9126_REG_DELTA_XY_H, &dxy_h);
    if (ret != PAT9126_OK) return ret;

    // X: 高nibble (bit[7:4]) 左移4位到 bit[11:8]
    raw_x = (int16_t)(((uint16_t)(dxy_h & 0xF0u) << 4) | (uint16_t)xy[0]);

    // Y: 低nibble (bit[3:0]) 左移8位到 bit[11:8]  
    raw_y = (int16_t)(((uint16_t)(dxy_h & 0x0Fu) << 8) | (uint16_t)xy[1]);

    // 符号扩展：12bit → 16bit
    if (raw_x & 0x0800) raw_x -= 4096;   // 和Arduino一致，比 |= 0xF000 更直观
    if (raw_y & 0x0800) raw_y -= 4096;

    motion->delta_x = raw_x;   // ← 加这两行
    motion->delta_y = raw_y;

    // motion->raw_xl  = xy[0];
    // motion->raw_yl  = xy[1];
    // motion->raw_xyh = dxy_h;

    /* 4. Image quality */
    ret = _rd(sensor_id, PAT9126_REG_IMG_QUALITY, &qual);
    if (ret != PAT9126_OK) return ret;
    motion->image_quality = qual;

    /* 5. Shutter */
    ret = _rd(sensor_id, PAT9126_REG_SHUTTER, &shutter);
    if (ret != PAT9126_OK) return ret;
    motion->shutter = shutter;

    return PAT9126_OK;
}

/* ============================================================
 * PAT9126_IsMotionReady
 * ============================================================ */
bool PAT9126_IsMotionReady(uint8_t sensor_id)
{
    uint8_t val = 0u;
    _rd(sensor_id, PAT9126_REG_MOTION, &val);
    return (val & PAT9126_MOTION_MOT) != 0u;
}

/* ============================================================
 * PAT9126_SetResolution
 * _wp_write supports single byte only; RES_X/Y need burst write,
 * so write-protect is managed manually here.
 * ============================================================ */
PAT9126_Status_t PAT9126_SetResolution(uint8_t sensor_id,
                                        uint8_t res_x, uint8_t res_y)
{
    PAT9126_Status_t ret;
    uint8_t res[2] = { res_x, res_y };

    ret = _wr(sensor_id, PAT9126_REG_WRITE_PROTECT, PAT9126_WP_DISABLE);
    if (ret != PAT9126_OK) return ret;

    ret = _wr_b(sensor_id, PAT9126_REG_RES_X, res, 2u);
    if (ret != PAT9126_OK) return ret;

    return _wr(sensor_id, PAT9126_REG_WRITE_PROTECT, PAT9126_WP_ENABLE);
}

/* ============================================================
 * PAT9126_ReadImageQuality
 * ============================================================ */
PAT9126_Status_t PAT9126_ReadImageQuality(uint8_t sensor_id,
                                           uint8_t *quality)
{
    if (quality == NULL) return PAT9126_ERR_PARAM;
    return _rd(sensor_id, PAT9126_REG_IMG_QUALITY, quality);
}

/* ============================================================
 * PAT9126_Sleep / Wakeup
 * ============================================================ */
PAT9126_Status_t PAT9126_Sleep(uint8_t sensor_id, uint8_t mode)
{
    if (mode != PAT9126_MODE_SLEEP1 && mode != PAT9126_MODE_SLEEP2)
        return PAT9126_ERR_PARAM;
    return _wp_write(sensor_id, PAT9126_REG_OPERATION_MODE, mode);
}

PAT9126_Status_t PAT9126_Wakeup(uint8_t sensor_id)
{
    return _wp_write(sensor_id, PAT9126_REG_OPERATION_MODE,
                     PAT9126_MODE_NORMAL);
}
