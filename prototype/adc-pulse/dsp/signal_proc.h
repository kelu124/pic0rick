//!
//! \file       signal_proc.h
//! \author     Abdelrahman Ali
//! \date       2025-01-21
//!
//! \brief      Signal processing for envelope detection and decimation.
//!

#ifndef SIGNAL_PROC_H
#define SIGNAL_PROC_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

//---------------------------------------------------------------------------
// CONSTANTS
//---------------------------------------------------------------------------

#define DSP_MAX_SAMPLES        8192     // Maximum samples per processing block
#define DSP_ENVELOPE_BUFFER    128      // Moving average window for envelope
#define DSP_DECIMATION_MAX     16       // Maximum decimation factor

// Filter types
#define DSP_FILTER_NONE        0
#define DSP_FILTER_MOVING_AVG  1
#define DSP_FILTER_IIR_LP      2
#define DSP_FILTER_ENVELOPE    3

// Data formats
#define DSP_FORMAT_UINT16      0        // 16-bit unsigned (ADC output)
#define DSP_FORMAT_INT16       1        // 16-bit signed 
#define DSP_FORMAT_UINT8       2        // 8-bit unsigned (decimated output)
#define DSP_FORMAT_FLOAT       3        // 32-bit float (debugging)

//---------------------------------------------------------------------------
// STRUCTURES
//---------------------------------------------------------------------------

typedef struct {
    uint16_t decimation_factor;     // Downsample ratio (1, 2, 4, 8, 16)
    uint8_t filter_type;           // Filter algorithm to use
    uint16_t filter_length;        // Filter window/tap length
    uint8_t input_format;          // Input data format
    uint8_t output_format;         // Output data format
    float filter_cutoff;           // Cutoff frequency (normalized 0-1)
    bool envelope_detection;       // Enable envelope detection
    bool high_speed_mode;          // Use optimized processing
} dsp_config_t;

typedef struct {
    dsp_config_t config;
    
    // Buffers
    int32_t* filter_buffer;        // Filter state buffer
    uint16_t filter_index;         // Current position in filter buffer
    int64_t accumulator;           // For moving average
    
    // IIR filter state (for low-pass)
    int32_t iir_y1;               // Previous output
    int32_t iir_x1;               // Previous input
    
    // Statistics
    uint32_t samples_processed;
    uint32_t samples_output;
    uint32_t processing_time_us;
    
    // Envelope detection state
    int32_t envelope_peak;
    uint16_t envelope_decay;
    
    bool initialized;
} dsp_state_t;

//---------------------------------------------------------------------------
// FUNCTION DECLARATIONS
//---------------------------------------------------------------------------

// Initialization
bool dsp_init(dsp_config_t* config);
void dsp_reset(void);
void dsp_deinit(void);

// Configuration
void dsp_set_decimation(uint16_t factor);
void dsp_set_filter_type(uint8_t filter_type);
void dsp_set_filter_length(uint16_t length);
void dsp_set_envelope_detection(bool enable);

// Processing functions
uint16_t dsp_process_samples(const uint16_t* input, uint16_t input_count, 
                            void* output, uint16_t max_output);

uint16_t dsp_envelope_detect(const uint16_t* input, uint16_t input_count,
                            uint8_t* output, uint16_t max_output);

uint16_t dsp_decimate_simple(const uint16_t* input, uint16_t input_count,
                            uint16_t decimation, void* output, uint8_t output_format);

uint16_t dsp_filter_moving_average(const uint16_t* input, uint16_t input_count,
                                  uint16_t window_size, uint16_t* output);

uint16_t dsp_filter_iir_lowpass(const uint16_t* input, uint16_t input_count,
                               float cutoff, uint16_t* output);

// Real-time processing (for streaming)
void dsp_process_streaming_init(uint16_t buffer_size);
bool dsp_process_streaming_add(const uint16_t* samples, uint16_t count);
uint16_t dsp_process_streaming_get(void* output, uint16_t max_count);

// Utility functions
void dsp_print_config(void);
void dsp_print_statistics(void);
uint32_t dsp_get_throughput_sps(void);      // Samples per second throughput
float dsp_get_compression_ratio(void);       // Input/output ratio

// Advanced envelope detection (from image example)
uint16_t dsp_envelope_extract_rf(const uint16_t* input, uint16_t input_count,
                                uint8_t* envelope, uint16_t decimation);

// Optimized processing for continuous acquisition
void dsp_setup_continuous_processing(uint16_t acquisition_samples, 
                                     uint16_t target_output_rate);

//---------------------------------------------------------------------------
// INLINE HELPER FUNCTIONS
//---------------------------------------------------------------------------

// Fast absolute value for 16-bit signed
static inline uint16_t dsp_abs16(int16_t x) {
    return (x < 0) ? -x : x;
}

// Fast integer square root (for RMS calculations)
static inline uint16_t dsp_isqrt(uint32_t x) {
    uint16_t root = 0;
    uint16_t bit = 0x8000;
    
    while (bit > x) bit >>= 2;
    
    while (bit != 0) {
        if (x >= root + bit) {
            x -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

// Convert ADC reading to signed value (assuming 12-bit ADC centered at 2048)
static inline int16_t dsp_adc_to_signed(uint16_t adc_val) {
    return (int16_t)(adc_val - 2048);
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------

#endif // SIGNAL_PROC_H
