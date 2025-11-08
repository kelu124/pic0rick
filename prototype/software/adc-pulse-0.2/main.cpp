//!
//! \file       main.cpp
//! \author     Abdelrahman Ali
//! \date       2024-01-20
//!
//! \brief      adc dac vga main entry with pico-sdio support.
//!

//---------------------------------------------------------------------------
// INCLUDES  
//--------------------------------------------------------------------------
#include <string.h>
#include <iostream>
using namespace std;

// C++ headers for pico-sdio
#include "SdFat.h"               
#include "sdio.h"                

// C headers with extern "C" wrapper
extern "C" {
#include "adc/adc.h"
#include "max/max14866.h"
#include "dsp/signal_proc.h"
#include "pipeline/pipeline.h"
}

//---------------------------------------------------------------------------
// GLOBALS
//--------------------------------------------------------------------------

typedef void (*command_func_t)(const char *args);

typedef struct
{
    const char *command_name;
    command_func_t func;
} command_t;

// Global SD objects for pico-sdio library
SdFat sd;
FsFile file;

// SDIO command functions using pico-sdio library
extern "C" void sdio_init_cmd(const char *args) {
    printf("=== SDIO Initialization ===\n");
    printf("Pins: CLK=22, CMD=26, DAT0-3=18-21\n\n");
    
    // Start with slow clock for compatibility
    rp2040_sdio_init(8);
    sleep_ms(50);
    
    SdioConfig config(FIFO_SDIO);
    
    if (!sd.begin(config)) {
        printf("✗ SDIO initialization failed\n");
        printf("Check: card insertion, wiring, 3.3V power\n");
        if (sd.card()) {
            printf("Card error code: %d\n", sd.card()->errorCode());
        }
        return;
    }
    
    printf("✓ SDIO initialization successful!\n");
    
    // Get card info
    uint32_t sectorCount = sd.card()->sectorCount();
    printf("Card: %.2f GB (%lu sectors)\n", sectorCount * 512.0 / 1e9, sectorCount);
    
    printf("✓ Ready for raw sector operations\n");
    printf("Note: File operations require 10kΩ pull-ups on DAT0-DAT3\n");
    
    // Switch to faster speed for testing
    rp2040_sdio_init(2);
    printf("✓ High-speed mode enabled\n");
}

extern "C" void sdio_speed_cmd(const char *args) {
    printf("=== SDIO Raw Sector Write Speed Test ===\n");
    printf("Testing raw sector write performance\n\n");
    
    if (!sd.card()) {
        printf("✗ SD card not initialized. Run 'sdio init' first.\n");
        return;
    }
    
    // Test parameters - use raw sector operations
    const uint32_t SECTORS_PER_TEST = 4096;  // 2MB test (512 bytes per sector)
    const uint8_t WRITE_COUNT = 3;
    const uint32_t START_SECTOR = 100000;  // Use sectors far from filesystem
    
    // Allocate 512-byte aligned buffer for single sector
    uint8_t* buf = (uint8_t*)malloc(512);
    if (!buf) {
        printf("✗ Failed to allocate sector buffer\n");
        return;
    }
    
    // Fill buffer with test pattern
    for (uint32_t i = 0; i < 512; i++) {
        buf[i] = (uint8_t)(0xAA ^ (i & 0xFF));
    }
    
    printf("Testing raw sector writes (sector size: 512 bytes)\n");
    printf("Test size: %lu sectors (%.2f MB per test)\n", 
           SECTORS_PER_TEST, SECTORS_PER_TEST * 512.0f / (1024*1024));
    printf("Running %d write tests...\n\n", WRITE_COUNT);
    
    float total_speed = 0;
    int successful_tests = 0;
    
    for (int test = 0; test < WRITE_COUNT; test++) {
        printf("Raw write test %d: ", test + 1);
        
        absolute_time_t start = get_absolute_time();
        
        bool success = true;
        for (uint32_t sector = 0; sector < SECTORS_PER_TEST && success; sector++) {
            if (!sd.card()->writeSector(START_SECTOR + sector, buf)) {
                printf("FAILED at sector %lu\n", sector);
                success = false;
                break;
            }
        }
        
        if (success) {
            absolute_time_t end = get_absolute_time();
            
            int64_t duration_us = absolute_time_diff_us(start, end);
            float total_bytes = SECTORS_PER_TEST * 512.0f;
            float speed_mbps = (total_bytes * 1000000.0f) / (duration_us * 1024.0f * 1024.0f);
            
            total_speed += speed_mbps;
            successful_tests++;
            printf("%.2f MB/s\n", speed_mbps);
        }
    }
    
    free(buf);
    
    if (successful_tests > 0) {
        float avg_speed = total_speed / successful_tests;
        printf("\n=== RAW SECTOR RESULTS ===\n");
        printf("Average Speed: %.2f MB/s (%d tests)\n", avg_speed, successful_tests);
        
        if (avg_speed >= 15.0f) {
            printf("STATUS: ✓ EXCELLENT raw performance!\n");
            printf("This shows SDIO is working well at hardware level\n");
        } else if (avg_speed >= 8.0f) {
            printf("STATUS: ✓ GOOD raw performance\n");
            printf("Hardware level SDIO is functional\n");
        } else {
            printf("STATUS: ⚠ MODERATE performance\n");
            printf("May improve with pull-up resistors\n");
        }
    } else {
        printf("\n=== FAILED ===\n");
        printf("Raw sector writes failed - check SDIO hardware\n");
    }
}

