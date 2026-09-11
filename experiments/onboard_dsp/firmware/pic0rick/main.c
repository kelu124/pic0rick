#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "acquisition.h"
#include "dac.h"
#include "dsp.h"
#include "pipeline.h"
#include "u4rk.h"
#include "usb_transport.h"

#define U4RK_COMMAND_BUFFER_SIZE 160u
#define U4RK_RESPONSE_BUFFER_SIZE 512u
#define U4RK_CONTROL_TIMEOUT_MS 2000u

typedef struct {
    bool active;
    u4rk_payload_type_t type;
    uint32_t rate_hz;
    uint32_t interval_us;
    uint64_t next_capture_us;
} stream_state_t;

static stream_state_t stream;
static bool capture_inflight;
static uint8_t capture_raw_index;
static u4rk_capture_job_t capture_job;
static uint32_t next_sequence;
static float alaw_reference = U4RK_ALAW_DEFAULT_REFERENCE;

static char command_buffer[U4RK_COMMAND_BUFFER_SIZE];
static size_t command_length;
static char response_buffer[U4RK_RESPONSE_BUFFER_SIZE];

static bool output_slot_active;
static uint8_t active_output_slot;
static uint32_t active_output_sequence;
static uint32_t active_output_session;
static bool stop_pending;
static bool dma_fault_pending;
static bool legacy_capture_pending;
static uint32_t legacy_capture_sequence;

static bool selftest_active;
static bool selftest_frame_pending;
static uint32_t selftest_pending_sequence;
static uint8_t selftest_case;
static u4rk_payload_type_t selftest_type;

/* Jobs and output frames are tagged so work from a closed CDC session can
 * never be mistaken for work belonging to the next host connection. */
static uint32_t usb_session_id = 1u;
static bool usb_was_mounted;

static uint16_t legacy_read_buffer[U4RK_SAMPLE_COUNT];

static bool send_formatted(const char *prefix, const char *format, va_list args) {
    int used = snprintf(response_buffer, sizeof(response_buffer), "%s", prefix);
    if (used < 0 || (size_t)used >= sizeof(response_buffer)) {
        return false;
    }
    int added = vsnprintf(response_buffer + used, sizeof(response_buffer) - used,
                          format, args);
    if (added < 0) {
        return false;
    }
    size_t length = strnlen(response_buffer, sizeof(response_buffer));
    if (length + 2u >= sizeof(response_buffer)) {
        length = sizeof(response_buffer) - 3u;
    }
    response_buffer[length++] = '\r';
    response_buffer[length++] = '\n';
    return u4rk_usb_write_blocking(response_buffer, length,
                                    U4RK_CONTROL_TIMEOUT_MS);
}

static bool send_ok(const char *format, ...) {
    va_list args;
    va_start(args, format);
    bool result = send_formatted("OK ", format, args);
    va_end(args);
    return result;
}

static bool send_error(const char *code, const char *format, ...) {
    int prefix_length =
        snprintf(response_buffer, sizeof(response_buffer), "ERR %s ", code);
    if (prefix_length < 0 ||
        (size_t)prefix_length >= sizeof(response_buffer)) {
        return false;
    }
    va_list args;
    va_start(args, format);
    int added = vsnprintf(response_buffer + prefix_length,
                          sizeof(response_buffer) - (size_t)prefix_length,
                          format, args);
    va_end(args);
    if (added < 0) {
        return false;
    }
    size_t length = strnlen(response_buffer, sizeof(response_buffer));
    if (length + 2u >= sizeof(response_buffer)) {
        length = sizeof(response_buffer) - 3u;
    }
    response_buffer[length++] = '\r';
    response_buffer[length++] = '\n';
    return u4rk_usb_write_blocking(response_buffer, length,
                                    U4RK_CONTROL_TIMEOUT_MS);
}

