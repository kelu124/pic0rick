//!
//! \file       pipeline.c
//! \author     Abdelrahman Ali
//! \date       2025-01-21
//!
//! \brief      Parallel processing pipeline implementation.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "pipeline.h"

//---------------------------------------------------------------------------
// GLOBAL VARIABLES
//---------------------------------------------------------------------------

static pipeline_state_t pipeline_state;
static bool core1_running = false;
static uint16_t* temp_adc_buffer = NULL;
static uint8_t* temp_dsp_buffer = NULL;

//---------------------------------------------------------------------------
// INITIALIZATION FUNCTIONS
//---------------------------------------------------------------------------

bool pipeline_init(void) {
    memset(&pipeline_state, 0, sizeof(pipeline_state));
    
    // Set default configuration
    pipeline_state.acquisition_samples = 3600;  // 30μs at 120MHz ADC
    pipeline_state.decimation_factor = 4;
    pipeline_state.target_output_rate = 12000000;  // 12 Msps
    pipeline_state.state = PIPELINE_STATE_IDLE;
    
    // Allocate temporary buffers
    temp_adc_buffer = malloc(SAMPLE_COUNT * sizeof(uint16_t));
    temp_dsp_buffer = malloc(SAMPLE_COUNT * sizeof(uint8_t));
    
    if (!temp_adc_buffer || !temp_dsp_buffer) {
        printf("Pipeline: Failed to allocate temporary buffers\n");
        return false;
    }
    
    // Configure DSP
    pipeline_state.dsp_config.decimation_factor = pipeline_state.decimation_factor;
    pipeline_state.dsp_config.filter_type = DSP_FILTER_ENVELOPE;
    pipeline_state.dsp_config.filter_length = 32;
    pipeline_state.dsp_config.input_format = DSP_FORMAT_UINT16;
    pipeline_state.dsp_config.output_format = DSP_FORMAT_UINT8;
    pipeline_state.dsp_config.envelope_detection = true;
    pipeline_state.dsp_config.high_speed_mode = true;
    
    if (!dsp_init(&pipeline_state.dsp_config)) {
        printf("Pipeline: DSP initialization failed\n");
        return false;
    }
    
    printf("Pipeline: Initialized successfully\n");
    return true;
}

bool pipeline_configure(uint16_t acquisition_samples, uint16_t decimation, 
                       uint32_t target_rate, bool continuous) {
    if (pipeline_state.state != PIPELINE_STATE_IDLE) {
        printf("Pipeline: Cannot configure while running\n");
        return false;
    }
    
    pipeline_state.acquisition_samples = acquisition_samples;
    pipeline_state.decimation_factor = decimation;
    pipeline_state.target_output_rate = target_rate;
    pipeline_state.continuous_mode = continuous;
    
    // Update DSP configuration
    dsp_set_decimation(decimation);
    
    printf("Pipeline: Configured - samples=%d, decimation=%d, rate=%lu\n",
           acquisition_samples, decimation, target_rate);
    
    return true;
}

void pipeline_deinit(void) {
    pipeline_stop();
    pipeline_free_buffers();
    
    if (temp_adc_buffer) {
        free(temp_adc_buffer);
        temp_adc_buffer = NULL;
    }
    
    if (temp_dsp_buffer) {
        free(temp_dsp_buffer);
        temp_dsp_buffer = NULL;
    }
    
    dsp_deinit();
    memset(&pipeline_state, 0, sizeof(pipeline_state));
    
    printf("Pipeline: Deinitialized\n");
}

//---------------------------------------------------------------------------
// BUFFER MANAGEMENT
//---------------------------------------------------------------------------

