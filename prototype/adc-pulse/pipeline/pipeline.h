//!
//! \file       pipeline.h
//! \author     Abdelrahman Ali
//! \date       2025-01-21
//!
//! \brief      Parallel processing pipeline for continuous acquisition and storage.
//!

#ifndef PIPELINE_H
#define PIPELINE_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "adc/adc.h"
#include "dsp/signal_proc.h"
#include "sdio/sdio.h"

//---------------------------------------------------------------------------
// CONSTANTS
//---------------------------------------------------------------------------

#define PIPELINE_MAX_BUFFERS       4       // Number of ping-pong buffers
#define PIPELINE_BUFFER_SIZE_KB    128     // Size of each buffer in KB
#define PIPELINE_BUFFER_SIZE       (PIPELINE_BUFFER_SIZE_KB * 1024)
#define PIPELINE_ACQUISITION_US    30      // 30μs acquisition cycles
#define PIPELINE_TARGET_RATE_MSPS  12      // Target 12 Msps output
#define PIPELINE_MAX_ITERATIONS    1000    // For stress testing

// Pipeline states
#define PIPELINE_STATE_IDLE        0
#define PIPELINE_STATE_RUNNING     1
#define PIPELINE_STATE_STOPPING   2
#define PIPELINE_STATE_ERROR       3

//---------------------------------------------------------------------------
// STRUCTURES
//---------------------------------------------------------------------------

typedef struct {
    uint8_t* data;                 // Buffer data
    uint32_t size;                 // Current data size
    uint32_t capacity;             // Maximum capacity
    bool ready_for_write;          // Ready to write to SD
    bool write_in_progress;        // Currently being written
    absolute_time_t created_time;  // When buffer was created
} pipeline_buffer_t;

typedef struct {
    // Configuration
    uint16_t acquisition_samples;   // Samples per 30μs acquisition
    uint16_t decimation_factor;     // DSP decimation factor
    uint32_t target_output_rate;    // Target output sample rate
    bool continuous_mode;           // Run continuously
    uint32_t max_iterations;        // Max acquisitions (0 = infinite)
    
    // State
    uint8_t state;                  // Current pipeline state
    uint32_t iteration_count;       // Current iteration
    uint32_t sd_block_counter;      // SD card block address counter
    
    // Buffers
    pipeline_buffer_t buffers[PIPELINE_MAX_BUFFERS];
    uint8_t current_buffer;         // Currently filling buffer
    uint8_t write_buffer;           // Buffer being written to SD
    
    // Statistics
    uint32_t total_acquisitions;
    uint32_t total_samples_processed;
    uint32_t total_bytes_written;
    uint32_t acquisition_overruns;
    uint32_t write_errors;
    absolute_time_t start_time;
    
    // Timing
    struct repeating_timer acquisition_timer;
    bool timer_active;
    
    // DSP configuration
    dsp_config_t dsp_config;
    
} pipeline_state_t;

//---------------------------------------------------------------------------
// FUNCTION DECLARATIONS
//---------------------------------------------------------------------------

// Initialization and configuration
bool pipeline_init(void);
bool pipeline_configure(uint16_t acquisition_samples, uint16_t decimation, 
                       uint32_t target_rate, bool continuous);
void pipeline_deinit(void);

// Control functions
bool pipeline_start(uint32_t max_iterations);
void pipeline_stop(void);
bool pipeline_is_running(void);
uint8_t pipeline_get_state(void);

// Buffer management
bool pipeline_allocate_buffers(void);
void pipeline_free_buffers(void);
pipeline_buffer_t* pipeline_get_current_buffer(void);
pipeline_buffer_t* pipeline_get_write_ready_buffer(void);
void pipeline_swap_buffers(void);

// Core processing functions (called by timer interrupt)
bool pipeline_acquisition_callback(struct repeating_timer *t);
bool pipeline_process_acquisition(const uint16_t* raw_data, uint16_t sample_count);
bool pipeline_add_to_buffer(const uint8_t* processed_data, uint16_t data_size);

// Background SD writing (runs on core 1)
void pipeline_sd_writer_core1(void);
bool pipeline_write_buffer_to_sd(pipeline_buffer_t* buffer);

// Statistics and monitoring
void pipeline_print_status(void);
void pipeline_print_statistics(void);
uint32_t pipeline_get_throughput_sps(void);
uint32_t pipeline_get_write_speed_mbps(void);
float pipeline_get_compression_ratio(void);

// Advanced control
bool pipeline_adjust_timing(uint32_t new_acquisition_us);
bool pipeline_pause_resume(bool pause);
void pipeline_flush_buffers(void);

// Stress testing
bool pipeline_stress_test(uint32_t duration_seconds, bool print_progress);

//---------------------------------------------------------------------------
// INLINE HELPER FUNCTIONS
//---------------------------------------------------------------------------

// Check if buffer is full
static inline bool pipeline_buffer_is_full(pipeline_buffer_t* buffer) {
    return buffer->size >= buffer->capacity;
}

// Get free space in buffer
static inline uint32_t pipeline_buffer_free_space(pipeline_buffer_t* buffer) {
    return buffer->capacity - buffer->size;
}

// Calculate expected samples per acquisition based on timing
static inline uint16_t pipeline_calculate_samples_per_acquisition(uint32_t acquisition_us, uint32_t adc_freq_hz) {
    return (uint16_t)((acquisition_us * adc_freq_hz) / 1000000UL);
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------

#endif // PIPELINE_H
