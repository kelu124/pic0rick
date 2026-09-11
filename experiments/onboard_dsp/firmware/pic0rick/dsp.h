#ifndef U4RK_DSP_H
#define U4RK_DSP_H

#include "u4rk.h"

bool u4rk_dsp_init(void);
float u4rk_dsp_extract(const uint16_t *dma_samples, uint16_t *raw_out);
void u4rk_dsp_envelope(const uint16_t *dma_samples, uint16_t *raw_out,
                       float reference, bool make_alaw,
                       float **envelope_out, uint8_t **alaw_out,
                       bool *saturated, u4rk_dsp_metrics_t *metrics);
void u4rk_dsp_make_selftest(uint8_t test_case, uint16_t *dma_samples);
const char *u4rk_dsp_selftest_name(uint8_t test_case);
uint8_t u4rk_dsp_selftest_count(void);

#endif
