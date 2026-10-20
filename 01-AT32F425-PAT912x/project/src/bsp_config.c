#include "bsp_config.h"
#include "at32f425_flash.h"
#include "bsp_usart.h"
#include <string.h>
#include <math.h>

#define USE_OTG_DEVICE_MODE    /* ← 必须有，否则 usbd_core_type 和 usbd_init 不会被编译进来 */

/* ================================================================
 * 静态断言：结构体必须是 4 字节倍数
 * ================================================================ */
_Static_assert(sizeof(device_config_t) % 4 == 0,
               "device_config_t must be 4-byte aligned");

/* ================================================================
 * 全局配置实例
 * ================================================================ */
device_config_t g_config = {0};

/* ================================================================
 * ★ 新增：调试开关 —— 打印陀螺仪校准原始采样数据
 * 用于诊断方差超限的具体原因（单次冲击 / 持续振荡 / 环境噪声）
 * 诊断完成后建议改成 0，避免正常使用时刷屏
 * ================================================================ */
#define CALIB_DEBUG_RAW_PRINT   1

/* ================================================================
 * 校准内部状态
 * ================================================================ */
static calib_state_t s_state        = CALIB_STATE_IDLE;
static uint32_t      s_sample_cnt   = 0;
static uint32_t      s_still_cnt    = 0;
static uint32_t      s_timeout_cnt  = 0;
static float         s_sum[6]       = {0};   /* [0-2]=gyro  [3-5]=acc 均值累加  */
static float         s_sum_sq[6]    = {0};   /* [0-2]=gyro  [3-5]=acc 平方累加  */
static float         s_acc_up[3]    = {0};

/* ================================================================
 * 内部常量
 * ================================================================ */
#define GRAVITY_MS2   9.80665f
#define F2I(x)        ((int32_t)((x) * 10000.0f))

/* ================================================================
 * 合法 ODR 表
 * ================================================================ */
static const uint32_t s_valid_odr[] = {50, 100, 200, 400};
#define VALID_ODR_COUNT (sizeof(s_valid_odr) / sizeof(s_valid_odr[0]))

static bool odr_is_valid(uint32_t odr)
{
    for (uint8_t i = 0; i < VALID_ODR_COUNT; i++)
        if (s_valid_odr[i] == odr) return true;
    return false;
}

/* ================================================================
 * CRC32
 * ================================================================ */
static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    while (len--) {
        crc ^= (uint32_t)(*data++) << 24;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x80000000U) ? (crc << 1) ^ 0x04C11DB7U : (crc << 1);
    }
    return crc ^ 0xFFFFFFFFU;
}

static uint32_t config_crc(const device_config_t *d)
{
    return crc32_calc((const uint8_t *)d,
                      sizeof(device_config_t) - sizeof(uint32_t));
}

/* ================================================================
 * 默认值
 * ================================================================ */
static void config_load_default(void)
{
    memset(&g_config, 0, sizeof(g_config));
    g_config.magic        = CONFIG_MAGIC;
    g_config.odr_hz       = CONFIG_DEFAULT_ODR;
    g_config.fusion_mode  = CONFIG_DEFAULT_FUSION_MODE;
    g_config.acc_scale[0] = 1.0f;
    g_config.acc_scale[1] = 1.0f;
    g_config.acc_scale[2] = 1.0f;
    strncpy(g_config.hw_version, "FY-33-425430TTL", HW_VERSION_LEN - 1);
}
/* ================================================================
 * Flash 底层操作
 * ================================================================ */
static bool flash_erase(uint32_t addr)
{
    flash_unlock();
    flash_flag_clear(FLASH_ODF_FLAG | FLASH_PRGMERR_FLAG | FLASH_EPPERR_FLAG);
    flash_status_type st = flash_sector_erase(addr);
    flash_lock();
    return (st == FLASH_OPERATE_DONE);
}

