//!
//! \file       signal_proc.c
//! \author     Abdelrahman Ali
//! \date       2025-01-21
//!
//! \brief      Signal processing implementation for envelope detection and decimation.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "signal_proc.h"

//---------------------------------------------------------------------------
// GLOBAL VARIABLES
//---------------------------------------------------------------------------

static dsp_state_t dsp_state;
static uint16_t* streaming_buffer = NULL;
static uint16_t streaming_buffer_size = 0;
static uint16_t streaming_write_pos = 0;
static uint16_t streaming_read_pos = 0;

//---------------------------------------------------------------------------
// INITIALIZATION FUNCTIONS
//---------------------------------------------------------------------------

bool dsp_init(dsp_config_t* config) {
    if (!config) return false;
    
    memset(&dsp_state, 0, sizeof(dsp_state));
    memcpy(&dsp_state.config, config, sizeof(dsp_config_t));
    
    // Allocate filter buffer
    if (config->filter_length > 0) {
        dsp_state.filter_buffer = malloc(config->filter_length * sizeof(int32_t));
        if (!dsp_state.filter_buffer) {
            printf("DSP: Failed to allocate filter buffer\n");
            return false;
        }
        memset(dsp_state.filter_buffer, 0, config->filter_length * sizeof(int32_t));
    }
    
    // Initialize envelope detection
    dsp_state.envelope_peak = 0;
    dsp_state.envelope_decay = 1000;  // Decay constant
    
    dsp_state.initialized = true;
    
    printf("DSP: Initialized with decimation=%d, filter=%d\n", 
           config->decimation_factor, config->filter_type);
    
    return true;
}

void dsp_reset(void) {
    if (!dsp_state.initialized) return;
    
    // Reset filter state
    if (dsp_state.filter_buffer) {
        memset(dsp_state.filter_buffer, 0, dsp_state.config.filter_length * sizeof(int32_t));
    }
    
    dsp_state.filter_index = 0;
    dsp_state.accumulator = 0;
    dsp_state.iir_y1 = 0;
    dsp_state.iir_x1 = 0;
    dsp_state.envelope_peak = 0;
    
    // Reset statistics
    dsp_state.samples_processed = 0;
    dsp_state.samples_output = 0;
    dsp_state.processing_time_us = 0;
}

void dsp_deinit(void) {
    if (dsp_state.filter_buffer) {
        free(dsp_state.filter_buffer);
        dsp_state.filter_buffer = NULL;
    }
    
    if (streaming_buffer) {
        free(streaming_buffer);
        streaming_buffer = NULL;
    }
    
    memset(&dsp_state, 0, sizeof(dsp_state));
}

//---------------------------------------------------------------------------
// CONFIGURATION FUNCTIONS
//---------------------------------------------------------------------------

void dsp_set_decimation(uint16_t factor) {
    if (factor > 0 && factor <= DSP_DECIMATION_MAX) {
        dsp_state.config.decimation_factor = factor;
    }
}

void dsp_set_filter_type(uint8_t filter_type) {
    dsp_state.config.filter_type = filter_type;
}

void dsp_set_filter_length(uint16_t length) {
    if (length <= DSP_ENVELOPE_BUFFER) {
        dsp_state.config.filter_length = length;
    }
}

void dsp_set_envelope_detection(bool enable) {
    dsp_state.config.envelope_detection = enable;
}

//---------------------------------------------------------------------------
// CORE PROCESSING FUNCTIONS
//---------------------------------------------------------------------------

uint16_t dsp_process_samples(const uint16_t* input, uint16_t input_count, 
                            void* output, uint16_t max_output) {
    if (!dsp_state.initialized || !input || !output) return 0;
    
    absolute_time_t start_time = get_absolute_time();
    uint16_t output_count = 0;
    
    // Choose processing path based on configuration
    if (dsp_state.config.envelope_detection) {
        output_count = dsp_envelope_detect(input, input_count, (uint8_t*)output, max_output);
    } else {
        switch (dsp_state.config.filter_type) {
            case DSP_FILTER_MOVING_AVG:
                output_count = dsp_filter_moving_average(input, input_count, 
                                                       dsp_state.config.filter_length, 
                                                       (uint16_t*)output);
                break;
                
            case DSP_FILTER_IIR_LP:
                output_count = dsp_filter_iir_lowpass(input, input_count,
                                                    dsp_state.config.filter_cutoff,
                                                    (uint16_t*)output);
                break;
                
            default:
                // Simple decimation without filtering
                output_count = dsp_decimate_simple(input, input_count,
                                                 dsp_state.config.decimation_factor,
                                                 output, dsp_state.config.output_format);
                break;
        }
    }
    
    // Update statistics
    absolute_time_t end_time = get_absolute_time();
    dsp_state.processing_time_us += absolute_time_diff_us(start_time, end_time);
    dsp_state.samples_processed += input_count;
    dsp_state.samples_output += output_count;
    
    return output_count;
}

