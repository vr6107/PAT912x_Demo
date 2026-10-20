#include "bsp_usb.h"
#include "usb_conf.h"
#include "usb_core.h"        /* otg_core_type, usbd_init, USB_FULL_SPEED_CORE_ID, USB_ID */
#include "usbd_int.h"        /* usbd_irq_handler ← 之前缺这个！ */
#include "cdc_class.h"       /* cdc_class_handler, usb_vcp_send_data, usb_vcp_get_rxdata */
#include "cdc_desc.h"        /* cdc_desc_handler */
#include "at32f425.h"        /* CRM_OTGFS1_PERIPH_CLOCK, OTGFS1_IRQn */
#include "at32f425_board.h"   /* 提供 delay_ms / delay_us */

/* USB 核心对象实体 */
otg_core_type usb_core_dev;

/* ------------------------------------------------------------------ */
/*  GPIO 配置（PA11=DM, PA12=DP, PA9=VBUS）                           */
/* ------------------------------------------------------------------ */
static void bsp_usb_gpio_config(void)
{
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(OTG_PIN_GPIO_CLOCK, TRUE);
    gpio_default_para_init(&gpio_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;

#ifndef USB_VBUS_IGNORE
    gpio_init_struct.gpio_pins = OTG_PIN_VBUS;
    gpio_init_struct.gpio_pull = GPIO_PULL_DOWN;
    gpio_pin_mux_config(OTG_PIN_GPIO, OTG_PIN_VBUS_SOURCE, OTG_PIN_MUX);
    gpio_init(OTG_PIN_GPIO, &gpio_init_struct);
#endif
}

/* ------------------------------------------------------------------ */
/*  USB 48MHz 时钟选择（与官方例程完全一致）                           */
/* ------------------------------------------------------------------ */
static void bsp_usb_clock48m_select(void)
{
    crm_clocks_freq_type clocks_struct;

    crm_usb_div_reset();
    crm_clocks_freq_get(&clocks_struct);

    switch (clocks_struct.sclk_freq)
    {
        case 48000000: crm_usb_clock_div_set(CRM_USB_DIV_1);   break;
        case 72000000: crm_usb_clock_div_set(CRM_USB_DIV_1_5); break;
        case 96000000: crm_usb_clock_div_set(CRM_USB_DIV_2);   break; /* 你的配置 */
        default: break;
    }
}

/* ------------------------------------------------------------------ */
/*  USB 初始化入口                                                      */
/* ------------------------------------------------------------------ */
void bsp_usb_init(void)
{
    /* Step 1: GPIO */
    bsp_usb_gpio_config();

    /* Step 2: 使能 OTGFS1 外设时钟 */
    crm_periph_clock_enable(OTG_CLOCK, TRUE);

    /* Step 3: 48MHz 时钟分频 */
    bsp_usb_clock48m_select();

    /* Step 4: 使能中断（优先级与官方一致：0,0） */
    nvic_irq_enable(OTG_IRQ, 0, 0);

    /* Step 5: 初始化 USB Device 协议栈 */
    usbd_init(&usb_core_dev,
              USB_FULL_SPEED_CORE_ID,
              USB_ID,               /* 来自 usb_conf.h，通常为 0 */
              &cdc_class_handler,
              &cdc_desc_handler);
}

/* ------------------------------------------------------------------ */
/*  USB OTG FS 中断服务函数（宏展开后 = OTGFS1_IRQHandler）            */
/* ------------------------------------------------------------------ */
void OTG_IRQ_HANDLER(void)
{
#ifdef USB_LOW_POWER_WAKUP
    if (exint_flag_get(OTG_WKUP_EXINT_LINE))
        exint_flag_clear(OTG_WKUP_EXINT_LINE);
#endif
    usbd_irq_handler(&usb_core_dev);   /* ← 正确，来自 usbd_int.h */
}

/* ------------------------------------------------------------------ */
/*  连接状态检测                                                        */
/* ------------------------------------------------------------------ */
uint8_t usb_cdc_connected(void)
{
    return (usbd_connect_state_get(&usb_core_dev.dev) == USB_CONN_STATE_CONFIGURED) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/*  发送数据（注意：传 .dev 子结构，与官方一致）                        */
/* ------------------------------------------------------------------ */
void usb_cdc_send(uint8_t *buf, uint16_t len)
{
    if (!usb_cdc_connected()) return;

    uint32_t timeout = 50000;
    do {
        if (usb_vcp_send_data(&usb_core_dev.dev, buf, len) == SUCCESS)
            return;
    } while (timeout--);
}

uint16_t usb_cdc_recv(uint8_t *buf)
{
    if (!usb_cdc_connected()) return 0u;
    return usb_vcp_get_rxdata(&usb_core_dev.dev, buf);
}

/* ------------------------------------------------------------------ */
/*  USB 延时（官方要求必须实现）                                        */
/* ------------------------------------------------------------------ */
void usb_delay_ms(uint32_t ms) { delay_ms(ms); }
void usb_delay_us(uint32_t us) { delay_us(us); }