static bool flash_write(uint32_t addr, const uint32_t *data, uint32_t word_count)
{
    flash_unlock();
    flash_flag_clear(FLASH_ODF_FLAG | FLASH_PRGMERR_FLAG | FLASH_EPPERR_FLAG);
    flash_status_type st = FLASH_OPERATE_DONE;
    for (uint32_t i = 0; i < word_count && st == FLASH_OPERATE_DONE; i++)
        st = flash_word_program(addr + i * 4, data[i]);
    flash_lock();
    return (st == FLASH_OPERATE_DONE);
}


const char *config_get_hw_version(void)
{
    /* 防止Flash里存了非法字符串 */
    if (g_config.hw_version[0] == '\0' ||
        g_config.hw_version[0] == 0xFF)
        return "UNKNOWN";
    return g_config.hw_version;
}

bool config_set_hw_version(const char *ver)
{
    if (ver == NULL || strlen(ver) == 0 ||
        strlen(ver) >= HW_VERSION_LEN)
        return false;
    strncpy(g_config.hw_version, ver, HW_VERSION_LEN - 1);
    g_config.hw_version[HW_VERSION_LEN - 1] = '\0';
    return config_save();
}

/* ================================================================
 * 公开：Flash 操作
 * ================================================================ */
void config_init(void)
{
    device_config_t tmp;
    memcpy(&tmp, (const void *)CONFIG_FLASH_ADDR, sizeof(device_config_t));

    if (tmp.magic == CONFIG_MAGIC &&
        odr_is_valid(tmp.odr_hz)  &&
        config_crc(&tmp) == tmp.crc)
    {
        memcpy(&g_config, &tmp, sizeof(device_config_t));
        LOG_DEBUG("[CFG] Loaded ODR=%lu Mode=%lu\r\n",
                  g_config.odr_hz, g_config.fusion_mode);
        LOG_DEBUG("[CFG] G=[%ld,%ld,%ld] A=[%ld,%ld,%ld] S=[%ld,%ld,%ld] x1e-4\r\n",
                  F2I(g_config.gyro_offset[0]), F2I(g_config.gyro_offset[1]),
                  F2I(g_config.gyro_offset[2]),
                  F2I(g_config.acc_offset[0]),  F2I(g_config.acc_offset[1]),
                  F2I(g_config.acc_offset[2]),
                  F2I(g_config.acc_scale[0]),   F2I(g_config.acc_scale[1]),
                  F2I(g_config.acc_scale[2]));
    }
    else
    {
        config_load_default();
        LOG_DEBUG("[CFG] No valid config, using defaults\r\n");
    }
}

bool config_save(void)
{
    g_config.magic = CONFIG_MAGIC;
    g_config.crc   = config_crc(&g_config);

    if (!flash_erase(CONFIG_FLASH_ADDR)) {
        LOG_INFO("[CFG] Erase FAILED\r\n");
        return false;
    }

    uint32_t word_count = sizeof(device_config_t) / 4;
    if (!flash_write(CONFIG_FLASH_ADDR, (const uint32_t *)&g_config, word_count)) {
        LOG_INFO("[CFG] Write FAILED\r\n");
        return false;
    }

    device_config_t verify;
    memcpy(&verify, (const void *)CONFIG_FLASH_ADDR, sizeof(device_config_t));
    if (memcmp(&g_config, &verify, sizeof(device_config_t)) != 0) {
        LOG_INFO("[CFG] Verify FAILED\r\n");
        return false;
    }

    LOG_INFO("[CFG] Saved OK\r\n");
    return true;
}

void config_reset(void)
{
    config_load_default();
    config_save();
    LOG_INFO("[CFG] Reset to defaults\r\n");
}

/* ================================================================
 * 设置接口
 * ================================================================ */
uint32_t config_get_odr(void)
{
    return g_config.odr_hz;
}

bool config_set_odr(uint32_t odr_hz)
{
    if (!odr_is_valid(odr_hz))     return false;
    if (g_config.odr_hz == odr_hz) return true;
    g_config.odr_hz = odr_hz;
    return config_save();
}

uint32_t config_get_fusion_mode(void)
{
    return g_config.fusion_mode;
}

bool config_set_fusion_mode(uint32_t mode)
{
    if (mode > 1)                     return false;
    if (g_config.fusion_mode == mode) return true;
    g_config.fusion_mode = mode;
    return config_save();
}