uint16_t dsp_envelope_detect(const uint16_t* input, uint16_t input_count,
                            uint8_t* output, uint16_t max_output) {
    if (!input || !output) return 0;
    
    uint16_t output_count = 0;
    uint16_t decimation = dsp_state.config.decimation_factor;
    if (decimation == 0) decimation = 1;
    
    for (uint16_t i = 0; i < input_count && output_count < max_output; i += decimation) {
        // Find peak in decimation window
        uint16_t window_peak = 0;
        for (uint16_t j = 0; j < decimation && (i + j) < input_count; j++) {
            // Convert to signed and take absolute value
            int16_t signed_val = dsp_adc_to_signed(input[i + j]);
            uint16_t abs_val = dsp_abs16(signed_val);
            if (abs_val > window_peak) {
                window_peak = abs_val;
            }
        }
        
        // Envelope following with peak detection and decay
        if (window_peak > dsp_state.envelope_peak) {
            dsp_state.envelope_peak = window_peak;
        } else {
            // Exponential decay
            dsp_state.envelope_peak = (dsp_state.envelope_peak * dsp_state.envelope_decay) / 1024;
        }
        
        // Convert to output format (8-bit)
        output[output_count] = (uint8_t)(dsp_state.envelope_peak >> 4);  // Scale to 8-bit
        output_count++;
    }
    
    return output_count;
}

uint16_t dsp_decimate_simple(const uint16_t* input, uint16_t input_count,
                            uint16_t decimation, void* output, uint8_t output_format) {
    if (!input || !output || decimation == 0) return 0;
    
    uint16_t output_count = 0;
    
    for (uint16_t i = 0; i < input_count; i += decimation) {
        uint32_t sum = 0;
        uint16_t samples = 0;
        
        // Average decimation window
        for (uint16_t j = 0; j < decimation && (i + j) < input_count; j++) {
            sum += input[i + j];
            samples++;
        }
        
        uint16_t avg = sum / samples;
        
        // Output in requested format
        switch (output_format) {
            case DSP_FORMAT_UINT16:
                ((uint16_t*)output)[output_count] = avg;
                break;
            case DSP_FORMAT_UINT8:
                ((uint8_t*)output)[output_count] = (uint8_t)(avg >> 4);  // Scale to 8-bit
                break;
            case DSP_FORMAT_INT16:
                ((int16_t*)output)[output_count] = dsp_adc_to_signed(avg);
                break;
            default:
                ((uint16_t*)output)[output_count] = avg;
                break;
        }
        
        output_count++;
    }
    
    return output_count;
}

uint16_t dsp_filter_moving_average(const uint16_t* input, uint16_t input_count,
                                  uint16_t window_size, uint16_t* output) {
    if (!input || !output || window_size == 0) return 0;
    if (!dsp_state.filter_buffer) return 0;
    
    uint16_t output_count = 0;
    
    for (uint16_t i = 0; i < input_count; i++) {
        // Add new sample to circular buffer
        uint16_t old_index = dsp_state.filter_index;
        dsp_state.filter_buffer[old_index] = input[i];
        
        // Update accumulator
        if (dsp_state.samples_processed >= window_size) {
            // Remove oldest sample
            uint16_t oldest_index = (old_index + 1) % window_size;
            dsp_state.accumulator -= dsp_state.filter_buffer[oldest_index];
        }
        dsp_state.accumulator += input[i];
        
        // Update index
        dsp_state.filter_index = (dsp_state.filter_index + 1) % window_size;
        
        // Output filtered value
        uint16_t divisor = (dsp_state.samples_processed + 1 < window_size) ? 
                          (dsp_state.samples_processed + 1) : window_size;
        output[output_count] = (uint16_t)(dsp_state.accumulator / divisor);
        output_count++;
    }
    
    return output_count;
}

uint16_t dsp_filter_iir_lowpass(const uint16_t* input, uint16_t input_count,
                               float cutoff, uint16_t* output) {
    if (!input || !output) return 0;
    
    // Simple first-order IIR low-pass filter
    // y[n] = a * x[n] + (1-a) * y[n-1]
    // where a = cutoff frequency (0-1)
    
    int32_t a_fixed = (int32_t)(cutoff * 32768);  // Convert to fixed point
    int32_t one_minus_a = 32768 - a_fixed;
    
    for (uint16_t i = 0; i < input_count; i++) {
        int32_t x = input[i];
        
        // Apply IIR filter in fixed point
        dsp_state.iir_y1 = (a_fixed * x + one_minus_a * dsp_state.iir_y1) >> 15;
        
        output[i] = (uint16_t)dsp_state.iir_y1;
    }
    
    return input_count;
}

//---------------------------------------------------------------------------
// ADVANCED ENVELOPE DETECTION (RF SIGNAL)
//---------------------------------------------------------------------------