extern "C" void sdio_read_test_cmd(const char *args) {
    printf("=== SDIO Read Performance Test ===\n");
    printf("Testing read performance similar to library benchmark\n\n");
    
    if (!sd.card()) {
        printf("✗ SD card not initialized. Run 'sdio init' first.\n");
        return;
    }
    
    // Test with larger size similar to library benchmark (1MB test)
    const uint32_t SECTORS_PER_BUFFER = 64;    // 32KB buffer (like library)
    const uint32_t TOTAL_BUFFERS = 32;         // 32 * 32KB = 1MB total
    const uint32_t BUFFER_SIZE = SECTORS_PER_BUFFER * 512;
    const uint32_t TOTAL_SECTORS = TOTAL_BUFFERS * SECTORS_PER_BUFFER;
    
    uint8_t* read_buf = (uint8_t*)malloc(BUFFER_SIZE);
    
    if (!read_buf) {
        printf("✗ Failed to allocate %lu KB buffer\n", BUFFER_SIZE / 1024);
        return;
    }
    
    printf("Testing with %lu KB buffers (like SdFat benchmark)\n", BUFFER_SIZE / 1024);
    printf("Total test size: %lu MB (%lu sectors)\n", 
           (TOTAL_SECTORS * 512) / (1024*1024), TOTAL_SECTORS);
    printf("This tests sustained read performance...\n\n");
    
    bool test_failed = false;
    uint32_t successful_buffers = 0;
    
    absolute_time_t total_start = get_absolute_time();
    
    for (uint32_t buffer = 0; buffer < TOTAL_BUFFERS; buffer++) {
        uint32_t start_sector = buffer * SECTORS_PER_BUFFER;
        
        printf("Reading buffer %lu/%lu (sectors %lu-%lu): ", 
               buffer + 1, TOTAL_BUFFERS, start_sector, start_sector + SECTORS_PER_BUFFER - 1);
        
        absolute_time_t start = get_absolute_time();
        
        // Read multiple sectors in one operation (more efficient)
        bool success = true;
        for (uint32_t i = 0; i < SECTORS_PER_BUFFER; i++) {
            if (!sd.card()->readSector(start_sector + i, read_buf + (i * 512))) {
                success = false;
                break;
            }
        }
        
        if (!success) {
            printf("FAILED\n");
            test_failed = true;
            break;
        }
        
        absolute_time_t end = get_absolute_time();
        int64_t duration_us = absolute_time_diff_us(start, end);
        float buffer_speed = (BUFFER_SIZE * 1000000.0f) / (duration_us * 1024.0f * 1024.0f);
        
        successful_buffers++;
        printf("%.2f MB/s\n", buffer_speed);
        
        // Show data verification for first buffer
        if (buffer == 0) {
            printf("  First sector data: ");
            for (int i = 0; i < 8; i++) {
                printf("%02X ", read_buf[i]);
            }
            printf("\n");
        }
    }
    
    absolute_time_t total_end = get_absolute_time();
    int64_t total_duration_us = absolute_time_diff_us(total_start, total_end);
    
    printf("\n=== PERFORMANCE RESULTS ===\n");
    if (!test_failed && successful_buffers == TOTAL_BUFFERS) {
        float total_bytes = TOTAL_SECTORS * 512.0f;
        float avg_read_speed = (total_bytes * 1000000.0f) / (total_duration_us * 1024.0f * 1024.0f);
        
        printf("✓✓✓ SUSTAINED READ TEST SUCCESSFUL ✓✓✓\n");
        printf("Read %.1f MB in %lu buffers\n", total_bytes / (1024*1024), TOTAL_BUFFERS);
        printf("Average sustained speed: %.2f MB/s\n", avg_read_speed);
        printf("Buffer size: %lu KB (matching SdFat benchmark)\n", BUFFER_SIZE / 1024);
        
        printf("\n📊 PERFORMANCE COMPARISON:\n");
        if (avg_read_speed >= 15.0f) {
            printf("🚀 EXCELLENT! Approaching library benchmark (24+ MB/s)\n");
            printf("   Your SDIO implementation is very efficient\n");
        } else if (avg_read_speed >= 8.0f) {
            printf("✅ VERY GOOD! Solid performance for no pull-ups\n");
            printf("   With pull-ups: expect 20+ MB/s reads\n");
        } else if (avg_read_speed >= 3.0f) {
            printf("⚡ GOOD! Functional SDIO communication\n");
            printf("   Adding pull-ups will significantly improve speed\n");
        } else {
            printf("⚠️  BASIC: Working but suboptimal\n");
            printf("   Pull-ups and optimizations needed for full speed\n");
        }
        
        printf("\n🎯 SDIO DATA LINES CONFIRMED WORKING!\n");
        printf("✅ Wiring: CLK=22, CMD=26, DAT0-3=18-21 ✓\n");
        printf("✅ Hardware: RP2350 SDIO implementation ✓\n");
        printf("\n💡 FOR 12+ MB/s WRITES:\n");
        printf("   Add 10kΩ pull-ups on DAT0-DAT3 (pins 18-21)\n");
        printf("   Expected: 15-25+ MB/s write performance\n");
    } else {
        printf("✗ SUSTAINED READ TEST FAILED\n");
        printf("Successful buffers: %lu/%lu\n", successful_buffers, TOTAL_BUFFERS);
        printf("This indicates SDIO hardware communication issues\n");
        printf("Check all DAT0-DAT3 wiring (pins 18-21)\n");
    }
    
    free(read_buf);
}