/* ================================================================
 * 校准辅助：清零采样缓冲
 * ================================================================ */
static void _calib_clear_buf(void)
{
    memset(s_sum,    0, sizeof(s_sum));
    memset(s_sum_sq, 0, sizeof(s_sum_sq));
    s_sample_cnt = 0;
}

/* ================================================================
 * 校准辅助：方差计算
 * Var = E[x²] - E[x]²
 * ================================================================ */
static float _variance(float sum, float sum_sq, uint32_t n)
{
    float mean = sum / (float)n;
    return (sum_sq / (float)n) - (mean * mean);
}

/* ================================================================
 * 校准辅助：瞬时静止检测
 * ================================================================ */
static bool _is_gyro_still(float gx, float gy, float gz)
{
    return (fabsf(gx) < CALIB_GYRO_MOTION_LIMIT_DPS &&
            fabsf(gy) < CALIB_GYRO_MOTION_LIMIT_DPS &&
            fabsf(gz) < CALIB_GYRO_MOTION_LIMIT_DPS);
}

static bool _is_acc_level(float ax, float ay, float az)
{
    float aabs[3] = { fabsf(ax), fabsf(ay), fabsf(az) };
    uint8_t g_axis = 0;
    if (aabs[1] > aabs[g_axis]) g_axis = 1;
    if (aabs[2] > aabs[g_axis]) g_axis = 2;

    if (fabsf(aabs[g_axis] - 1.0f) > CALIB_ACC_GRAVITY_LIMIT_G) return false;
    for (uint8_t i = 0; i < 3; i++) {
        if (i == g_axis) continue;
        if (aabs[i] > CALIB_ACC_TILT_LIMIT_G) return false;
    }
    return true;
}

/* ================================================================
 * 校准辅助：采样完成后方差校验
 * 返回 false 并打印原因时，调用方直接进入 ERROR
 * ================================================================ */
static bool _gyro_var_check(void)
{
    float vx = _variance(s_sum[0], s_sum_sq[0], s_sample_cnt);
    float vy = _variance(s_sum[1], s_sum_sq[1], s_sample_cnt);
    float vz = _variance(s_sum[2], s_sum_sq[2], s_sample_cnt);
    if (vx > CALIB_GYRO_VAR_LIMIT_DPS2 ||
        vy > CALIB_GYRO_VAR_LIMIT_DPS2 ||
        vz > CALIB_GYRO_VAR_LIMIT_DPS2)
    {
        LOG_INFO("[CALIB] FAIL: gyro variance too large"
                 " [%ld,%ld,%ld] x1e-4 dps2\r\n",
                 F2I(vx), F2I(vy), F2I(vz));
        return false;
    }
    return true;
}

static bool _acc_var_check(void)
{
    float vx = _variance(s_sum[3], s_sum_sq[3], s_sample_cnt);
    float vy = _variance(s_sum[4], s_sum_sq[4], s_sample_cnt);
    float vz = _variance(s_sum[5], s_sum_sq[5], s_sample_cnt);
    if (vx > CALIB_ACC_VAR_LIMIT_G2 ||
        vy > CALIB_ACC_VAR_LIMIT_G2 ||
        vz > CALIB_ACC_VAR_LIMIT_G2)
    {
        LOG_INFO("[CALIB] FAIL: acc variance too large"
                 " [%ld,%ld,%ld] x1e-4 g2\r\n",
                 F2I(vx), F2I(vy), F2I(vz));
        return false;
    }
    return true;
}

/* ================================================================
 * 校准命令接口
 * ================================================================ */
void config_calib_start(void)
{
    if (s_state != CALIB_STATE_IDLE    &&
        s_state != CALIB_STATE_COMPLETE &&
        s_state != CALIB_STATE_ERROR)
    {
        LOG_INFO("[CALIB] Already running, state=%d\r\n", s_state);
        return;
    }
    _calib_clear_buf();
    memset(s_acc_up, 0, sizeof(s_acc_up));
    s_still_cnt   = 0;
    s_timeout_cnt = 0;
    s_state       = CALIB_STATE_GYRO_WAIT;
    LOG_INFO("[CALIB] -- Calibration Start --\r\n");
    LOG_INFO("[CALIB] Step 1/3: Settling... keep device still\r\n");
}

