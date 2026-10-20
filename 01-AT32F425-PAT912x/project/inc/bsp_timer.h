#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include "at32f425_board.h"
#include <stdint.h>

/* ----------------------------------------------------------------
 * 公开接口
 * ---------------------------------------------------------------- */
void     bsp_timer_init(void);
uint32_t system_us(void);
uint32_t system_ms_precise(void);        // 基于us的高精度ms（可替代system_ms）
uint32_t elapsed_us(uint32_t since_us);  // 计算经过时间，自动处理溢出

/* 将原来的 delay_us 和 delay_ms 改名 */
void     bsp_delay_us(uint32_t us);          // 微秒级精准延时
void     bsp_delay_ms(uint32_t ms);          // 毫秒级精准延时


#endif /* BSP_TIMER_H */
