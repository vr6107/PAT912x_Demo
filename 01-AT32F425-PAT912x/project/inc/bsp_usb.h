#ifndef __BSP_USB_H
#define __BSP_USB_H

#include <stdint.h>
#include "usb_core.h"    /* otg_core_type */
uint16_t usb_cdc_recv(uint8_t *buf);

extern otg_core_type usb_core_dev;

void    bsp_usb_init(void);
void    usb_cdc_send(uint8_t *buf, uint16_t len);
uint8_t usb_cdc_connected(void);

#endif
