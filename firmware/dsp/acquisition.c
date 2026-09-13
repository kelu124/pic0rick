#include "acquisition.h"

#include <string.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "acquisition.pio.h"
#include "pulser.pio.h"

#define U4RK_ADC_PIO_INSTRUCTION_HZ 120000000.0f
#define U4RK_PULSE_PIO_INSTRUCTION_HZ 125000000.0f
#define U4RK_DMA_TIMEOUT_US 2000u
#define U4RK_PULSE_TICK_NS 8u
#define U4RK_PULSE_OVERHEAD_TICKS 5u

static PIO adc_pio;
static PIO pulser_pio;
static uint adc_sm;
static uint pulser_drive_sm;
static uint pulser_gate_sm;
static uint adc_offset;
static uint pulser_offset;
static uint dma_channel;
static dma_channel_config dma_config;
static bool pulser_armed;
static bool capture_active;
static uint64_t capture_started_us;
static u4rk_pulse_config_t pulse_config = {
    .negative_ns = 96,
    .damp_ns = 6000,
    .positive_ns = 96,
    .order = U4RK_PULSE_NEGATIVE_FIRST,
};

static uint32_t rounded_ticks(uint32_t duration_ns) {
    return (duration_ns + U4RK_PULSE_TICK_NS / 2u) / U4RK_PULSE_TICK_NS;
}

static void reset_pulser_sm(uint sm, uint pin_base) {
    pio_sm_set_enabled(pulser_pio, sm, false);
    pio_sm_clear_fifos(pulser_pio, sm);
    pio_sm_restart(pulser_pio, sm);
    pio_sm_exec(pulser_pio, sm, pio_encode_jmp(pulser_offset));
    pio_sm_set_pins_with_mask(pulser_pio, sm, 0u, 3u << pin_base);
}

static void force_pulser_idle(void) {
    reset_pulser_sm(pulser_drive_sm, U4RK_PULSER_DRIVE_PIN_BASE);
    reset_pulser_sm(pulser_gate_sm, U4RK_PULSER_GATE_PIN_BASE);
}

static void force_adc_clock_low(void) {
    pio_sm_set_enabled(adc_pio, adc_sm, false);
    pio_sm_set_pins_with_mask(
        adc_pio, adc_sm, 0u, 1u << U4RK_ADC_CLOCK_PIN);
}

static void initialize_safe_output(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, false);
}

void u4rk_acquisition_init(void) {
    /* Establish a safe all-low PMOD state before assigning any pin to PIO. */
    initialize_safe_output(U4RK_PULSER_PP_PIN);
    initialize_safe_output(U4RK_PULSER_PN_PIN);
    initialize_safe_output(U4RK_PULSER_PDAMP_PIN);
    initialize_safe_output(U4RK_PULSER_OE_PIN);

    adc_pio = pio0;
    pulser_pio = pio1;
    adc_sm = pio_claim_unused_sm(adc_pio, true);
    pulser_drive_sm = pio_claim_unused_sm(pulser_pio, true);
    pulser_gate_sm = pio_claim_unused_sm(pulser_pio, true);
    adc_offset = pio_add_program(adc_pio, &u4rk_adc_program);
    pulser_offset = pio_add_program(pulser_pio, &u4rk_pulser_program);
    u4rk_adc_program_init(adc_pio, adc_sm, adc_offset,
                          U4RK_ADC_PIO_INSTRUCTION_HZ);
    u4rk_pulser_program_init(
        pulser_pio, pulser_drive_sm, pulser_offset,
        U4RK_PULSE_PIO_INSTRUCTION_HZ, U4RK_PULSER_DRIVE_PIN_BASE);
    u4rk_pulser_program_init(
        pulser_pio, pulser_gate_sm, pulser_offset,
        U4RK_PULSE_PIO_INSTRUCTION_HZ, U4RK_PULSER_GATE_PIN_BASE);

    dma_channel = dma_claim_unused_channel(true);
    dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_dreq(
        &dma_config, pio_get_dreq(adc_pio, adc_sm, false));

    pulser_armed = false;
    capture_active = false;
    force_pulser_idle();
}

bool u4rk_pulser_configure(uint32_t negative_ns, uint32_t damp_ns,
                           uint32_t positive_ns, u4rk_pulse_order_t order) {
    uint32_t negative_ticks = rounded_ticks(negative_ns);
    uint32_t damp_ticks = rounded_ticks(damp_ns);
    uint32_t positive_ticks = rounded_ticks(positive_ns);

    if (negative_ticks < U4RK_PULSE_OVERHEAD_TICKS ||
        damp_ticks < U4RK_PULSE_OVERHEAD_TICKS ||
        positive_ticks < U4RK_PULSE_OVERHEAD_TICKS) {
        return false;
    }

    pulse_config.negative_ns = negative_ticks * U4RK_PULSE_TICK_NS;
    pulse_config.damp_ns = damp_ticks * U4RK_PULSE_TICK_NS;
    pulse_config.positive_ns = positive_ticks * U4RK_PULSE_TICK_NS;
    pulse_config.order = order;
    return true;
}