uint16_t dsp_envelope_extract_rf(const uint16_t* input, uint16_t input_count,
                                uint8_t* envelope, uint16_t decimation) {
    if (!input || !envelope || decimation == 0) return 0;
    
    uint16_t output_count = 0;
    
    for (uint16_t i = 0; i < input_count; i += decimation) {
        // Calculate RMS over decimation window for RF envelope
        uint64_t sum_squares = 0;
        uint16_t samples = 0;
        
        for (uint16_t j = 0; j < decimation && (i + j) < input_count; j++) {
            int16_t signed_val = dsp_adc_to_signed(input[i + j]);
            sum_squares += (int64_t)signed_val * signed_val;
            samples++;
        }
        
        if (samples > 0) {
            uint32_t mean_square = (uint32_t)(sum_squares / samples);
            uint16_t rms = dsp_isqrt(mean_square);
            
            // Convert to 8-bit envelope
            envelope[output_count] = (uint8_t)(rms >> 4);
            output_count++;
        }
    }
    
    return output_count;
}

//---------------------------------------------------------------------------
// STREAMING PROCESSING
//---------------------------------------------------------------------------

void dsp_process_streaming_init(uint16_t buffer_size) {
    if (streaming_buffer) {
        free(streaming_buffer);
    }
    
    streaming_buffer_size = buffer_size;
    streaming_buffer = malloc(buffer_size * sizeof(uint16_t));
    streaming_write_pos = 0;
    streaming_read_pos = 0;
    
    printf("DSP: Streaming buffer initialized (%d samples)\n", buffer_size);
}

bool dsp_process_streaming_add(const uint16_t* samples, uint16_t count) {
    if (!streaming_buffer || !samples) return false;
    
    for (uint16_t i = 0; i < count; i++) {
        streaming_buffer[streaming_write_pos] = samples[i];
        streaming_write_pos = (streaming_write_pos + 1) % streaming_buffer_size;
        
        // Check for buffer overflow
        if (streaming_write_pos == streaming_read_pos) {
            printf("DSP: Streaming buffer overflow\n");
            return false;
        }
    }
    
    return true;
}

uint16_t dsp_process_streaming_get(void* output, uint16_t max_count) {
    if (!streaming_buffer || !output) return 0;
    
    uint16_t available = 0;
    if (streaming_write_pos >= streaming_read_pos) {
        available = streaming_write_pos - streaming_read_pos;
    } else {
        available = streaming_buffer_size - streaming_read_pos + streaming_write_pos;
    }
    
    uint16_t to_process = (available > max_count) ? max_count : available;
    if (to_process < dsp_state.config.decimation_factor) return 0;
    
    // Create temporary input buffer
    uint16_t* temp_input = malloc(to_process * sizeof(uint16_t));
    if (!temp_input) return 0;
    
    // Copy data from circular buffer
    for (uint16_t i = 0; i < to_process; i++) {
        temp_input[i] = streaming_buffer[streaming_read_pos];
        streaming_read_pos = (streaming_read_pos + 1) % streaming_buffer_size;
    }
    
    // Process data
    uint16_t output_count = dsp_process_samples(temp_input, to_process, output, max_count);
    
    free(temp_input);
    return output_count;
}

//---------------------------------------------------------------------------
// UTILITY FUNCTIONS
//---------------------------------------------------------------------------

void dsp_print_config(void) {
    printf("DSP Configuration:\n");
    printf("  - Decimation: %d\n", dsp_state.config.decimation_factor);
    printf("  - Filter: %d\n", dsp_state.config.filter_type);
    printf("  - Filter Length: %d\n", dsp_state.config.filter_length);
    printf("  - Envelope Detection: %s\n", dsp_state.config.envelope_detection ? "Yes" : "No");
    printf("  - Input Format: %d\n", dsp_state.config.input_format);
    printf("  - Output Format: %d\n", dsp_state.config.output_format);
    printf("  - High Speed: %s\n", dsp_state.config.high_speed_mode ? "Yes" : "No");
}

void dsp_print_statistics(void) {
    printf("DSP Statistics:\n");
    printf("  - Samples Processed: %lu\n", dsp_state.samples_processed);
    printf("  - Samples Output: %lu\n", dsp_state.samples_output);
    printf("  - Processing Time: %lu us\n", dsp_state.processing_time_us);
    printf("  - Throughput: %lu SPS\n", dsp_get_throughput_sps());
    printf("  - Compression: %.2fx\n", dsp_get_compression_ratio());
}

uint32_t dsp_get_throughput_sps(void) {
    if (dsp_state.processing_time_us == 0) return 0;
    return (dsp_state.samples_processed * 1000000UL) / dsp_state.processing_time_us;
}

float dsp_get_compression_ratio(void) {
    if (dsp_state.samples_output == 0) return 0.0f;
    return (float)dsp_state.samples_processed / (float)dsp_state.samples_output;
}

void dsp_setup_continuous_processing(uint16_t acquisition_samples, 
                                     uint16_t target_output_rate) {
    // Calculate optimal decimation factor
    uint16_t decimation = acquisition_samples / target_output_rate;
    if (decimation > DSP_DECIMATION_MAX) decimation = DSP_DECIMATION_MAX;
    if (decimation == 0) decimation = 1;
    
    dsp_state.config.decimation_factor = decimation;
    dsp_state.config.envelope_detection = true;
    dsp_state.config.output_format = DSP_FORMAT_UINT8;  // Compressed output
    
    printf("DSP: Continuous processing setup - decimation=%d\n", decimation);
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
