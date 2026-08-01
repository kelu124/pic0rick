#ifndef U4RK_PIPELINE_H
#define U4RK_PIPELINE_H

#include "u4rk.h"

bool u4rk_pipeline_init(void);
void u4rk_pipeline_core1_entry(void);

bool u4rk_pipeline_claim_raw(uint8_t *index, uint16_t **buffer);
void u4rk_pipeline_release_raw(uint8_t index);
bool u4rk_pipeline_submit(const u4rk_capture_job_t *job);

bool u4rk_pipeline_take_output(uint8_t *slot, const uint8_t **data,
                               size_t *size);
void u4rk_pipeline_release_output(uint8_t slot);
bool u4rk_pipeline_has_pending_output(void);
bool u4rk_pipeline_take_completion(uint32_t *sequence);
bool u4rk_pipeline_processing_idle(void);

void u4rk_pipeline_note_processing_drop(void);
void u4rk_pipeline_note_usb_drop(void);
uint32_t u4rk_pipeline_dropped_frames(void);
bool u4rk_pipeline_copy_latest_raw(uint16_t destination[U4RK_SAMPLE_COUNT]);
void u4rk_pipeline_get_metrics(u4rk_dsp_metrics_t *metrics);

#endif