u4rk_pulse_config_t u4rk_pulser_get_config(void) {
    return pulse_config;
}

void u4rk_pulser_arm(void) {
    pulser_armed = true;
}

void u4rk_pulser_disarm(void) {
    pulser_armed = false;
    force_pulser_idle();
}

bool u4rk_pulser_is_armed(void) {
    return pulser_armed;
}

static void queue_state(uint sm, uint32_t state, uint32_t ticks) {
    pio_sm_put(pulser_pio, sm, state);
    pio_sm_put(pulser_pio, sm, ticks - U4RK_PULSE_OVERHEAD_TICKS);
}

static void queue_pulse(void) {
    uint32_t first_ticks;
    uint32_t last_ticks;
    uint32_t first_drive_state;
    uint32_t last_drive_state;
    if (pulse_config.order == U4RK_PULSE_NEGATIVE_FIRST) {
        first_ticks = pulse_config.negative_ns / U4RK_PULSE_TICK_NS;
        last_ticks = pulse_config.positive_ns / U4RK_PULSE_TICK_NS;
        first_drive_state = 2u; /* P-=1, P+=0 */
        last_drive_state = 1u;  /* P+=1, P-=0 */
    } else {
        first_ticks = pulse_config.positive_ns / U4RK_PULSE_TICK_NS;
        last_ticks = pulse_config.negative_ns / U4RK_PULSE_TICK_NS;
        first_drive_state = 1u;
        last_drive_state = 2u;
    }
    uint32_t damp_ticks = pulse_config.damp_ns / U4RK_PULSE_TICK_NS;

    queue_state(pulser_drive_sm, first_drive_state, first_ticks);
    queue_state(pulser_drive_sm, 0u, damp_ticks);
    queue_state(pulser_drive_sm, last_drive_state, last_ticks);

    queue_state(pulser_gate_sm, 2u, first_ticks); /* OE */
    queue_state(pulser_gate_sm, 3u, damp_ticks);  /* OE + PDAMP */
    queue_state(pulser_gate_sm, 2u, last_ticks);  /* OE */
}

bool u4rk_capture_start(uint16_t *destination) {
    if (capture_active || destination == NULL) {
        return false;
    }

    pio_sm_set_enabled(adc_pio, adc_sm, false);
    pio_sm_clear_fifos(adc_pio, adc_sm);
    pio_sm_restart(adc_pio, adc_sm);
    pio_sm_exec(adc_pio, adc_sm, pio_encode_jmp(adc_offset));
    force_pulser_idle();
    memset(destination, 0, U4RK_SAMPLE_COUNT * sizeof(*destination));

    dma_channel_configure(
        dma_channel, &dma_config, destination,
        &adc_pio->rxf[adc_sm], U4RK_SAMPLE_COUNT, false);

    /* x-- executes x+1 iterations, hence N-1 for exactly 4096 samples. */
    pio_sm_put(adc_pio, adc_sm, U4RK_SAMPLE_COUNT - 1u);
    if (pulser_armed) {
        queue_pulse();
    }

    dma_start_channel_mask(1u << dma_channel);
    if (pulser_armed) {
        uint32_t pulser_mask =
            (1u << pulser_drive_sm) | (1u << pulser_gate_sm);
        pio_enable_sm_mask_in_sync(pulser_pio, pulser_mask);
    }
    pio_sm_set_enabled(adc_pio, adc_sm, true);
    capture_started_us = time_us_64();
    capture_active = true;
    return true;
}

u4rk_capture_state_t u4rk_capture_poll(void) {
    if (!capture_active) {
        return U4RK_CAPTURE_IDLE;
    }
    if (!dma_channel_is_busy(dma_channel)) {
        pio_sm_set_enabled(adc_pio, adc_sm, false);
        capture_active = false;
        return U4RK_CAPTURE_DONE;
    }
    if ((time_us_64() - capture_started_us) > U4RK_DMA_TIMEOUT_US) {
        dma_channel_abort(dma_channel);
        force_adc_clock_low();
        capture_active = false;
        u4rk_pulser_disarm();
        return U4RK_CAPTURE_DMA_FAULT;
    }
    return U4RK_CAPTURE_ACTIVE;
}

void u4rk_capture_abort(void) {
    if (capture_active) {
        dma_channel_abort(dma_channel);
        force_adc_clock_low();
        capture_active = false;
    }
    u4rk_pulser_disarm();
}