void config_calib_reset(void)
{
    memset(g_config.gyro_offset, 0, sizeof(g_config.gyro_offset));
    memset(g_config.acc_offset,  0, sizeof(g_config.acc_offset));
    g_config.acc_scale[0] = 1.0f;
    g_config.acc_scale[1] = 1.0f;
    g_config.acc_scale[2] = 1.0f;
    _calib_clear_buf();
    memset(s_acc_up, 0, sizeof(s_acc_up));
    s_state       = CALIB_STATE_IDLE;
    s_still_cnt   = 0;
    s_timeout_cnt = 0;
    LOG_INFO("[CALIB] Reset to defaults\r\n");
}

void config_calib_print_status(void)
{
    static const char * const state_str[] = {
        "IDLE", "GYRO_WAIT", "GYRO_RUN",
        "ACC_WAIT_UP",  "ACC_RUN_UP",
        "ACC_WAIT_DOWN","ACC_RUN_DOWN",
        "COMPLETE", "ERROR"
    };
    LOG_INFO("[CALIB] State=%s Valid=%d\r\n",
             state_str[s_state], config_calib_is_valid());
    LOG_INFO("[CALIB] G=[%ld,%ld,%ld] x1e-4 dps\r\n",
             F2I(g_config.gyro_offset[0]),
             F2I(g_config.gyro_offset[1]),
             F2I(g_config.gyro_offset[2]));
    LOG_INFO("[CALIB] A_off=[%ld,%ld,%ld] A_scl=[%ld,%ld,%ld] x1e-4\r\n",
             F2I(g_config.acc_offset[0]), F2I(g_config.acc_offset[1]),
             F2I(g_config.acc_offset[2]),
             F2I(g_config.acc_scale[0]),  F2I(g_config.acc_scale[1]),
             F2I(g_config.acc_scale[2]));
}

/* ================================================================
 * 校准主循环喂数据
 * ================================================================ */