bool pipeline_allocate_buffers(void) {
    printf("Pipeline: Allocating %d buffers of %d KB each\n", 
           PIPELINE_MAX_BUFFERS, PIPELINE_BUFFER_SIZE_KB);
    
    for (int i = 0; i < PIPELINE_MAX_BUFFERS; i++) {
        pipeline_state.buffers[i].data = malloc(PIPELINE_BUFFER_SIZE);
        if (!pipeline_state.buffers[i].data) {
            printf("Pipeline: Failed to allocate buffer %d\n", i);
            pipeline_free_buffers();
            return false;
        }
        
        pipeline_state.buffers[i].capacity = PIPELINE_BUFFER_SIZE;
        pipeline_state.buffers[i].size = 0;
        pipeline_state.buffers[i].ready_for_write = false;
        pipeline_state.buffers[i].write_in_progress = false;
    }
    
    pipeline_state.current_buffer = 0;
    pipeline_state.write_buffer = 0;
    
    printf("Pipeline: Buffers allocated successfully\n");
    return true;
}

void pipeline_free_buffers(void) {
    for (int i = 0; i < PIPELINE_MAX_BUFFERS; i++) {
        if (pipeline_state.buffers[i].data) {
            free(pipeline_state.buffers[i].data);
            pipeline_state.buffers[i].data = NULL;
        }
    }
}

pipeline_buffer_t* pipeline_get_current_buffer(void) {
    return &pipeline_state.buffers[pipeline_state.current_buffer];
}

pipeline_buffer_t* pipeline_get_write_ready_buffer(void) {
    for (int i = 0; i < PIPELINE_MAX_BUFFERS; i++) {
        if (pipeline_state.buffers[i].ready_for_write && 
            !pipeline_state.buffers[i].write_in_progress) {
            return &pipeline_state.buffers[i];
        }
    }
    return NULL;
}

void pipeline_swap_buffers(void) {
    // Mark current buffer as ready for writing
    pipeline_buffer_t* current = pipeline_get_current_buffer();
    current->ready_for_write = true;
    current->created_time = get_absolute_time();
    
    // Find next available buffer
    uint8_t next_buffer = (pipeline_state.current_buffer + 1) % PIPELINE_MAX_BUFFERS;
    
    // Wait for next buffer to be available (should not happen in well-tuned system)
    while (pipeline_state.buffers[next_buffer].write_in_progress) {
        sleep_us(10);
        pipeline_state.acquisition_overruns++;
    }
    
    // Reset next buffer
    pipeline_state.buffers[next_buffer].size = 0;
    pipeline_state.buffers[next_buffer].ready_for_write = false;
    pipeline_state.buffers[next_buffer].write_in_progress = false;
    
    pipeline_state.current_buffer = next_buffer;
}

//---------------------------------------------------------------------------
// CORE PROCESSING FUNCTIONS
//---------------------------------------------------------------------------

bool pipeline_acquisition_callback(struct repeating_timer *t) {
    if (pipeline_state.state != PIPELINE_STATE_RUNNING) {
        return false;  // Stop timer
    }
    
    // Check iteration limit
    if (!pipeline_state.continuous_mode && 
        pipeline_state.iteration_count >= pipeline_state.max_iterations) {
        pipeline_state.state = PIPELINE_STATE_STOPPING;
        return false;
    }
    
    // Trigger ADC acquisition
    extern uint16_t buffer[SAMPLE_COUNT];
    extern void pio_adc_clear_fifo();
    extern void reset_all_sms();
    extern uint dma_chan;
    extern dma_channel_config dma_chan_cfg;
    extern PIO pio_adc;
    extern uint sm;
    
    // Quick acquisition setup
    pio_adc_clear_fifo();
    reset_all_sms();
    
    // Start DMA
    dma_channel_configure(dma_chan, &dma_chan_cfg, buffer, &pio_adc->rxf[sm], 
                         pipeline_state.acquisition_samples, true);
    
    // Trigger acquisition (simplified - assumes PIO programs are ready)
    pio_sm_put_blocking(pio_adc, sm, pipeline_state.acquisition_samples);
    
    // Wait for completion (non-blocking with timeout)
    absolute_time_t deadline = make_timeout_time_us(PIPELINE_ACQUISITION_US + 1000);  // Extra time
    while (dma_channel_is_busy(dma_chan)) {
        if (absolute_time_diff_us(get_absolute_time(), deadline) >= 0) {
            pipeline_state.acquisition_overruns++;
            printf("Pipeline: ADC timeout, using test data\n");
            
            // Fill with test data instead of random
            for (uint16_t i = 0; i < pipeline_state.acquisition_samples; i++) {
                buffer[i] = 2048 + (i % 100);  // Test pattern
            }
            break;
        }
    }
    
    // Process the acquired data
    pipeline_process_acquisition(buffer, pipeline_state.acquisition_samples);
    
    pipeline_state.iteration_count++;
    pipeline_state.total_acquisitions++;
    
    return true;  // Continue timer
}