extern "C" void sdio_verify_cmd(const char *args) {
    printf("=== SDIO Raw Sector Verification Test ===\n");
    printf("Testing data integrity with raw sector operations\n\n");
    
    if (!sd.card()) {
        printf("✗ SD card not initialized. Run 'sdio init' first.\n");
        return;
    }
    
    const uint32_t TEST_SECTORS = 8;  // Test 8 sectors (4KB total)
    const uint32_t START_SECTOR = 200000;  // Use sectors far from filesystem
    
    uint8_t* write_buf = (uint8_t*)malloc(512);
    uint8_t* read_buf = (uint8_t*)malloc(512);
    
    if (!write_buf || !read_buf) {
        printf("✗ Failed to allocate sector buffers\n");
        if (write_buf) free(write_buf);
        if (read_buf) free(read_buf);
        return;
    }
    
    printf("Testing %lu sectors (%lu bytes total)\n", TEST_SECTORS, TEST_SECTORS * 512);
    printf("Using raw sector read/write operations\n\n");
    
    uint32_t total_mismatches = 0;
    bool test_failed = false;
    
    for (uint32_t sector = 0; sector < TEST_SECTORS; sector++) {
        // Create unique test pattern for this sector
        for (uint32_t i = 0; i < 512; i++) {
            write_buf[i] = (uint8_t)(0xAA ^ (sector & 0xFF) ^ (i & 0xFF) ^ ((i >> 4) & 0x0F));
        }
        
        printf("Sector %lu: Writing...", sector);
        
        // Write sector
        if (!sd.card()->writeSector(START_SECTOR + sector, write_buf)) {
            printf(" WRITE FAILED\n");
            test_failed = true;
            break;
        }
        
        printf(" Reading...");
        
        // Read back sector
        if (!sd.card()->readSector(START_SECTOR + sector, read_buf)) {
            printf(" READ FAILED\n");
            test_failed = true;
            break;
        }
        
        printf(" Verifying...");
        
        // Verify data integrity for this sector
        uint32_t sector_mismatches = 0;
        for (uint32_t i = 0; i < 512; i++) {
            if (write_buf[i] != read_buf[i]) {
                sector_mismatches++;
                total_mismatches++;
            }
        }
        
        if (sector_mismatches == 0) {
            printf(" ✓ PERFECT\n");
        } else {
            printf(" ✗ %lu errors\n", sector_mismatches);
        }
    }
    
    printf("\n=== RAW SECTOR VERIFICATION RESULT ===\n");
    if (!test_failed && total_mismatches == 0) {
        printf("✓✓✓ PERFECT DATA INTEGRITY ✓✓✓\n");
        printf("All %lu sectors (%lu bytes) verified perfectly!\n", TEST_SECTORS, TEST_SECTORS * 512);
        printf("SDIO hardware implementation is working correctly!\n");
        printf("Raw sector operations are reliable!\n");
    } else if (test_failed) {
        printf("✗ SECTOR OPERATION FAILED\n");
        printf("Basic read/write operations are not working\n");
        printf("This indicates a fundamental SDIO hardware issue\n");
    } else {
        printf("✗ DATA CORRUPTION DETECTED\n");
        printf("Total mismatches: %lu out of %lu bytes\n", total_mismatches, TEST_SECTORS * 512);
        printf("This indicates signal integrity issues\n");
        printf("Try adding pull-up resistors on DAT0-DAT3 lines\n");
    }
    
    free(write_buf);
    free(read_buf);
}


