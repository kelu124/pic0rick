#include "pipeline.h"

#include <string.h>

#include "pico/critical_section.h"
#include "pico/util/queue.h"

#include "dsp.h"
#include "protocol.h"

typedef struct {
    size_t size;
    uint32_t sequence;
    uint32_t session_id;
    uint8_t bytes[U4RK_MAX_FRAME_SIZE];
} output_slot_t;

static uint16_t raw_buffers[U4RK_RAW_BUFFER_COUNT][U4RK_SAMPLE_COUNT]
    __attribute__((aligned(16)));
static uint16_t latest_raw[U4RK_SAMPLE_COUNT] __attribute__((aligned(16)));
static uint16_t raw_work[U4RK_SAMPLE_COUNT] __attribute__((aligned(16)));
static output_slot_t output_slots[U4RK_OUTPUT_SLOT_COUNT]
    __attribute__((aligned(16)));

static queue_t raw_free_queue;
static queue_t job_queue;
static queue_t output_free_queue;
static queue_t output_ready_queue;
static queue_t completion_queue;
static critical_section_t shared_lock;
static bool latest_valid;
static u4rk_dsp_metrics_t latest_metrics;
static volatile uint32_t processing_drops;
static volatile uint32_t usb_drops;

static void store_u16_le(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

bool u4rk_pipeline_init(void) {
    queue_init(&raw_free_queue, sizeof(uint8_t), U4RK_RAW_BUFFER_COUNT);
    queue_init(&job_queue, sizeof(u4rk_capture_job_t), U4RK_RAW_BUFFER_COUNT);
    queue_init(&output_free_queue, sizeof(uint8_t), U4RK_OUTPUT_SLOT_COUNT);
    queue_init(&output_ready_queue, sizeof(uint8_t), U4RK_OUTPUT_SLOT_COUNT);
    queue_init(&completion_queue, sizeof(uint32_t), U4RK_RAW_BUFFER_COUNT);
    critical_section_init(&shared_lock);
    latest_valid = false;
    processing_drops = 0;
    usb_drops = 0;
    memset(&latest_metrics, 0, sizeof(latest_metrics));

    for (uint8_t i = 0; i < U4RK_RAW_BUFFER_COUNT; ++i) {
        queue_add_blocking(&raw_free_queue, &i);
    }
    for (uint8_t i = 0; i < U4RK_OUTPUT_SLOT_COUNT; ++i) {
        queue_add_blocking(&output_free_queue, &i);
    }
    return u4rk_dsp_init();
}

bool u4rk_pipeline_claim_raw(uint8_t *index, uint16_t **buffer) {
    if (!queue_try_remove(&raw_free_queue, index)) {
        return false;
    }
    *buffer = raw_buffers[*index];
    return true;
}

void u4rk_pipeline_release_raw(uint8_t index) {
    queue_add_blocking(&raw_free_queue, &index);
}

bool u4rk_pipeline_submit(const u4rk_capture_job_t *job) {
    if (queue_try_add(&job_queue, job)) {
        return true;
    }
    u4rk_pipeline_release_raw(job->raw_index);
    u4rk_pipeline_note_processing_drop();
    return false;
}

void u4rk_pipeline_note_processing_drop(void) {
    __atomic_fetch_add(&processing_drops, 1u, __ATOMIC_RELAXED);
}

void u4rk_pipeline_note_usb_drop(void) {
    __atomic_fetch_add(&usb_drops, 1u, __ATOMIC_RELAXED);
}

uint32_t u4rk_pipeline_dropped_frames(void) {
    return __atomic_load_n(&processing_drops, __ATOMIC_RELAXED) +
           __atomic_load_n(&usb_drops, __ATOMIC_RELAXED);
}

static void copy_latest(const uint16_t *source,
                        const u4rk_dsp_metrics_t *metrics) {
    critical_section_enter_blocking(&shared_lock);
    memcpy(latest_raw, source, sizeof(latest_raw));
    if (metrics != NULL) {
        latest_metrics = *metrics;
    }
    latest_valid = true;
    critical_section_exit(&shared_lock);
}

bool u4rk_pipeline_copy_latest_raw(
        uint16_t destination[U4RK_SAMPLE_COUNT]) {
    bool valid;
    critical_section_enter_blocking(&shared_lock);
    valid = latest_valid;
    if (valid) {
        memcpy(destination, latest_raw, sizeof(latest_raw));
    }
    critical_section_exit(&shared_lock);
    return valid;
}

void u4rk_pipeline_get_metrics(u4rk_dsp_metrics_t *metrics) {
    critical_section_enter_blocking(&shared_lock);
    *metrics = latest_metrics;
    critical_section_exit(&shared_lock);
}

bool u4rk_pipeline_take_output(uint8_t *slot, const uint8_t **data,
                               size_t *size, uint32_t *sequence,
                               uint32_t *session_id) {
    if (!queue_try_remove(&output_ready_queue, slot)) {
        return false;
    }
    *data = output_slots[*slot].bytes;
    *size = output_slots[*slot].size;
    *sequence = output_slots[*slot].sequence;
    *session_id = output_slots[*slot].session_id;
    return true;
}

void u4rk_pipeline_release_output(uint8_t slot) {
    queue_add_blocking(&output_free_queue, &slot);
}

bool u4rk_pipeline_has_pending_output(void) {
    return !queue_is_empty(&output_ready_queue);
}

bool u4rk_pipeline_take_completion(uint32_t *sequence) {
    return queue_try_remove(&completion_queue, sequence);
}

bool u4rk_pipeline_processing_idle(void) {
    return queue_get_level(&raw_free_queue) == U4RK_RAW_BUFFER_COUNT &&
           queue_is_empty(&job_queue);
}

static uint32_t payload_size_for(uint8_t payload_type) {
    switch ((u4rk_payload_type_t)payload_type) {
        case U4RK_PAYLOAD_RAW:
            return U4RK_SAMPLE_COUNT * sizeof(uint16_t);
        case U4RK_PAYLOAD_ENVELOPE:
            return U4RK_SAMPLE_COUNT * sizeof(float);
        case U4RK_PAYLOAD_ALAW:
            return U4RK_SAMPLE_COUNT;
        default:
            return 0;
    }
}

static void process_job(const u4rk_capture_job_t *job) {
    if (job->payload_type == U4RK_PAYLOAD_NONE) {
        u4rk_dsp_metrics_t metrics;
        memset(&metrics, 0, sizeof(metrics));
        metrics.dc_mean =
            u4rk_dsp_extract(raw_buffers[job->raw_index], raw_work);
        copy_latest(raw_work, &metrics);
        u4rk_pipeline_release_raw(job->raw_index);
        queue_try_add(&completion_queue, &job->sequence);
        return;
    }

    uint8_t output_index;
    if (!queue_try_remove(&output_free_queue, &output_index)) {
        u4rk_pipeline_note_processing_drop();
        u4rk_pipeline_release_raw(job->raw_index);
        return;
    }

    output_slot_t *slot = &output_slots[output_index];
    uint8_t *payload = slot->bytes + U4RK_HEADER_SIZE;
    u4rk_dsp_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    float *envelope = NULL;
    uint8_t *alaw = NULL;
    bool saturated = false;

    if (job->payload_type == U4RK_PAYLOAD_RAW) {
        metrics.dc_mean =
            u4rk_dsp_extract(raw_buffers[job->raw_index], raw_work);
        for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
            store_u16_le(payload + 2u * i, raw_work[i]);
        }
        copy_latest(raw_work, &metrics);
    } else {
        u4rk_dsp_envelope(
            raw_buffers[job->raw_index], raw_work, job->alaw_reference,
            job->payload_type == U4RK_PAYLOAD_ALAW,
            &envelope, &alaw, &saturated, &metrics);
        copy_latest(raw_work, &metrics);
        if (job->payload_type == U4RK_PAYLOAD_ENVELOPE) {
            memcpy(payload, envelope, U4RK_SAMPLE_COUNT * sizeof(float));
        } else {
            memcpy(payload, alaw, U4RK_SAMPLE_COUNT);
        }
    }

    uint32_t payload_size = payload_size_for(job->payload_type);
    uint16_t flags = job->flags;
    uint32_t processing_drop_count =
        __atomic_load_n(&processing_drops, __ATOMIC_RELAXED);
    uint32_t usb_drop_count =
        __atomic_load_n(&usb_drops, __ATOMIC_RELAXED);
    if (saturated) {
        flags |= U4RK_FLAG_ALAW_SATURATED;
    }
    if (processing_drop_count != 0u) {
        flags |= U4RK_FLAG_PROCESSING_DROP;
    }
    if (usb_drop_count != 0u) {
        flags |= U4RK_FLAG_USB_DROP;
    }

    u4rk_frame_header_t header = {
        .payload_type = job->payload_type,
        .flags = flags,
        .sequence = job->sequence,
        .sample_count = U4RK_SAMPLE_COUNT,
        .sample_rate_hz = job->sample_rate_hz,
        .payload_bytes = payload_size,
        .capture_timestamp_us = job->capture_timestamp_us,
        .adc_dc_mean = metrics.dc_mean,
        .envelope_peak = metrics.envelope_peak,
        .alaw_reference = job->alaw_reference,
        .pulse = job->pulse,
        .dropped_frames = processing_drop_count + usb_drop_count,
        .payload_crc32 = u4rk_crc32(payload, payload_size),
    };
    u4rk_serialize_header(slot->bytes, &header);
    slot->size = U4RK_HEADER_SIZE + payload_size;
    slot->sequence = job->sequence;
    slot->session_id = job->session_id;

    u4rk_pipeline_release_raw(job->raw_index);
    if (!queue_try_add(&output_ready_queue, &output_index)) {
        u4rk_pipeline_note_processing_drop();
        queue_add_blocking(&output_free_queue, &output_index);
    }
    queue_try_add(&completion_queue, &job->sequence);
}

void u4rk_pipeline_core1_entry(void) {
    while (true) {
        u4rk_capture_job_t job;
        queue_remove_blocking(&job_queue, &job);
        process_job(&job);
    }
}