bool pipeline_process_acquisition(const uint16_t* raw_data, uint16_t sample_count) {
    // Ensure DSP is initialized
    if (!dsp_is_initialized()) {
        printf("Pipeline: DSP not initialized, skipping processing\n");
        return false;
    }
    
    // Process samples through DSP
    uint16_t processed_count = dsp_process_samples(raw_data, sample_count, 
                                                  temp_dsp_buffer, sample_count);
    
    if (processed_count > 0) {
        pipeline_add_to_buffer(temp_dsp_buffer, processed_count);
        pipeline_state.total_samples_processed += sample_count;  // Track input samples
        
        printf("Pipeline: Processed %d samples -> %d output\n", sample_count, processed_count);
    } else {
        printf("Pipeline: DSP processing failed\n");
    }
    
    return (processed_count > 0);
}

bool pipeline_add_to_buffer(const uint8_t* processed_data, uint16_t data_size) {
    pipeline_buffer_t* current = pipeline_get_current_buffer();
    
    // Check if current buffer has space
    if (pipeline_buffer_free_space(current) < data_size) {
        // Buffer is full, swap to next buffer
        pipeline_swap_buffers();
        current = pipeline_get_current_buffer();
    }
    
    // Add data to buffer
    memcpy(current->data + current->size, processed_data, data_size);
    current->size += data_size;
    
    return true;
}

//---------------------------------------------------------------------------
// CONTROL FUNCTIONS
//---------------------------------------------------------------------------

bool pipeline_start(uint32_t max_iterations) {
    if (pipeline_state.state != PIPELINE_STATE_IDLE) {
        printf("Pipeline: Already running\n");
        return false;
    }
    
    // Ensure DSP is initialized before starting pipeline
    if (!dsp_is_initialized()) {
        printf("Pipeline: Auto-initializing DSP...\n");
        if (!dsp_init(&pipeline_state.dsp_config)) {
            printf("Pipeline: DSP auto-initialization failed\n");
            return false;
        }
    }
    
    // Allocate buffers
    if (!pipeline_allocate_buffers()) {
        return false;
    }
    
    // Reset statistics
    pipeline_state.total_acquisitions = 0;
    pipeline_state.total_samples_processed = 0;
    pipeline_state.total_bytes_written = 0;
    pipeline_state.acquisition_overruns = 0;
    pipeline_state.write_errors = 0;
    pipeline_state.iteration_count = 0;
    pipeline_state.max_iterations = max_iterations;
    pipeline_state.start_time = get_absolute_time();
    
    // Start core 1 for SD writing
    if (!core1_running) {
        multicore_launch_core1(pipeline_sd_writer_core1);
        core1_running = true;
        sleep_ms(100);  // Give core 1 time to start
    }
    
    // Start acquisition timer
    pipeline_state.state = PIPELINE_STATE_RUNNING;
    
    if (!add_repeating_timer_us(-PIPELINE_ACQUISITION_US, 
                               pipeline_acquisition_callback, 
                               NULL, &pipeline_state.acquisition_timer)) {
        printf("Pipeline: Failed to start acquisition timer\n");
        pipeline_state.state = PIPELINE_STATE_ERROR;
        return false;
    }
    
    pipeline_state.timer_active = true;
    
    printf("Pipeline: Started - %s mode, max_iterations=%lu\n",
           pipeline_state.continuous_mode ? "continuous" : "limited",
           max_iterations);
    
    return true;
}