void config_calib_process(float gx, float gy, float gz,
                           float ax, float ay, float az)
{
    switch (s_state)
    {
    /* ────────────────────────────────────────────────────────
     * Step 0: 等待陀螺仪静止
     * ──────────────────────────────────────────────────────── */
    case CALIB_STATE_GYRO_WAIT:
        if (++s_timeout_cnt > CALIB_WAIT_TIMEOUT_FRAMES) {
            s_state = CALIB_STATE_ERROR;
            LOG_INFO("[CALIB] FAIL: timeout waiting for stillness\r\n");
            LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
            return;
        }
        if (_is_gyro_still(gx, gy, gz)) {
            if (++s_still_cnt >= CALIB_STILL_FRAMES) {
                _calib_clear_buf();
                s_timeout_cnt = 0;
                s_state       = CALIB_STATE_GYRO_RUN;
                LOG_INFO("[CALIB] Stable, collecting gyro data...\r\n");
            }
        } else {
            s_still_cnt = 0;
        }
        break;

    /* ────────────────────────────────────────────────────────
     * Step 1: 陀螺仪采样
     * 瞬时超门限立即 FAIL；采满后做方差校验
     * ──────────────────────────────────────────────────────── */
    case CALIB_STATE_GYRO_RUN:
        if (!_is_gyro_still(gx, gy, gz)) {
            _calib_clear_buf();
            s_state = CALIB_STATE_ERROR;
            LOG_INFO("[CALIB] FAIL: motion during gyro (>%.0fdps)\r\n",
                     CALIB_GYRO_MOTION_LIMIT_DPS);
            LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
            return;
        }
        s_sum[0]    += gx;   s_sum_sq[0] += gx * gx;
        s_sum[1]    += gy;   s_sum_sq[1] += gy * gy;
        s_sum[2]    += gz;   s_sum_sq[2] += gz * gz;

#if CALIB_DEBUG_RAW_PRINT
        /* ★ 新增：逐帧打印原始陀螺仪数据，用于诊断方差超限原因
         * 格式：序号, gx, gy, gz（均为 x1e-4 dps 定点整数） */
        LOG_INFO("[RAW] #%lu G=%ld,%ld,%ld\r\n",
                 (unsigned long)(s_sample_cnt + 1),
                 F2I(gx), F2I(gy), F2I(gz));
#endif

        if (++s_sample_cnt >= CALIB_SAMPLE_COUNT) {
            if (!_gyro_var_check()) {
                _calib_clear_buf();
                s_state = CALIB_STATE_ERROR;
                LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
                return;
            }
            g_config.gyro_offset[0] = s_sum[0] / CALIB_SAMPLE_COUNT;
            g_config.gyro_offset[1] = s_sum[1] / CALIB_SAMPLE_COUNT;
            g_config.gyro_offset[2] = s_sum[2] / CALIB_SAMPLE_COUNT;
            _calib_clear_buf();
            s_still_cnt   = 0;
            s_timeout_cnt = 0;
            s_state       = CALIB_STATE_ACC_WAIT_UP;
            LOG_INFO("[CALIB] Gyro done: [%ld,%ld,%ld] x1e-4 dps\r\n",
                     F2I(g_config.gyro_offset[0]),
                     F2I(g_config.gyro_offset[1]),
                     F2I(g_config.gyro_offset[2]));
            LOG_INFO("[CALIB] Step 2/3: Place Z-axis UP, hold still...\r\n");
        }
        break;

    /* ────────────────────────────────────────────────────────
     * Step 2: 等待 Z+ 放稳
     * ──────────────────────────────────────────────────────── */
    case CALIB_STATE_ACC_WAIT_UP:
        if (++s_timeout_cnt > CALIB_WAIT_TIMEOUT_FRAMES) {
            s_state = CALIB_STATE_ERROR;
            LOG_INFO("[CALIB] FAIL: timeout waiting for Z+ placement\r\n");
            LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
            return;
        }
        if (_is_acc_level(ax, ay, az)) {
            if (++s_still_cnt >= CALIB_STILL_FRAMES) {
                _calib_clear_buf();
                s_timeout_cnt = 0;
                s_state       = CALIB_STATE_ACC_RUN_UP;
                LOG_INFO("[CALIB] Z+ stable, collecting...\r\n");
            }
        } else {
            s_still_cnt = 0;
        }
        break;

    /* ────────────────────────────────────────────────────────
     * Step 3: Z+ 采样
     * ──────────────────────────────────────────────────────── */
    case CALIB_STATE_ACC_RUN_UP:
        if (!_is_acc_level(ax, ay, az)) {
            _calib_clear_buf();
            s_state = CALIB_STATE_ERROR;
            LOG_INFO("[CALIB] FAIL: device moved during Z+ collection\r\n");
            LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
            return;
        }
        s_sum[3]    += ax;   s_sum_sq[3] += ax * ax;
        s_sum[4]    += ay;   s_sum_sq[4] += ay * ay;
        s_sum[5]    += az;   s_sum_sq[5] += az * az;
        if (++s_sample_cnt >= CALIB_SAMPLE_COUNT) {
            if (!_acc_var_check()) {
                _calib_clear_buf();
                s_state = CALIB_STATE_ERROR;
                LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
                return;
            }
            s_acc_up[0]   = s_sum[3] / CALIB_SAMPLE_COUNT;
            s_acc_up[1]   = s_sum[4] / CALIB_SAMPLE_COUNT;
            s_acc_up[2]   = s_sum[5] / CALIB_SAMPLE_COUNT;
            _calib_clear_buf();
            s_still_cnt   = 0;
            s_timeout_cnt = 0;
            s_state       = CALIB_STATE_ACC_WAIT_DOWN;
            LOG_INFO("[CALIB] Z+ done.\r\n");
            LOG_INFO("[CALIB] Step 3/3: Flip to Z-axis DOWN, hold still...\r\n");
        }
        break;

    /* ────────────────────────────────────────────────────────
     * Step 4: 等待 Z- 放稳
     * ──────────────────────────────────────────────────────── */
    case CALIB_STATE_ACC_WAIT_DOWN:
        if (++s_timeout_cnt > CALIB_WAIT_TIMEOUT_FRAMES) {
            s_state = CALIB_STATE_ERROR;
            LOG_INFO("[CALIB] FAIL: timeout waiting for Z- placement\r\n");
            LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
            return;
        }
        if (_is_acc_level(ax, ay, az)) {
            if (++s_still_cnt >= CALIB_STILL_FRAMES) {
                _calib_clear_buf();
                s_timeout_cnt = 0;
                s_state       = CALIB_STATE_ACC_RUN_DOWN;
                LOG_INFO("[CALIB] Z- stable, collecting...\r\n");
            }
        } else {
            s_still_cnt = 0;
        }
        break;

    /* ────────────────────────────────────────────────────────
     * Step 5: Z- 采样 → 方差校验 → 计算 offset/scale → 自动保存
     * ──────────────────────────────────────────────────────── */
    case CALIB_STATE_ACC_RUN_DOWN:
        if (!_is_acc_level(ax, ay, az)) {
            _calib_clear_buf();
            s_state = CALIB_STATE_ERROR;
            LOG_INFO("[CALIB] FAIL: device moved during Z- collection\r\n");
            LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
            return;
        }
        s_sum[3]    += ax;   s_sum_sq[3] += ax * ax;
        s_sum[4]    += ay;   s_sum_sq[4] += ay * ay;
        s_sum[5]    += az;   s_sum_sq[5] += az * az;
        if (++s_sample_cnt >= CALIB_SAMPLE_COUNT) {
            if (!_acc_var_check()) {
                _calib_clear_buf();
                s_state = CALIB_STATE_ERROR;
                LOG_INFO("[CALIB] Send 'calib start' to retry\r\n");
                return;
            }
            float mean_ax = s_sum[3] / CALIB_SAMPLE_COUNT;
            float mean_ay = s_sum[4] / CALIB_SAMPLE_COUNT;
            float mean_az = s_sum[5] / CALIB_SAMPLE_COUNT;

            float diff_z = s_acc_up[2] - mean_az;
            if (fabsf(diff_z) > 0.1f) {
                g_config.acc_scale[2]  = 2.0f / diff_z;
                g_config.acc_offset[2] = (s_acc_up[2] + mean_az) / 2.0f;
            }
            g_config.acc_offset[0] = (s_acc_up[0] + mean_ax) / 2.0f;
            g_config.acc_offset[1] = (s_acc_up[1] + mean_ay) / 2.0f;
            g_config.acc_scale[0]  = 1.0f;
            g_config.acc_scale[1]  = 1.0f;

            _calib_clear_buf();
            s_state = CALIB_STATE_COMPLETE;
            LOG_INFO("[CALIB] Acc done.\r\n");

            if (config_save()) {
                LOG_INFO("[CALIB] -- All done! --\r\n");
            } else {
                s_state = CALIB_STATE_ERROR;
                LOG_INFO("[CALIB] FAIL: Flash save error\r\n");
            }
        }
        break;

    default:
        break;
    }
}

/* ================================================================
 * 校准应用
 * ================================================================ */
void config_apply_gyro(float *gx, float *gy, float *gz)
{
    *gx -= g_config.gyro_offset[0];
    *gy -= g_config.gyro_offset[1];
    *gz -= g_config.gyro_offset[2];
}

void config_apply_acc(float *ax, float *ay, float *az)
{
    *ax = (*ax - g_config.acc_offset[0]) * g_config.acc_scale[0];
    *ay = (*ay - g_config.acc_offset[1]) * g_config.acc_scale[1];
    *az = (*az - g_config.acc_offset[2]) * g_config.acc_scale[2];
}

/* ================================================================
 * 校准状态查询
 * ================================================================ */
calib_state_t config_calib_get_state(void)
{
    return s_state;
}

bool config_calib_is_valid(void)
{
    return (g_config.magic == CONFIG_MAGIC &&
            config_crc(&g_config) == g_config.crc);
}

bool config_calib_is_active(void)
{
    return (s_state != CALIB_STATE_IDLE    &&
            s_state != CALIB_STATE_COMPLETE &&
            s_state != CALIB_STATE_ERROR);
}
