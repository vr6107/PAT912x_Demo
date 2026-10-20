#include "at32f425_board.h"
#include "at32f425_clock.h"
#include "bsp_usb.h"
#include "bsp_i2c.h"
#include "pat9126.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ----------------------------------------------------------------
 * 参数
 * ---------------------------------------------------------------- */
#define POLL_INTERVAL_MS   5u
#define QUALITY_MIN        19u
#define USB_TX_BUF_SIZE    128u

/* 串口接收复位功能：bsp_usb 提供 usb_cdc_recv() 后改成 1 */
#define ENABLE_USB_RX      1

#define UM_PER_COUNT_NUM   25400
#define UM_PER_COUNT_DEN   PAT9126_CPI

/* ----------------------------------------------------------------
 * 位移状态：只存整数 count
 * ---------------------------------------------------------------- */
typedef struct {
    int32_t total_x;
    int32_t total_y;
} motion_state_t;

static motion_state_t s_state;

static int32_t counts_to_um(int32_t counts)
{
    return (int32_t)(((int64_t)counts * UM_PER_COUNT_NUM) / UM_PER_COUNT_DEN);
}

static void split_um(int32_t um, const char **sign, int32_t *mm, int32_t *frac)
{
    int32_t a = (um < 0) ? -um : um;
    *sign = (um < 0) ? "-" : "";
    *mm   = a / 1000;
    *frac = a % 1000;
}

static void update_state(motion_state_t *st, const PAT9126_Motion_t *m)
{
    if (m->motion_detected && !m->overflow && m->image_quality > QUALITY_MIN) {
        st->total_x += m->delta_x;
        st->total_y += m->delta_y;
    }
}

/* ----------------------------------------------------------------
 * USB 输出：双缓冲
 * ---------------------------------------------------------------- */
static uint8_t s_usb_tx_buf[2][USB_TX_BUF_SIZE];
static uint8_t s_buf_idx = 0u;

static void usb_printf(const char *fmt, ...)
{
    uint8_t *buf = s_usb_tx_buf[s_buf_idx];
    va_list  args;
    int      len;

    va_start(args, fmt);
    len = vsnprintf((char *)buf, USB_TX_BUF_SIZE, fmt, args);
    va_end(args);

    if (len <= 0) return;
    if (len >= (int)USB_TX_BUF_SIZE) len = (int)USB_TX_BUF_SIZE - 1;

    if (usb_cdc_connected()) {
        usb_cdc_send(buf, (uint16_t)len);
        s_buf_idx ^= 1u;
    }
}

/* ----------------------------------------------------------------
 * 串口接收（可选）
 * ---------------------------------------------------------------- */
#if ENABLE_USB_RX
#define USB_RX_BUF_SIZE 256u

static void handle_rx(void)
{
    static uint8_t rx_buf[USB_RX_BUF_SIZE];
    uint16_t n = usb_cdc_recv(rx_buf);     /* 需在 bsp_usb 中实现 */
    uint16_t i;
    uint8_t  need_reset = 0u;

    for (i = 0u; i < n; i++) {
        if (rx_buf[i] == 'R' || rx_buf[i] == 'r') need_reset = 1u;
    }
    if (need_reset) {
        memset(&s_state, 0, sizeof(s_state));
        usb_printf("#RESET\r\n");
    }
}
#endif

/* ----------------------------------------------------------------
 * main
 * ---------------------------------------------------------------- */
int main(void)
{
    PAT9126_Status_t pat_ret;
    PAT9126_Motion_t motion;
    uint8_t          header_sent = 0u;
    uint8_t          last_q      = 0u;
    uint32_t         ts_ms       = 0u;
    uint32_t         tick        = 0u;

    nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
    system_clock_config();
    at32_board_init();

    bsp_usb_init();
    bsp_i2c_init();

    pat_ret = PAT9126_Init(I2C_SENSOR_1);
    memset(&motion, 0, sizeof(motion));
    memset(&s_state, 0, sizeof(s_state));

    while (1)
    {
        if (!header_sent && usb_cdc_connected()) {
            usb_printf("$timestamp_ms,p1_dx,p1_dy,p1_tx_mm,p1_ty_mm,p1_sq\r\n");
            header_sent = 1u;
            ts_ms = 0u;
        }

        if (pat_ret != PAT9126_OK) {
            if (++tick >= (1000u / POLL_INTERVAL_MS)) {
                tick = 0u;
                pat_ret = PAT9126_Init(I2C_SENSOR_1);
            }
        }
        else if (header_sent)
        {
            PAT9126_Status_t r = PAT9126_ReadMotion(I2C_SENSOR_1, &motion);

            if (r == PAT9126_OK) {
                const char *sx, *sy;
                int32_t     xm, xf, ym, yf;

                update_state(&s_state, &motion);
                if (motion.motion_detected) last_q = motion.image_quality;

                split_um(counts_to_um(s_state.total_x), &sx, &xm, &xf);
                split_um(counts_to_um(s_state.total_y), &sy, &ym, &yf);

                usb_printf("$%lu,PAT912x, %d,%d,%s%ld.%03ld,%s%ld.%03ld,%u\r\n",
                           (unsigned long)ts_ms,
                           (int)motion.delta_x, (int)motion.delta_y,
                           sx, (long)xm, (long)xf,
                           sy, (long)ym, (long)yf,
                           (unsigned)last_q);
            }
        }

#if ENABLE_USB_RX
        handle_rx();
#endif

        delay_ms(POLL_INTERVAL_MS);
        ts_ms += POLL_INTERVAL_MS;
    }
}