static bool parse_u32(const char *text, uint32_t *value) {
    if (text == NULL || *text == '\0' || *text == '-') {
        return false;
    }
    char *end;
    unsigned long parsed = strtoul(text, &end, 10);
    if (*end != '\0' || parsed > UINT32_MAX) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool parse_float(const char *text, float *value) {
    if (text == NULL || *text == '\0') {
        return false;
    }
    char *end;
    float parsed = strtof(text, &end);
    if (*end != '\0' || !isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parse_payload_type(const char *text,
                               u4rk_payload_type_t *type) {
    if (text == NULL) {
        return false;
    }
    if (strcmp(text, "raw") == 0) {
        *type = U4RK_PAYLOAD_RAW;
    } else if (strcmp(text, "envelope") == 0) {
        *type = U4RK_PAYLOAD_ENVELOPE;
    } else if (strcmp(text, "alaw") == 0) {
        *type = U4RK_PAYLOAD_ALAW;
    } else {
        return false;
    }
    return true;
}

static uint32_t maximum_rate(u4rk_payload_type_t type) {
    switch (type) {
        case U4RK_PAYLOAD_RAW:
            return U4RK_RAW_MAX_RATE_HZ;
        case U4RK_PAYLOAD_ENVELOPE:
            return U4RK_ENVELOPE_MAX_RATE_HZ;
        case U4RK_PAYLOAD_ALAW:
            return U4RK_ALAW_MAX_RATE_HZ;
        default:
            return 0u;
    }
}

static bool operation_busy(void) {
    return capture_inflight || legacy_capture_pending || selftest_active ||
           output_slot_active || u4rk_usb_tx_busy() ||
           u4rk_pipeline_has_pending_output() ||
           !u4rk_pipeline_processing_idle();
}

static bool begin_capture(u4rk_payload_type_t type, uint16_t extra_flags) {
    uint16_t *raw_buffer;
    uint8_t raw_index;
    if (!u4rk_pipeline_claim_raw(&raw_index, &raw_buffer)) {
        return false;
    }

    uint16_t flags = extra_flags;
    if (u4rk_pulser_is_armed()) {
        flags |= U4RK_FLAG_PULSER_ARMED;
    }
    capture_job = (u4rk_capture_job_t){
        .raw_index = raw_index,
        .payload_type = (uint8_t)type,
        .flags = flags,
        .sequence = next_sequence++,
        .session_id = usb_session_id,
        .sample_rate_hz = U4RK_SAMPLE_RATE_HZ,
        .capture_timestamp_us = time_us_64(),
        .alaw_reference = alaw_reference,
        .pulse = u4rk_pulser_get_config(),
    };
    if (!u4rk_capture_start(raw_buffer)) {
        u4rk_pipeline_release_raw(raw_index);
        return false;
    }
    capture_raw_index = raw_index;
    capture_inflight = true;
    return true;
}

static void poll_capture(void) {
    if (!capture_inflight) {
        return;
    }
    u4rk_capture_state_t state = u4rk_capture_poll();
    if (state == U4RK_CAPTURE_DONE) {
        capture_inflight = false;
        u4rk_pipeline_submit(&capture_job);
    } else if (state == U4RK_CAPTURE_DMA_FAULT) {
        capture_inflight = false;
        u4rk_pipeline_release_raw(capture_raw_index);
        stream.active = false;
        dma_fault_pending = true;
    }
}

static void drain_ready_outputs_as_drops(void) {
    uint8_t slot;
    const uint8_t *data;
    size_t size;
    uint32_t sequence;
    uint32_t session_id;
    while (u4rk_pipeline_take_output(
            &slot, &data, &size, &sequence, &session_id)) {
        (void)data;
        (void)size;
        if (selftest_frame_pending && session_id == usb_session_id &&
            sequence == selftest_pending_sequence) {
            selftest_active = false;
            selftest_frame_pending = false;
        }
        u4rk_pipeline_release_output(slot);
        u4rk_pipeline_note_usb_drop();
    }
}

static void advance_selftest(void) {
    selftest_frame_pending = false;
    if (selftest_type == U4RK_PAYLOAD_ALAW) {
        selftest_type = U4RK_PAYLOAD_RAW;
        ++selftest_case;
        if (selftest_case >= u4rk_dsp_selftest_count()) {
            selftest_active = false;
        }
    } else {
        selftest_type =
            (u4rk_payload_type_t)((uint8_t)selftest_type + 1u);
    }
}

static void poll_output(void) {
    if (output_slot_active && !u4rk_usb_tx_busy()) {
        bool failed = u4rk_usb_tx_take_failed();
        u4rk_pipeline_release_output(active_output_slot);
        output_slot_active = false;
        bool was_pending_selftest = selftest_frame_pending &&
            active_output_session == usb_session_id &&
            active_output_sequence == selftest_pending_sequence;
        if (failed) {
            u4rk_pipeline_note_usb_drop();
            if (was_pending_selftest) {
                selftest_active = false;
                selftest_frame_pending = false;
            }
        } else if (was_pending_selftest) {
            advance_selftest();
        }
    }

    if (!output_slot_active && !u4rk_usb_tx_busy() &&
        !stop_pending && !dma_fault_pending) {
        const uint8_t *data;
        size_t size;
        uint8_t slot;
        uint32_t sequence;
        uint32_t session_id;
        while (u4rk_pipeline_take_output(
                &slot, &data, &size, &sequence, &session_id)) {
            if (session_id != usb_session_id) {
                /* Core 1 may finish an old job after its CDC session closes. */
                u4rk_pipeline_release_output(slot);
                u4rk_pipeline_note_usb_drop();
                continue;
            }
            if (u4rk_usb_tx_start(data, size)) {
                output_slot_active = true;
                active_output_slot = slot;
                active_output_sequence = sequence;
                active_output_session = session_id;
            } else {
                u4rk_pipeline_release_output(slot);
                u4rk_pipeline_note_usb_drop();
                /* Never leave a failed self-test transfer permanently busy. */
                if (selftest_frame_pending &&
                    sequence == selftest_pending_sequence) {
                    selftest_active = false;
                    selftest_frame_pending = false;
                }
            }
            break;
        }
    }
}

static void poll_completions(void) {
    uint32_t sequence;
    while (u4rk_pipeline_take_completion(&sequence)) {
        if (legacy_capture_pending && sequence == legacy_capture_sequence) {
            legacy_capture_pending = false;
        }
    }
}

static void schedule_stream(void) {
    if (!stream.active || capture_inflight) {
        return;
    }
    uint64_t now = time_us_64();
    if (now < stream.next_capture_us) {
        return;
    }
    if (!begin_capture(stream.type, 0)) {
        u4rk_pipeline_note_processing_drop();
    }
    stream.next_capture_us += stream.interval_us;
    if (stream.next_capture_us <= now) {
        stream.next_capture_us = now + stream.interval_us;
    }
}

static void schedule_selftest(void) {
    if (!selftest_active || selftest_frame_pending || capture_inflight ||
        output_slot_active || u4rk_usb_tx_busy() ||
        u4rk_pipeline_has_pending_output()) {
        return;
    }

    uint8_t raw_index;
    uint16_t *raw_buffer;
    if (!u4rk_pipeline_claim_raw(&raw_index, &raw_buffer)) {
        return;
    }
    u4rk_dsp_make_selftest(selftest_case, raw_buffer);
    float reference =
        selftest_case == (u4rk_dsp_selftest_count() - 1u)
            ? 128.0f : alaw_reference;
    u4rk_capture_job_t job = {
        .raw_index = raw_index,
        .payload_type = (uint8_t)selftest_type,
        .flags = (uint16_t)(U4RK_FLAG_SELFTEST |
            ((uint16_t)selftest_case << U4RK_FLAG_SELFTEST_CASE_SHIFT)),
        .sequence = next_sequence++,
        .session_id = usb_session_id,
        .sample_rate_hz = U4RK_SAMPLE_RATE_HZ,
        .capture_timestamp_us = selftest_case,
        .alaw_reference = reference,
        .pulse = u4rk_pulser_get_config(),
    };
    if (u4rk_pipeline_submit(&job)) {
        selftest_pending_sequence = job.sequence;
        selftest_frame_pending = true;
    }
}

static void stop_stream(void) {
    stream.active = false;
    u4rk_pulser_disarm();
    if (capture_inflight) {
        u4rk_capture_abort();
        capture_inflight = false;
        u4rk_pipeline_release_raw(capture_raw_index);
        u4rk_pipeline_note_processing_drop();
    }
    drain_ready_outputs_as_drops();
    stop_pending = true;
}

static void send_help(void) {
    send_ok(
        "commands=status|help|pulser arm|pulser disarm|"
        "pulse config <negative_ns> <damp_ns> <positive_ns> "
        "<neg-first|pos-first>|dac write <0..1023>|"
        "dsp scale <reference>|dsp selftest|"
        "acq <raw|envelope|alaw>|"
        "stream start <raw|envelope|alaw> <rate_hz>|stream stop|"
        "start acq|read");
}

static void send_status(void) {
    u4rk_pulse_config_t pulse = u4rk_pulser_get_config();
    u4rk_dsp_metrics_t metrics;
    u4rk_pipeline_get_metrics(&metrics);
    send_ok(
        "board=pic0rick package=RP2350A firmware=%s "
        "dsp_backend=f32-rfft-hilbert "
        "samples=%u sample_rate=%u "
        "pulser=%s pulse=%u/%u/%u/%s dac=%u scale=%.6g "
        "stream=%s/%u drops=%u stages_us=%u/%u/%u/%u/%u/%u "
        "dsp_us=%u worst_us=%u performance=%s "
        "envelope_max_rate=%u alaw_max_rate=%u cmsis=%s",
        PICO_PROGRAM_VERSION_STRING,
        U4RK_SAMPLE_COUNT, U4RK_SAMPLE_RATE_HZ,
        u4rk_pulser_is_armed() ? "armed" : "disarmed",
        pulse.negative_ns, pulse.damp_ns, pulse.positive_ns,
        pulse.order == U4RK_PULSE_NEGATIVE_FIRST ? "neg-first" : "pos-first",
        u4rk_dac_last_value(), (double)alaw_reference,
        stream.active ? "on" : "off", stream.rate_hz,
        u4rk_pipeline_dropped_frames(),
        metrics.preprocess_us, metrics.forward_fft_us, metrics.mask_us,
        metrics.inverse_fft_us, metrics.magnitude_us, metrics.alaw_us,
        metrics.total_us, metrics.worst_total_us,
        metrics.worst_total_us <= U4RK_DSP_TARGET_US
            ? "ok" : "over-budget",
        U4RK_ENVELOPE_MAX_RATE_HZ, U4RK_ALAW_MAX_RATE_HZ,
        U4RK_CMSIS_DSP_VERSION);
}

static void legacy_read(void) {
    if (!u4rk_pipeline_copy_latest_raw(legacy_read_buffer)) {
        send_error("NO_DATA", "no completed acquisition");
        return;
    }
    static const char prefix[] = "OK raw-hex 4096\r\n";
    if (!u4rk_usb_write_blocking(prefix, sizeof(prefix) - 1u,
                                 U4RK_CONTROL_TIMEOUT_MS)) {
        return;
    }
    char chunk[192];
    size_t used = 0;
    for (uint32_t i = 0; i < U4RK_SAMPLE_COUNT; ++i) {
        int count = snprintf(chunk + used, sizeof(chunk) - used, "%03X%s",
                             legacy_read_buffer[i],
                             i + 1u == U4RK_SAMPLE_COUNT ? "\r\n" : ",");
        if (count < 0) {
            return;
        }
        used += (size_t)count;
        if (used > sizeof(chunk) - 8u || i + 1u == U4RK_SAMPLE_COUNT) {
            if (!u4rk_usb_write_blocking(chunk, used,
                                         U4RK_CONTROL_TIMEOUT_MS)) {
                return;
            }
            used = 0;
        }
    }
}

static void process_command(char *line) {
    char *save;
    char *first = strtok_r(line, " \t", &save);
    if (first == NULL) {
        return;
    }
    char *second = strtok_r(NULL, " \t", &save);

    if (stream.active) {
        if (strcmp(first, "stream") == 0 && second != NULL &&
            strcmp(second, "stop") == 0) {
            stop_stream();
        }
        /* No unframed text is emitted while a stream is active. */
        return;
    }

    if (strcmp(first, "help") == 0 && second == NULL) {
        send_help();
        return;
    }
    if (strcmp(first, "status") == 0 && second == NULL) {
        send_status();
        return;
    }

    if (strcmp(first, "pulser") == 0 && second != NULL) {
        if (operation_busy()) {
            send_error("BUSY", "operation in progress");
        } else if (strcmp(second, "arm") == 0) {
            u4rk_pulser_arm();
            send_ok("pulser armed");
        } else if (strcmp(second, "disarm") == 0) {
            u4rk_pulser_disarm();
            send_ok("pulser disarmed");
        } else {
            send_error("ARG", "expected arm or disarm");
        }
        return;
    }

    if (strcmp(first, "pulse") == 0 && second != NULL &&
        strcmp(second, "config") == 0) {
        if (operation_busy()) {
            send_error("BUSY", "operation in progress");
            return;
        }
        char *negative_text = strtok_r(NULL, " \t", &save);
        char *damp_text = strtok_r(NULL, " \t", &save);
        char *positive_text = strtok_r(NULL, " \t", &save);
        char *order_text = strtok_r(NULL, " \t", &save);
        char *extra = strtok_r(NULL, " \t", &save);
        uint32_t negative_ns, damp_ns, positive_ns;
        u4rk_pulse_order_t order;
        if (!parse_u32(negative_text, &negative_ns) ||
            !parse_u32(damp_text, &damp_ns) ||
            !parse_u32(positive_text, &positive_ns) ||
            order_text == NULL || extra != NULL) {
            send_error("ARG", "invalid pulse configuration");
            return;
        }
        if (strcmp(order_text, "neg-first") == 0) {
            order = U4RK_PULSE_NEGATIVE_FIRST;
        } else if (strcmp(order_text, "pos-first") == 0) {
            order = U4RK_PULSE_POSITIVE_FIRST;
        } else {
            send_error("ARG", "order must be neg-first or pos-first");
            return;
        }
        if (!u4rk_pulser_configure(
                negative_ns, damp_ns, positive_ns, order)) {
            send_error("RANGE", "minimum is 40/40/40 ns");
            return;
        }
        u4rk_pulse_config_t actual = u4rk_pulser_get_config();
        send_ok("pulse=%u/%u/%u/%s", actual.negative_ns, actual.damp_ns,
                actual.positive_ns,
                actual.order == U4RK_PULSE_NEGATIVE_FIRST
                    ? "neg-first" : "pos-first");
        return;
    }

    if (strcmp(first, "dac") == 0 && second != NULL &&
        strcmp(second, "write") == 0) {
        char *value_text = strtok_r(NULL, " \t", &save);
        char *extra = strtok_r(NULL, " \t", &save);
        uint32_t value;
        if (operation_busy()) {
            send_error("BUSY", "operation in progress");
        } else if (!parse_u32(value_text, &value) ||
                   value > 1023u || extra != NULL) {
            send_error("RANGE", "DAC value must be 0..1023");
        } else {
            u4rk_dac_write((uint16_t)value);
            send_ok("dac=%u", value);
        }
        return;
    }

    if (strcmp(first, "dsp") == 0 && second != NULL) {
        if (strcmp(second, "scale") == 0) {
            char *reference_text = strtok_r(NULL, " \t", &save);
            char *extra = strtok_r(NULL, " \t", &save);
            float reference;
            if (operation_busy()) {
                send_error("BUSY", "operation in progress");
            } else if (!parse_float(reference_text, &reference) ||
                       reference <= 0.0f || reference > 65535.0f ||
                       extra != NULL) {
                send_error("RANGE", "reference must be in (0,65535]");
            } else {
                alaw_reference = reference;
                send_ok("scale=%.6g", (double)alaw_reference);
            }
        } else if (strcmp(second, "selftest") == 0) {
            if (operation_busy()) {
                send_error("BUSY", "operation in progress");
            } else {
                selftest_active = true;
                selftest_frame_pending = false;
                selftest_case = 0;
                selftest_type = U4RK_PAYLOAD_RAW;
                send_ok("selftest frames=%u cases=%u",
                        (unsigned)u4rk_dsp_selftest_count() * 3u,
                        u4rk_dsp_selftest_count());
            }
        } else {
            send_error("ARG", "expected scale or selftest");
        }
        return;
    }

    if (strcmp(first, "acq") == 0 && second != NULL) {
        u4rk_payload_type_t type;
        if (operation_busy()) {
            send_error("BUSY", "operation in progress");
        } else if (!parse_payload_type(second, &type)) {
            send_error("ARG", "type must be raw, envelope, or alaw");
        } else if (!begin_capture(type, 0)) {
            send_error("BUSY", "no acquisition buffer");
        } else {
            send_ok("capture started type=%s", second);
        }
        return;
    }

    if (strcmp(first, "stream") == 0 && second != NULL &&
        strcmp(second, "start") == 0) {
        char *type_text = strtok_r(NULL, " \t", &save);
        char *rate_text = strtok_r(NULL, " \t", &save);
        char *extra = strtok_r(NULL, " \t", &save);
        u4rk_payload_type_t type;
        uint32_t rate;
        if (operation_busy()) {
            send_error("BUSY", "operation in progress");
        } else if (!parse_payload_type(type_text, &type) ||
                   !parse_u32(rate_text, &rate) || extra != NULL) {
            send_error("ARG", "expected type and integer rate");
        } else if (rate < 1u || rate > maximum_rate(type)) {
            send_error("RATE", "allowed rate is 1..%u Hz",
                       maximum_rate(type));
        } else {
            send_ok("stream started type=%s rate=%u", type_text, rate);
            stream.active = true;
            stream.type = type;
            stream.rate_hz = rate;
            stream.interval_us = 1000000u / rate;
            stream.next_capture_us = time_us_64();
        }
        return;
    }

    if (strcmp(first, "stream") == 0 && second != NULL &&
        strcmp(second, "stop") == 0) {
        send_ok("stream already stopped");
        return;
    }

    if (strcmp(first, "start") == 0 && second != NULL &&
        strcmp(second, "acq") == 0) {
        if (operation_busy()) {
            send_error("BUSY", "operation in progress");
        } else if (!begin_capture(U4RK_PAYLOAD_NONE, 0)) {
            send_error("BUSY", "no acquisition buffer");
        } else {
            legacy_capture_pending = true;
            legacy_capture_sequence = capture_job.sequence;
            send_ok("legacy acquisition started");
        }
        return;
    }

    if (strcmp(first, "read") == 0 && second == NULL) {
        if (operation_busy()) {
            send_error("BUSY", "operation in progress");
        } else {
            legacy_read();
        }
        return;
    }

    send_error("COMMAND", "unknown command");
}

static void poll_command_input(void) {
    /*
     * During a stream, input is still consumed so "stream stop" always works.
     * During a one-shot binary response, defer parsing to avoid mixing text
     * into a frame already in flight.
     */
    if (!stream.active &&
        (output_slot_active || u4rk_usb_tx_busy() ||
         u4rk_pipeline_has_pending_output())) {
        return;
    }

    int character;
    while ((character = u4rk_usb_read_char()) >= 0) {
        if (character == '\r' || character == '\n') {
            if (command_length != 0u) {
                command_buffer[command_length] = '\0';
                process_command(command_buffer);
                command_length = 0;
            }
        } else if ((character == '\b' || character == 127) &&
                   command_length != 0u) {
            --command_length;
        } else if (character >= 32 && character <= 126) {
            if (command_length + 1u < sizeof(command_buffer)) {
                command_buffer[command_length++] = (char)character;
            } else {
                command_length = 0;
                if (!stream.active) {
                    send_error("LINE", "command too long");
                }
            }
        }
    }
}

static void finish_stop_or_fault(void) {
    /* This cleanup path is destructive: it deliberately discards every
     * completed output frame.  Run it only while finishing an explicit
     * stream stop or reporting a DMA fault.  Without this guard, a Core 1
     * result that became ready just after poll_output() was silently drained
     * during normal acquisition and self-test operation. */
    if (!stop_pending && !dma_fault_pending) {
        return;
    }
    if (output_slot_active || u4rk_usb_tx_busy()) {
        return;
    }
    drain_ready_outputs_as_drops();
    if (!u4rk_pipeline_processing_idle()) {
        return;
    }
    /* A just-finished DSP job may have populated the ready queue. */
    drain_ready_outputs_as_drops();
    if (stop_pending) {
        stop_pending = false;
        send_ok("stream stopped drops=%u",
                u4rk_pipeline_dropped_frames());
    } else if (dma_fault_pending) {
        dma_fault_pending = false;
        send_error("DMA", "acquisition timeout; pulser disarmed");
    }
}

static void cancel_usb_session(void) {
    /* Invalidate queued/in-flight Core 1 jobs before releasing USB slots. */
    ++usb_session_id;
    if (usb_session_id == 0u) {
        usb_session_id = 1u;
    }
    stream.active = false;
    stop_pending = false;
    dma_fault_pending = false;
    selftest_active = false;
    selftest_frame_pending = false;
    legacy_capture_pending = false;
    command_length = 0;
    u4rk_capture_abort();
    if (capture_inflight) {
        u4rk_pipeline_release_raw(capture_raw_index);
        capture_inflight = false;
    }
    u4rk_usb_tx_cancel();
    if (output_slot_active) {
        u4rk_pipeline_release_output(active_output_slot);
        output_slot_active = false;
        u4rk_pipeline_note_usb_drop();
    }
    drain_ready_outputs_as_drops();
}

static void handle_disconnect(void) {
    bool mounted = u4rk_usb_mounted();
    if (usb_was_mounted && !mounted) {
        cancel_usb_session();
    }
    usb_was_mounted = mounted;
}

int main(void) {
    bi_decl(bi_program_description(
        "pic0rick RP2350A 4096-sample Hilbert envelope and A-law firmware"));
    bi_decl(bi_pin_mask_with_name(0x7ffu, "ADC clock GPIO0 and data GPIO1..10"));
    bi_decl(bi_3pins_with_names(
        U4RK_DAC_CS_PIN, "MCP4812 CS",
        U4RK_DAC_SCK_PIN, "MCP4812 SCK",
        U4RK_DAC_TX_PIN, "MCP4812 MOSI"));
    bi_decl(bi_2pins_with_names(
        U4RK_PULSER_PP_PIN, "PMOD pulser P+",
        U4RK_PULSER_PN_PIN, "PMOD pulser P-"));
    bi_decl(bi_2pins_with_names(
        U4RK_PULSER_PDAMP_PIN, "PMOD pulser PDAMP",
        U4RK_PULSER_OE_PIN, "PMOD pulser OE"));

    /*
     * Pulser safety comes first: all four control pins are driven low before
     * DSP-table generation, USB enumeration, or any command handling.
     */
    u4rk_acquisition_init();
    u4rk_dac_init();
    if (!u4rk_pipeline_init()) {
        u4rk_pulser_disarm();
        while (true) {
            tight_loop_contents();
        }
    }
    multicore_launch_core1(u4rk_pipeline_core1_entry);
    if (!u4rk_usb_init()) {
        u4rk_pulser_disarm();
        while (true) {
            tight_loop_contents();
        }
    }

    while (true) {
        u4rk_usb_task();
        handle_disconnect();
        poll_capture();
        poll_completions();
        poll_output();
        finish_stop_or_fault();
        poll_command_input();
        schedule_stream();
        schedule_selftest();
        tight_loop_contents();
    }
}
