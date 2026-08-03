#include "usb_transport.h"

#include "pico/stdlib.h"
#include "tusb.h"

#define U4RK_USB_TX_STALL_TIMEOUT_US 2000000u

static const uint8_t *tx_data;
static size_t tx_length;
static size_t tx_offset;
static bool tx_active;
static bool tx_failed;
static uint64_t tx_last_progress_us;

static void fail_async_tx(void) {
    tx_active = false;
    tx_failed = true;
    tx_data = NULL;
    tx_length = 0;
    tx_offset = 0;
    tud_cdc_write_clear();
}

bool u4rk_usb_init(void) {
    tusb_rhport_init_t device_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };
    tx_active = false;
    tx_failed = false;
    return tusb_init(0, &device_init);
}

void u4rk_usb_task(void) {
    tud_task();
    if (!tx_active) {
        return;
    }

    /* CDC "connected" only reports the optional DTR line state.  Binary
     * transport must instead follow the configured CDC endpoints. */
    if (!tud_mounted()) {
        fail_async_tx();
        return;
    }
    if (!tud_cdc_ready()) {
        if ((time_us_64() - tx_last_progress_us) >=
            U4RK_USB_TX_STALL_TIMEOUT_US) {
            fail_async_tx();
        }
        return;
    }

    uint32_t available = tud_cdc_write_available();
    size_t remaining = tx_length - tx_offset;
    if (available > remaining) {
        available = (uint32_t)remaining;
    }
    if (available != 0u) {
        uint32_t written = tud_cdc_write(tx_data + tx_offset, available);
        tx_offset += written;
        if (written != 0u) {
            tx_last_progress_us = time_us_64();
            tud_cdc_write_flush();
        }
    }
    if (tx_offset == tx_length) {
        tx_active = false;
        tx_data = NULL;
        tx_length = 0;
        tx_offset = 0;
    } else if ((time_us_64() - tx_last_progress_us) >=
               U4RK_USB_TX_STALL_TIMEOUT_US) {
        fail_async_tx();
    }
}

bool u4rk_usb_connected(void) {
    return tud_cdc_ready();
}

bool u4rk_usb_mounted(void) {
    return tud_mounted();
}

int u4rk_usb_read_char(void) {
    if (!tud_cdc_available()) {
        return -1;
    }
    return (int)tud_cdc_read_char();
}

bool u4rk_usb_write_blocking(const void *data, size_t length,
                             uint32_t timeout_ms) {
    if (tx_active || !tud_cdc_ready()) {
        return false;
    }

    const uint8_t *source = (const uint8_t *)data;
    size_t offset = 0;
    uint64_t deadline = time_us_64() + (uint64_t)timeout_ms * 1000u;
    while (offset < length) {
        tud_task();
        if (!tud_mounted() || time_us_64() > deadline) {
            return false;
        }
        if (!tud_cdc_ready()) {
            continue;
        }
        uint32_t available = tud_cdc_write_available();
        size_t remaining = length - offset;
        if (available > remaining) {
            available = (uint32_t)remaining;
        }
        if (available != 0u) {
            offset += tud_cdc_write(source + offset, available);
            tud_cdc_write_flush();
        }
    }
    return true;
}

bool u4rk_usb_tx_start(const void *data, size_t length) {
    if (tx_active || data == NULL || length == 0u || !tud_cdc_ready()) {
        return false;
    }
    tx_data = (const uint8_t *)data;
    tx_length = length;
    tx_offset = 0;
    tx_active = true;
    tx_failed = false;
    tx_last_progress_us = time_us_64();
    return true;
}

bool u4rk_usb_tx_busy(void) {
    return tx_active;
}

bool u4rk_usb_tx_take_failed(void) {
    bool failed = tx_failed;
    tx_failed = false;
    return failed;
}

void u4rk_usb_tx_cancel(void) {
    tx_active = false;
    tx_failed = false;
    tx_data = NULL;
    tx_length = 0;
    tx_offset = 0;
    tud_cdc_write_clear();
}