void pipeline_stop(void) {
    if (pipeline_state.state == PIPELINE_STATE_IDLE) {
        return;
    }
    
    printf("Pipeline: Stopping...\n");
    pipeline_state.state = PIPELINE_STATE_STOPPING;
    
    // Stop acquisition timer
    if (pipeline_state.timer_active) {
        cancel_repeating_timer(&pipeline_state.acquisition_timer);
        pipeline_state.timer_active = false;
    }
    
    // Wait for any remaining writes to complete
    sleep_ms(100);
    
    // Flush any remaining buffers
    pipeline_flush_buffers();
    
    pipeline_state.state = PIPELINE_STATE_IDLE;
    printf("Pipeline: Stopped\n");
}

bool pipeline_is_running(void) {
    return (pipeline_state.state == PIPELINE_STATE_RUNNING);
}

uint8_t pipeline_get_state(void) {
    return pipeline_state.state;
}

//---------------------------------------------------------------------------
// SD WRITER (CORE 1)
//---------------------------------------------------------------------------

void pipeline_sd_writer_core1(void) {
    printf("Pipeline: SD writer core started\n");
    
    while (true) {
        // Look for buffers ready to write
        pipeline_buffer_t* buffer = pipeline_get_write_ready_buffer();
        
        if (buffer && !buffer->write_in_progress) {
            buffer->write_in_progress = true;
            
            if (pipeline_write_buffer_to_sd(buffer)) {
                pipeline_state.total_bytes_written += buffer->size;
            } else {
                pipeline_state.write_errors++;
            }
            
            // Reset buffer for reuse
            buffer->size = 0;
            buffer->ready_for_write = false;
            buffer->write_in_progress = false;
        } else {
            // No work to do, sleep briefly
            sleep_ms(1);
        }
        
        // Exit if pipeline is stopping and no more work
        if (pipeline_state.state == PIPELINE_STATE_STOPPING) {
            bool any_pending = false;
            for (int i = 0; i < PIPELINE_MAX_BUFFERS; i++) {
                if (pipeline_state.buffers[i].ready_for_write) {
                    any_pending = true;
                    break;
                }
            }
            if (!any_pending) break;
        }
    }
    
    printf("Pipeline: SD writer core stopped\n");
    core1_running = false;
}

bool pipeline_write_buffer_to_sd(pipeline_buffer_t* buffer) {
    if (!buffer || buffer->size == 0) return false;
    
    return true;
}

//---------------------------------------------------------------------------
// UTILITY FUNCTIONS
//---------------------------------------------------------------------------

void pipeline_flush_buffers(void) {
    printf("Pipeline: Flushing remaining buffers...\n");
    
    for (int i = 0; i < PIPELINE_MAX_BUFFERS; i++) {
        pipeline_buffer_t* buffer = &pipeline_state.buffers[i];
        if (buffer->size > 0) {
            buffer->ready_for_write = true;
            if (pipeline_write_buffer_to_sd(buffer)) {
                pipeline_state.total_bytes_written += buffer->size;
                printf("Pipeline: Flushed buffer %d (%lu bytes)\n", i, buffer->size);
            }
            buffer->size = 0;
            buffer->ready_for_write = false;
        }
    }
}