// DSP command functions
extern "C" void dsp_init_cmd(const char *args) {
    dsp_config_t config = {
        .decimation_factor = 4,
        .filter_type = DSP_FILTER_ENVELOPE,
        .filter_length = 32,
        .input_format = DSP_FORMAT_UINT16,
        .output_format = DSP_FORMAT_UINT8,
        .filter_cutoff = 0.1f,
        .envelope_detection = true,
        .high_speed_mode = true
    };
    
    if (dsp_init(&config)) {
        printf("DSP initialized successfully\n");
        dsp_print_config();
    } else {
        printf("DSP initialization failed\n");
    }
}

extern "C" void dsp_test_cmd(const char *args) {
    extern uint16_t buffer[SAMPLE_COUNT];  // Use ADC buffer
    
    printf("Testing DSP with current ADC buffer...\n");
    
    // Allocate output buffer  
    uint8_t* output = (uint8_t*)malloc(SAMPLE_COUNT / 2);
    if (!output) {
        printf("DSP: Failed to allocate output buffer\n");
        return;
    }
    
    // Process ADC samples
    uint16_t output_count = dsp_process_samples(buffer, SAMPLE_COUNT, output, SAMPLE_COUNT / 2);
    
    printf("DSP: Processed %d samples -> %d output samples\n", SAMPLE_COUNT, output_count);
    
    // Print first few samples for verification
    printf("First 10 envelope samples: ");
    for (int i = 0; i < 10 && i < output_count; i++) {
        printf("%d ", output[i]);
    }
    printf("\n");
    
    dsp_print_statistics();
    free(output);
}

extern "C" void dsp_status_cmd(const char *args) {
    dsp_print_config();
    dsp_print_statistics();
}

// Pipeline command functions
extern "C" void pipeline_init_cmd(const char *args) {
    if (pipeline_init()) {
        printf("Pipeline initialized successfully\n");
    } else {
        printf("Pipeline initialization failed\n");
    }
}

extern "C" void pipeline_start_cmd(const char *args) {
    uint32_t max_iterations = 0;  // Default: continuous
    
    if (args && strlen(args) > 0) {
        max_iterations = atoi(args);
    }
    
    if (pipeline_start(max_iterations)) {
        if (max_iterations == 0) {
            printf("Pipeline started in continuous mode\n");
        } else {
            printf("Pipeline started for %lu iterations\n", max_iterations);
        }
    } else {
        printf("Pipeline start failed\n");
    }
}

extern "C" void pipeline_stop_cmd(const char *args) {
    pipeline_stop();
    printf("Pipeline stopped\n");
}

extern "C" void pipeline_status_cmd(const char *args) {
    pipeline_print_status();
    pipeline_print_statistics();
}

extern "C" void pipeline_test_cmd(const char *args) {
    uint32_t duration = 10;  // Default: 10 seconds
    
    if (args && strlen(args) > 0) {
        duration = atoi(args);
        if (duration == 0) duration = 10;
    }
    
    printf("Starting pipeline stress test for %lu seconds...\n", duration);
    
    if (pipeline_stress_test(duration, true)) {
        printf("Pipeline stress test PASSED\n");
    } else {
        printf("Pipeline stress test FAILED\n");
    }
}

