#include "protocol.h"

#include <string.h>

static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static void put_u64(uint8_t *out, uint64_t value) {
    put_u32(out, (uint32_t)value);
    put_u32(out + 4, (uint32_t)(value >> 32));
}

static void put_f32(uint8_t *out, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    put_u32(out, bits);
}

uint32_t u4rk_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8u; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

void u4rk_serialize_header(uint8_t destination[U4RK_HEADER_SIZE],
                           const u4rk_frame_header_t *header) {
    memset(destination, 0, U4RK_HEADER_SIZE);
    memcpy(destination, "P0RK", 4);
    destination[4] = U4RK_PROTOCOL_VERSION;
    destination[5] = header->payload_type;
    put_u16(destination + 6, header->flags);
    put_u32(destination + 8, header->sequence);
    put_u32(destination + 12, header->sample_count);
    put_u32(destination + 16, header->sample_rate_hz);
    put_u32(destination + 20, header->payload_bytes);
    put_u64(destination + 24, header->capture_timestamp_us);
    put_f32(destination + 32, header->adc_dc_mean);
    put_f32(destination + 36, header->envelope_peak);
    put_f32(destination + 40, header->alaw_reference);
    put_u32(destination + 44, header->pulse.negative_ns);
    put_u32(destination + 48, header->pulse.damp_ns);
    put_u32(destination + 52, header->pulse.positive_ns);
    put_u32(destination + 56, header->dropped_frames);
    put_u32(destination + 60, header->payload_crc32);
}
