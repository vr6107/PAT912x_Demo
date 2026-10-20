/**
  **************************************************************************
  * @file     at32f425_board.h
  * @brief    header file for at-start board. set of firmware functions to
  *           manage leds and push-button. initialize delay function.
  **************************************************************************
  *
  * Copyright (c) 2025, Artery Technology, All rights reserved.
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#ifndef __AT32F425_BOARD_H
#define __AT32F425_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "at32f425.h"

/** @addtogroup AT32F425_board
  * @{
  */

/** @addtogroup BOARD
  * @{
  */

/** @defgroup BOARD_pins_definition
  * @{
  */

/**
  * this header include define support list:
  * 1. at-start-f425 v1.x boards
  * if define AT_START_F425_V1, the header file support at-start-f425 v1.x board
  */

// #if !defined (AT_START_F425_V1)
// #error "please select first the board at-start device used in your application (in at32f425_board.h file)"
// #endif

// /******************** define led ********************/
// typedef enum
// {
//   LED2                                   = 0,
//   LED3                                   = 1,
//   LED4                                   = 2
// } led_type;

// #define LED_NUM                          3

// #if defined (AT_START_F425_V1)
// #define LED2_PIN                         GPIO_PINS_2
// #define LED2_GPIO                        GPIOC
// #define LED2_GPIO_CRM_CLK                CRM_GPIOC_PERIPH_CLOCK

// #define LED3_PIN                         GPIO_PINS_3
// #define LED3_GPIO                        GPIOC
// #define LED3_GPIO_CRM_CLK                CRM_GPIOC_PERIPH_CLOCK

// #define LED4_PIN                         GPIO_PINS_5
// #define LED4_GPIO                        GPIOC
// #define LED4_GPIO_CRM_CLK                CRM_GPIOC_PERIPH_CLOCK
// #endif

/**************** define print uart ******************/
// #define PRINT_UART                       USART1
// #define PRINT_UART_CRM_CLK               CRM_USART1_PERIPH_CLOCK
// #define PRINT_UART_TX_PIN                GPIO_PINS_9
// #define PRINT_UART_TX_GPIO               GPIOA
// #define PRINT_UART_TX_GPIO_CRM_CLK       CRM_GPIOA_PERIPH_CLOCK
// #define PRINT_UART_TX_PIN_SOURCE         GPIO_PINS_SOURCE9
// #define PRINT_UART_TX_PIN_MUX_NUM        GPIO_MUX_1

/******************* define button *******************/
// typedef enum
// {
//   USER_BUTTON                            = 0,
//   NO_BUTTON                              = 1
// } button_type;

// #define USER_BUTTON_PIN                  GPIO_PINS_0
// #define USER_BUTTON_PORT                 GPIOA
// #define USER_BUTTON_CRM_CLK              CRM_GPIOA_PERIPH_CLOCK


/**
  * @}
  */

/** @defgroup BOARD_exported_functions
  * @{
  */

/******************** functions ********************/
void at32_board_init(void);

/* printf uart init function */
void uart_print_init(uint32_t baudrate);

/* ----------------------------------------------------------------
 * 微秒时间戳
 * ---------------------------------------------------------------- */
/* delay function */
void delay_init(void);
void delay_us(uint32_t nus);
void delay_ms(uint16_t nms);
void delay_sec(uint16_t sec);

/* ✅ 新增：自由运行模式下的延时（systick_restart_freerun 之后使用）*/
void freerun_delay_us(uint32_t nus);
void freerun_delay_ms(uint32_t nms);

/* ✅ 新增：所有初始化完成后调用一次，启动 system_us() */
void systick_restart_freerun(void);

/* 兼容旧名称 */
#define bsp_delay_us(us)  delay_us(us)
#define bsp_delay_ms(ms)  delay_ms((uint16_t)(ms))

/* 微秒时间戳 */
uint32_t system_us(void);
uint32_t elapsed_us(uint32_t t0);

/* 在 at32f425_board.h 的 exported functions 区域添加 */
extern volatile uint32_t s_tick_us;
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif

