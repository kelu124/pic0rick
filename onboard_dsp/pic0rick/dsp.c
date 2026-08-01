#include "dsp.h"

#include <math.h>
#include <string.h>

#include "arm_math.h"
#include "pico/stdlib.h"

#define U4RK_ALAW_A 87.6f
#define U4RK_ALAW_LUT_SIZE 4096u
#define U4RK_PI 3.14159265358979323846f

static arm_cfft_instance_f32 cfft;
static float32_t complex_buffer[U4RK_SAMPLE_COUNT * 2u]
    __attribute__((aligned(16)));
static float32_t envelope_buffer[U4RK_SAMPLE_COUNT]
    __attribute__((aligned(16)));
static uint8_t alaw_buffer[U4RK_SAMPLE_COUNT] __attribute__((aligned(16)));
static uint8_t alaw_lut[U4RK_ALAW_LUT_SIZE];
static uint32_t worst_total_us;

static uint32_t elapsed_us(uint64_t start) {
    return (uint32_t)(time_us_64() - start);
}

bool u4rk_dsp_init(void) {
    if (arm_cfft_init_4096_f32(&cfft) != ARM_MATH_SUCCESS) {
        return false;
    }

    const float denominator = 1.0f + logf(U4RK_ALAW_A);
    for (uint32_t i = 0; i < U4RK_ALAW_LUT_SIZE; ++i) {
        float x = (float)i / (float)(U4RK_ALAW_LUT_SIZE - 1u);
        float y;
        if (x < (1.0f / U4RK_ALAW_A)) {
            y = U4RK_ALAW_A * x / denominator;
        } else {
            y = (1.0f + logf(U4RK_ALAW_A * x)) / denominator;
        }
        int level = (int)(255.0f * y + 0.5f);
        if (level < 0) {
            level = 0;
        } else if (level > 255) {
            level = 255;
        }
        alaw_lut[i] = (uint8_t)level;
    }
    worst_total_us = 0;
    return true;
}

float u4rk_dsp_extract(const uint16_t *dma_samples, uint16_t *raw_out) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
        uint16_t value = (uint16_t)((dma_samples[i] >> 1) & 0x03ffu);
        raw_out[i] = value;
        sum += value;
    }
    return (float)sum / (float)U4RK_SAMPLE_COUNT;
}

void u4rk_dsp_envelope(const uint16_t *dma_samples, uint16_t *raw_out,
                       float reference, bool make_alaw,
                       float **envelope_out, uint8_t **alaw_out,
                       bool *saturated, u4rk_dsp_metrics_t *metrics) {
    memset(metrics, 0, sizeof(*metrics));
    uint64_t total_started = time_us_64();
    uint64_t stage_started = total_started;

    float mean = u4rk_dsp_extract(dma_samples, raw_out);
    for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
        complex_buffer[2u * i] = (float32_t)raw_out[i] - mean;
        complex_buffer[2u * i + 1u] = 0.0f;
    }
    metrics->dc_mean = mean;
    metrics->preprocess_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    arm_cfft_f32(&cfft, complex_buffer, 0, 1);
    metrics->forward_fft_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    arm_scale_f32(&complex_buffer[2], 2.0f, &complex_buffer[2],
                  (U4RK_SAMPLE_COUNT / 2u - 1u) * 2u);
    memset(&complex_buffer[(U4RK_SAMPLE_COUNT / 2u + 1u) * 2u], 0,
           (U4RK_SAMPLE_COUNT / 2u - 1u) * 2u * sizeof(float32_t));
    metrics->mask_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    /*
     * CMSIS-DSP's inverse arm_cfft_f32 path performs the 1/N normalization.
     * Adding another scale here would make the result 4096 times too small.
     */
    arm_cfft_f32(&cfft, complex_buffer, 1, 1);
    metrics->inverse_fft_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    arm_cmplx_mag_f32(complex_buffer, envelope_buffer, U4RK_SAMPLE_COUNT);
    uint32_t peak_index;
    arm_max_f32(envelope_buffer, U4RK_SAMPLE_COUNT,
                &metrics->envelope_peak, &peak_index);
    (void)peak_index;
    metrics->magnitude_us = elapsed_us(stage_started);

    *saturated = false;
    if (make_alaw) {
        stage_started = time_us_64();
        float inv_reference = 1.0f / reference;
        for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
            float normalized = envelope_buffer[i] * inv_reference;
            if (normalized > 1.0f) {
                normalized = 1.0f;
                *saturated = true;
            } else if (normalized < 0.0f) {
                normalized = 0.0f;
            }
            uint32_t index =
                (uint32_t)(normalized * (U4RK_ALAW_LUT_SIZE - 1u) + 0.5f);
            alaw_buffer[i] = alaw_lut[index];
        }
        metrics->alaw_us = elapsed_us(stage_started);
    }

    metrics->total_us = elapsed_us(total_started);
    if (metrics->total_us > worst_total_us) {
        worst_total_us = metrics->total_us;
    }
    metrics->worst_total_us = worst_total_us;
    *envelope_out = envelope_buffer;
    *alaw_out = alaw_buffer;
}

static uint16_t clamp_adc(float value) {
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 1023.0f) {
        value = 1023.0f;
    }
    return (uint16_t)(value + 0.5f);
}

void u4rk_dsp_make_selftest(uint8_t test_case, uint16_t *dma_samples) {
    for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
        float value;
        float carrier = sinf(2.0f * U4RK_PI * 80.0f * (float)i /
                             (float)U4RK_SAMPLE_COUNT);
        switch (test_case) {
            case 0: /* zero */
                value = 0.0f;
                break;
            case 1: /* DC */
                value = 700.0f;
                break;
            case 2: /* centred bin-coherent sinusoid */
                value = 512.0f +
                    200.0f * sinf(2.0f * U4RK_PI * 32.0f * (float)i /
                                  (float)U4RK_SAMPLE_COUNT);
                break;
            case 3: { /* amplitude-modulated tone */
                float modulation = 1.0f +
                    0.65f * sinf(2.0f * U4RK_PI * 5.0f * (float)i /
                                 (float)U4RK_SAMPLE_COUNT);
                value = 512.0f + 190.0f * modulation * carrier;
                break;
            }
            case 4: /* two separated tone bursts */
                value = 512.0f;
                if ((i >= 480u && i < 900u) ||
                    (i >= 2250u && i < 2730u)) {
                    value += 260.0f * carrier;
                }
                break;
            case 5: /* impulse */
                value = (i == U4RK_SAMPLE_COUNT / 2u) ? 1023.0f : 512.0f;
                break;
            default: /* clipping/extreme square wave */
                value = ((i / 16u) & 1u) ? 1023.0f : 0.0f;
                break;
        }
        /* Acquisition buffers include GPIO0 at bit zero, hence << 1. */
        dma_samples[i] = (uint16_t)(clamp_adc(value) << 1);
    }
}

const char *u4rk_dsp_selftest_name(uint8_t test_case) {
    static const char *const names[] = {
        "zero", "dc", "sinusoid", "am", "two-bursts", "impulse", "clipping",
    };
    return test_case < (sizeof(names) / sizeof(names[0]))
        ? names[test_case] : "unknown";
}

uint8_t u4rk_dsp_selftest_count(void) {
    return 7u;
}