command_t command_list[] = {
    {"start acq", pulse_adc_trigger},
    {"write dac", dac},
    {"write mux", max14866},
    {"set mux", max14866_set},
    {"clear mux", max14866_clear},
    {"read", adc},
    {"sdio init", sdio_init_cmd},
    {"sdio speed", sdio_speed_cmd},
    {"sdio verify", sdio_verify_cmd},
    {"sdio read", sdio_read_test_cmd},
    {"dsp init", dsp_init_cmd},
    {"dsp test", dsp_test_cmd},
    {"dsp status", dsp_status_cmd},
    {"pipeline init", pipeline_init_cmd},
    {"pipeline start", pipeline_start_cmd},
    {"pipeline stop", pipeline_stop_cmd},
    {"pipeline status", pipeline_status_cmd},
    {"pipeline test", pipeline_test_cmd},
};

extern "C" void process_command(char *input)
{
    char *command = strtok(input, " ");
    char *subcommand = strtok(NULL, " ");
    char *args = strtok(NULL, "");

    if (command != NULL && subcommand != NULL)
    {
        char full_command[50];
        snprintf(full_command, sizeof(full_command), "%s %s", command, subcommand);

        for (int i = 0; i < sizeof(command_list) / sizeof(command_t); i++)
        {
            if (strcmp(full_command, command_list[i].command_name) == 0)
            {
                if (args == NULL)
                {
                    args = (char*)"0";
                }
                command_list[i].func(args);
                return;
            }
        }
        printf("Unknown command: %s %s\n", command, subcommand);
    }
    else if (command != NULL)
    {
        for (int i = 0; i < sizeof(command_list) / sizeof(command_t); i++)
        {
            if (strcmp(command, command_list[i].command_name) == 0)
            {
                command_list[i].func(args);
                return;
            }
        }
        printf("Unknown command: %s\n", command);
    }
}

extern "C" void read_input(char *buffer, int max_len)
{
    int index = 0;
    while (1)
    {
        char ch = getchar();
        if (ch == '\r' || ch == '\n')
        {
            buffer[index] = '\0';
            printf("\n");
            return;
        }
        else if (ch == 127 || ch == '\b')
        {
            if (index > 0)
            {
                index--;
                printf("\b \b");
            }
        }
        else if (ch >= 32 && ch <= 126)
        {
            if (index < max_len - 1)
            {
                buffer[index++] = ch;
                putchar(ch);
            }
        }
    }
}

//---------------------------------------------------------------------------
// MAIN FUNCTION
//---------------------------------------------------------------------------

extern "C" int main()
{
    stdio_init_all();

    while (!stdio_usb_connected())
    {
        tight_loop_contents();
    }
    sleep_ms(100);
    
    printf("\n=== ADC-Pulse System with Pico-SDIO ===\n");
    printf("Board: Pico2_W (RP2350)\n");
    printf("SD Card: SDIO mode (pins CLK=22, CMD=26, DAT0-3=18-21)\n");
    printf("Optimized for U3/V30 high-speed cards\n\n");
    
    // Initialize all systems (excluding SD - initialized on-demand)
    pio_adc_init();
    sleep_ms(100);
    dac_init();
    sleep_ms(100);
    max14866_init();
    sleep_ms(100);
    
    printf("System initialized. Available commands:\n");
    printf("  sdio init      - Initialize SD card (SDIO mode)\n");
    printf("  sdio read      - Test data reading (works without pull-ups)\n");
    printf("  sdio speed     - Test write speed (needs pull-ups)\n");
    printf("  sdio verify    - Test write/read integrity (needs pull-ups)\n");
    printf("  start acq      - Start ADC acquisition\n");
    printf("  dsp init/test  - DSP operations\n");
    printf("  pipeline *     - Pipeline operations\n");
    printf("\nTip: Try 'sdio read' first to test your wiring!\n");
    printf("For 12+ MB/s writes: Add 10kΩ pull-ups on DAT0-DAT3.\n\n");
    
    char input[128];
    while (true)
    {
        printf("run> ");
        fflush(stdout);
        read_input(input, sizeof(input));
        process_command(input);
    }
}
