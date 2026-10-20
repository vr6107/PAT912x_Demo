#include "bsp_timer.h"

/* ----------------------------------------------------------------
 * 内部：获取 TMR2 输入时钟频率
 * TMR2 挂在 APB1，APB1 分频 > 1 时定时器时钟 = APB1 × 2
 * ---------------------------------------------------------------- */
static uint32_t get_tmr2_clock_hz(void)
{
    crm_clocks_freq_type clocks;
    crm_clocks_freq_get(&clocks);

    /* CRM->cfg_bit.apb1div >= 4 表示 APB1 实际分频 > 1 */
    if (CRM->cfg_bit.apb1div >= 4)
    {
        return clocks.apb1_freq * 2U;
    }
    return clocks.apb1_freq;
}

/* ----------------------------------------------------------------
 * bsp_timer_init
 * TMR2 配置为自由运行 32 位微秒计数器
 * 目标：计数频率 1MHz，1 tick = 1 us
 * 溢出时间：2^32 / 1000000 ≈ 4294 秒
 * ---------------------------------------------------------------- */
void bsp_timer_init(void)
{
    /* 1. 使能 TMR2 外设时钟 */
    crm_periph_clock_enable(CRM_TMR2_PERIPH_CLOCK, TRUE);

    /* 2. 自动计算预分频，目标 1MHz */
    uint32_t tmr_clk   = get_tmr2_clock_hz();
    uint32_t prescaler = (tmr_clk / 1000000U) - 1U;

    /* 3. 初始化：周期设为32位最大值，预分频自动计算
     * API：tmr_base_init(tmr_x, tmr_pr, tmr_div)
     *   tmr_pr  = 自动重装载值（period）
     *   tmr_div = 预分频值
     * TMR2 是真正的32位计数器，0xFFFFFFFF有效
     */
    tmr_base_init(TMR2, 0xFFFFFFFFU, prescaler);

    /* 4. 向上计数模式（默认即是，显式设置更安全） */
    tmr_cnt_dir_set(TMR2, TMR_COUNT_UP);

    /* 5. 计数器归零后启动 */
    tmr_counter_value_set(TMR2, 0U);
    tmr_counter_enable(TMR2, TRUE);
}

/* ----------------------------------------------------------------
 * system_us
 * 返回微秒时间戳，直接读硬件寄存器
 * 可在中断中安全调用，无锁，开销约 2 个指令周期
 * ---------------------------------------------------------------- */
uint32_t system_us(void)
{
    return tmr_counter_value_get(TMR2);
}

/* ----------------------------------------------------------------
 * system_ms_precise
 * 基于微秒计数器的高精度毫秒时间戳
 * ---------------------------------------------------------------- */
uint32_t system_ms_precise(void)
{
    return tmr_counter_value_get(TMR2) / 1000U;
}

/* ----------------------------------------------------------------
 * elapsed_us
 * 计算从 since_us 到现在经过的微秒数
 * 无符号减法自动处理 32 位溢出回绕，4294 秒内永远正确
 *
 * 用法：
 *   uint32_t t0 = system_us();
 *   do_something();
 *   uint32_t cost = elapsed_us(t0);
 * ---------------------------------------------------------------- */
uint32_t elapsed_us(uint32_t since_us)
{
    return tmr_counter_value_get(TMR2) - since_us;
}

/* ----------------------------------------------------------------
 * bsp_delay_us
 * 阻塞式微秒级精准延时
 * ---------------------------------------------------------------- */
void bsp_delay_us(uint32_t us)
{
    uint32_t t0 = system_us();
    while (elapsed_us(t0) < us)
    {
        /* 紧凑循环等待 */
    }
}

/* ----------------------------------------------------------------
 * bsp_delay_ms
 * 阻塞式毫秒级精准延时
 * ---------------------------------------------------------------- */
void bsp_delay_ms(uint32_t ms)
{
    bsp_delay_us(ms * 1000U);
}
