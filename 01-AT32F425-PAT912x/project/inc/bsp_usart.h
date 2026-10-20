#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "at32f425.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define USART_BUFFER_SIZE 256
#define USART_TX_DMA_BUF_SIZE 256   /* 单次DMA发送的最大长度 */

extern uint8_t  usart_rx_buffer[USART_BUFFER_SIZE];
extern volatile uint16_t hw_usart_rx_index;
extern volatile uint16_t hw_usart_read_index;

extern volatile uint8_t g_debug_mode;
extern volatile uint8_t g_is_streaming;

/* DMA发送忙标志，volatile因为会在DMA中断里被清除 */
extern volatile uint8_t g_usart_tx_busy;

/* ----------------------------------------------------------------
 * 打印宏定义：改为走DMA发送函数，而不是直接调用printf
 * ---------------------------------------------------------------- */
#define LOG_INFO(fmt, ...)  bsp_usart_send(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) do { if(g_debug_mode) bsp_usart_send("[DEBUG] " fmt, ##__VA_ARGS__); } while(0)

/* 接口声明 */
void bsp_external_interface_init(uint32_t baudrate);
void usart_process_cmd(void);
void bsp_usart_send(const char *fmt, ...);   /* 新增：DMA格式化发送 */
void calib_cmd_acc_collect_up(void);
void calib_cmd_acc_collect_down(void);


#endif /* __BSP_USART_H */
