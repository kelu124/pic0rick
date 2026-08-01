#ifndef U4RK_USB_TRANSPORT_H
#define U4RK_USB_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool u4rk_usb_init(void);
void u4rk_usb_task(void);
bool u4rk_usb_connected(void);
int u4rk_usb_read_char(void);

bool u4rk_usb_write_blocking(const void *data, size_t length,
                             uint32_t timeout_ms);
bool u4rk_usb_tx_start(const void *data, size_t length);
bool u4rk_usb_tx_busy(void);
void u4rk_usb_tx_cancel(void);

#endif
