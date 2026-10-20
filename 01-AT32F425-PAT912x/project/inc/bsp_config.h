#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * Flash 地址 & Magic
 * ================================================================ */
#define CONFIG_FLASH_ADDR           0x0800FC00U
#define CONFIG_MAGIC                0xA5C3E1F0U

/* ================================================================
 * 默认值
 * ================================================================ */
#define CONFIG_DEFAULT_ODR          100U
#define CONFIG_DEFAULT_FUSION_MODE    0U   /* 0=ACC+LPF  1=VQF */

/* ================================================================
 * 校准参数
 * ================================================================ */
#define CALIB_SAMPLE_COUNT          200U    /* 采样帧数，100Hz下=2s      */
#define CALIB_STILL_FRAMES           50U    /* 连续静止帧，100Hz下=0.5s  */
#define CALIB_WAIT_TIMEOUT_FRAMES   500U    /* 等待超时帧，100Hz下=5s    */

/* 瞬时值门限 */
#define CALIB_GYRO_MOTION_LIMIT_DPS  10.0f  /* 陀螺仪运动门限 dps        */
#define CALIB_ACC_TILT_LIMIT_G        0.1f  /* 加速度计非重力轴门限 g    */
#define CALIB_ACC_GRAVITY_LIMIT_G     0.1f  /* 重力轴偏离1g门限 g        */

/* 方差门限 */
#define CALIB_GYRO_VAR_LIMIT_DPS2    0.05f  /* 陀螺仪方差门限 dps²       */
#define CALIB_ACC_VAR_LIMIT_G2      0.0001f /* 加速度计方差门限 g²        */

/* ── 软件版本（跟代码走，编译时确定）─────────────────────────── */
#define SW_VERSION_MAJOR    1
#define SW_VERSION_MINOR    0
#define SW_VERSION_PATCH    0
#define SW_VERSION          "V1.0.0"

/* ── 硬件版本字符串最大长度 ──────────────────────────────────── */
#define HW_VERSION_LEN      32


/* ================================================================
 * 校准状态机
 * ================================================================ */
typedef enum {
    CALIB_STATE_IDLE = 0,
    CALIB_STATE_GYRO_WAIT,     /* ★ 新增：等待静止，吸收敲键盘等振动干扰 */
    CALIB_STATE_GYRO_RUN,
    CALIB_STATE_ACC_WAIT_UP,
    CALIB_STATE_ACC_RUN_UP,
    CALIB_STATE_ACC_WAIT_DOWN,
    CALIB_STATE_ACC_RUN_DOWN,
    CALIB_STATE_COMPLETE,
    CALIB_STATE_ERROR,
} calib_state_t;

/* ================================================================
 * 持久化配置结构体
 * ================================================================ */
typedef struct {
    uint32_t magic;
    uint32_t odr_hz;
    uint32_t fusion_mode;

    float gyro_offset[3];
    float acc_offset[3];
    float acc_scale[3];

    char hw_version[HW_VERSION_LEN];   /* ← 新增，存硬件版本 */

    uint32_t crc;   /* 必须保持最后 */
} device_config_t;


/* ================================================================
 * 全局配置实例
 * ================================================================ */
extern device_config_t g_config;

/* ================================================================
 * Flash 配置接口
 * ================================================================ */
void     config_init(void);
bool     config_save(void);
void     config_reset(void);

/* ================================================================
 * 设置接口
 * ================================================================ */
uint32_t config_get_odr(void);
bool     config_set_odr(uint32_t odr_hz);
uint32_t config_get_fusion_mode(void);
bool     config_set_fusion_mode(uint32_t mode);

/* ================================================================
 * 校准接口
 * ================================================================ */
void          config_calib_start(void);
void          config_calib_reset(void);
void          config_calib_print_status(void);
void          config_calib_process(float gx, float gy, float gz,
                                   float ax, float ay, float az);
calib_state_t config_calib_get_state(void);
bool          config_calib_is_valid(void);

/* ================================================================
 * 校准应用接口
 * ================================================================ */
void config_apply_gyro(float *gx, float *gy, float *gz);
void config_apply_acc (float *ax, float *ay, float *az);

/* 新增：查询校准是否正在进行中（非 IDLE/COMPLETE/ERROR） */
bool config_calib_is_active(void);
/* hw_version 接口 */
const char *config_get_hw_version(void);
bool        config_set_hw_version(const char *ver);

#endif /* BSP_CONFIG_H */
