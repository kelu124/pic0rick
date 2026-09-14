#ifndef U4RK_PROTOCOL_H
#define U4RK_PROTOCOL_H

#include "u4rk.h"

typedef struct {
    uint8_t payload_type;
    uint16_t flags;
    uint32_t sequence;
    uint32_t sample_count;
    uint32_t sample_rate_hz;
    uint32_t payload_bytes;
    uint64_t capture_timestamp_us;
    float adc_dc_mean;
    float envelope_peak;
    float alaw_reference;
    u4rk_pulse_config_t pulse;
    uint32_t dropped_frames;
    uint32_t payload_crc32;
} u4rk_frame_header_t;

uint32_t u4rk_crc32(const uint8_t *data, size_t length);
void u4rk_serialize_header(uint8_t destination[U4RK_HEADER_SIZE],
                           const u4rk_frame_header_t *header);

#endif
