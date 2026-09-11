#include <string.h>

#include "tusb.h"

#define U4RK_USB_VID 0xcafe
#define U4RK_USB_PID 0x4011
#define U4RK_USB_BCD 0x0100

static tusb_desc_device_t const device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = U4RK_USB_VID,
    .idProduct = U4RK_USB_PID,
    .bcdDevice = U4RK_USB_BCD,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&device_descriptor;
}

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL,
};

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

static uint8_t const configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return configuration_descriptor;
}

static char const *const string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "pic0rick",
    "pic0rick RP2350 Signal Processor",
    "P0RK0001",
    "pic0rick control and data",
};
static uint16_t descriptor_string[64];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t count;
    if (index == 0) {
        memcpy(&descriptor_string[1], string_descriptors[0], 2);
        count = 1;
    } else {
        if (index >= sizeof(string_descriptors) / sizeof(string_descriptors[0])) {
            return NULL;
        }
        const char *source = string_descriptors[index];
        count = strlen(source);
        if (count > 63u) {
            count = 63u;
        }
        for (size_t i = 0; i < count; ++i) {
            descriptor_string[1 + i] = (uint8_t)source[i];
        }
    }
    descriptor_string[0] =
        (uint16_t)((TUSB_DESC_STRING << 8) | (2u * count + 2u));
    return descriptor_string;
}
