#include "dsp.h"

#include <math.h>
#include <string.h>

#include "arm_math.h"
#include "pico/stdlib.h"

#define U4RK_ALAW_A 87.6f
#define U4RK_ALAW_LUT_SIZE 4096u
#define U4RK_PI 3.14159265358979323846f

static arm_rfft_fast_instance_f32 rfft;
static float32_t rfft_buffer[U4RK_SAMPLE_COUNT]
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
    if (arm_rfft_fast_init_4096_f32(&rfft) != ARM_MATH_SUCCESS) {
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
        rfft_buffer[i] = (float32_t)raw_out[i] - mean;
    }
    metrics->dc_mean = mean;
    metrics->preprocess_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    /* A real input has a redundant negative-frequency half, so the fast real
     * FFT performs half the complex FFT work of the former implementation. */
    arm_rfft_fast_f32(&rfft, rfft_buffer, envelope_buffer, 0);
    metrics->forward_fft_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    /* Build the packed spectrum of the real Hilbert transform. Multiplication
     * by -j maps (real + j*imag) to (imag - j*real). */
    envelope_buffer[0] = 0.0f;
    envelope_buffer[1] = 0.0f;
    for (uint32_t i = 1; i < U4RK_SAMPLE_COUNT / 2u; ++i) {
        float32_t real = envelope_buffer[2u * i];
        float32_t imag = envelope_buffer[2u * i + 1u];
        envelope_buffer[2u * i] = imag;
        envelope_buffer[2u * i + 1u] = -real;
    }
    metrics->mask_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    arm_rfft_fast_f32(&rfft, envelope_buffer, rfft_buffer, 1);
    metrics->inverse_fft_us = elapsed_us(stage_started);

    stage_started = time_us_64();
    metrics->envelope_peak = 0.0f;
    for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
        float32_t real = (float32_t)raw_out[i] - mean;
        float32_t quadrature = rfft_buffer[i];
        float32_t magnitude = sqrtf(real * real + quadrature * quadrature);
        envelope_buffer[i] = magnitude;
        if (magnitude > metrics->envelope_peak) {
            metrics->envelope_peak = magnitude;
        }
    }
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
    /* Select the case outside the sample loop. In particular, zero and DC no
     * longer execute 4096 unnecessary sinf calls before the first frame. */
    if (test_case == 0u) {
        memset(dma_samples, 0, U4RK_SAMPLE_COUNT * sizeof(*dma_samples));
        return;
    }
    if (test_case == 1u) {
        for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
            dma_samples[i] = (uint16_t)(700u << 1);
        }
        return;
    }
    if (test_case == 2u) {
        for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
            float value = 512.0f +
                200.0f * sinf(2.0f * U4RK_PI * 32.0f * (float)i /
                              (float)U4RK_SAMPLE_COUNT);
            dma_samples[i] = (uint16_t)(clamp_adc(value) << 1);
        }
        return;
    }
    if (test_case == 3u) {
        for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
            float carrier = sinf(2.0f * U4RK_PI * 80.0f * (float)i /
                                 (float)U4RK_SAMPLE_COUNT);
            float modulation = 1.0f +
                0.65f * sinf(2.0f * U4RK_PI * 5.0f * (float)i /
                              (float)U4RK_SAMPLE_COUNT);
            float value = 512.0f + 190.0f * modulation * carrier;
            dma_samples[i] = (uint16_t)(clamp_adc(value) << 1);
        }
        return;
    }
    if (test_case == 4u) {
        for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
            float value = 512.0f;
            if ((i >= 480u && i < 900u) ||
                (i >= 2250u && i < 2730u)) {
                float carrier = sinf(2.0f * U4RK_PI * 80.0f * (float)i /
                                     (float)U4RK_SAMPLE_COUNT);
                value += 260.0f * carrier;
            }
            dma_samples[i] = (uint16_t)(clamp_adc(value) << 1);
        }
        return;
    }
    if (test_case == 5u) {
        for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
            uint16_t value =
                i == U4RK_SAMPLE_COUNT / 2u ? 1023u : 512u;
            dma_samples[i] = (uint16_t)(value << 1);
        }
        return;
    }
    for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
        uint16_t value = ((i / 16u) & 1u) ? 1023u : 0u;
        dma_samples[i] = (uint16_t)(value << 1);
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
