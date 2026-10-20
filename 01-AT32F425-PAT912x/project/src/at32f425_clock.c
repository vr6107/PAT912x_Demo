/**
  **************************************************************************
  * @file     at32f425_clock.c
  * @brief    system clock config program
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
/**
  **************************************************************************
  * @file     at32f425_clock.c
  * @brief    system clock config program
  *           HEXT(8MHz) × PLL×12 = 96MHz SCLK
  *           USB = 96MHz ÷ 2 = 48MHz ✅
  **************************************************************************
  */

#include "at32f425_clock.h"

void system_clock_config(void)
{
    crm_reset();
    flash_psr_set(FLASH_WAIT_CYCLE_2);

    /* ── 1. 使能外部晶振 HEXT ──────────────────────────────── */
    crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, TRUE);
    while (crm_flag_get(CRM_HEXT_STABLE_FLAG) != SET) {}

    /* ── 2. PLL：HEXT(12MHz) × 8 = 96MHz ──────────────────── */
    crm_pll_config(CRM_PLL_SOURCE_HEXT, CRM_PLL_MULT_8);
    crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);
    while (crm_flag_get(CRM_PLL_STABLE_FLAG) != SET) {}

    /* ── 3. 总线分频 ───────────────────────────────────────── */
    crm_ahb_div_set(CRM_AHB_DIV_1);    /* AHB  = 96MHz */
    crm_apb2_div_set(CRM_APB2_DIV_1);  /* APB2 = 96MHz */
    crm_apb1_div_set(CRM_APB1_DIV_1);  /* APB1 = 96MHz */

    /* ── 4. 切换系统时钟到 PLL ─────────────────────────────── */
    crm_sysclk_switch(CRM_SCLK_PLL);
    while (crm_sysclk_switch_status_get() != CRM_SCLK_PLL) {}

    /* ── 5. USB 时钟：96MHz ÷ 2 = 48MHz ✅ ─────────────────── */
    crm_usb_clock_div_set(CRM_USB_DIV_2);

    system_core_clock_update();
}