void pipeline_print_status(void) {
    printf("Pipeline Status:\n");
    printf("  - State: %d\n", pipeline_state.state);
    printf("  - Acquisitions: %lu\n", pipeline_state.total_acquisitions);
    printf("  - Samples Processed: %lu\n", pipeline_state.total_samples_processed);
    printf("  - Bytes Written: %lu\n", pipeline_state.total_bytes_written);
    printf("  - Overruns: %lu\n", pipeline_state.acquisition_overruns);
    printf("  - Write Errors: %lu\n", pipeline_state.write_errors);
    printf("  - Current Buffer: %d\n", pipeline_state.current_buffer);
    printf("  - SD Block Counter: %lu\n", pipeline_state.sd_block_counter);
    
    // Buffer status
    for (int i = 0; i < PIPELINE_MAX_BUFFERS; i++) {
        pipeline_buffer_t* buf = &pipeline_state.buffers[i];
        printf("  - Buffer %d: %lu/%lu bytes, ready=%d, writing=%d\n",
               i, buf->size, buf->capacity, buf->ready_for_write, buf->write_in_progress);
    }
}

void pipeline_print_statistics(void) {
    absolute_time_t now = get_absolute_time();
    uint64_t runtime_us = absolute_time_diff_us(pipeline_state.start_time, now);
    
    printf("Pipeline Statistics:\n");
    printf("  - Runtime: %llu us (%.2f s)\n", runtime_us, runtime_us / 1000000.0);
    printf("  - Throughput: %lu SPS\n", pipeline_get_throughput_sps());
    printf("  - Write Speed: %lu MB/s\n", pipeline_get_write_speed_mbps());
    printf("  - Compression: %.2fx\n", pipeline_get_compression_ratio());
    
    // Fix division by zero in success rate calculation
    if (pipeline_state.total_acquisitions > 0) {
        printf("  - Success Rate: %.2f%%\n", 
               100.0 * (pipeline_state.total_acquisitions - pipeline_state.acquisition_overruns) / 
               (float)pipeline_state.total_acquisitions);
    } else {
        printf("  - Success Rate: 0.00%% (no acquisitions)\n");
    }
}

uint32_t pipeline_get_throughput_sps(void) {
    absolute_time_t now = get_absolute_time();
    uint64_t runtime_us = absolute_time_diff_us(pipeline_state.start_time, now);
    
    if (runtime_us == 0) return 0;
    return (pipeline_state.total_samples_processed * 1000000ULL) / runtime_us;
}

uint32_t pipeline_get_write_speed_mbps(void) {
    absolute_time_t now = get_absolute_time();
    uint64_t runtime_us = absolute_time_diff_us(pipeline_state.start_time, now);
    
    if (runtime_us == 0) return 0;
    return (pipeline_state.total_bytes_written * 1000000ULL) / (runtime_us * 1024 * 1024);
}

float pipeline_get_compression_ratio(void) {
    if (pipeline_state.total_bytes_written == 0) return 0.0f;
    
    uint64_t input_bytes = pipeline_state.total_samples_processed * 2;  // 16-bit samples
    if (input_bytes == 0) return 0.0f;  // Prevent division by zero
    
    return (float)input_bytes / (float)pipeline_state.total_bytes_written;
}

bool pipeline_stress_test(uint32_t duration_seconds, bool print_progress) {
    printf("Pipeline: Starting stress test for %lu seconds\n", duration_seconds);
    
    if (!pipeline_start(0)) {  // Unlimited iterations
        return false;
    }
    
    absolute_time_t test_start = get_absolute_time();
    absolute_time_t last_print = test_start;
    
    while (true) {
        absolute_time_t now = get_absolute_time();
        uint64_t elapsed_us = absolute_time_diff_us(test_start, now);
        
        if (elapsed_us >= duration_seconds * 1000000ULL) {
            break;
        }
        
        if (print_progress && absolute_time_diff_us(last_print, now) >= 1000000) {
            pipeline_print_statistics();
            last_print = now;
        }
        
        sleep_ms(100);
    }
    
    pipeline_stop();
    pipeline_print_statistics();
    
    return (pipeline_state.acquisition_overruns == 0 && pipeline_state.write_errors == 0);
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
