//!
//! \file       main.c
//! \author     Abdelrahman Ali
//! \date       2024-01-20
//!
//! \brief      adc dac vga main entry.
//!

//---------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------
#include "adc/adc.h"
#include "max/max14866.h"
#include "sdio/sdio.h"
#include "dsp/signal_proc.h"
#include "pipeline/pipeline.h"

//---------------------------------------------------------------------------
// GLOBALS
//--------------------------------------------------------------------------

typedef void (*command_func_t)(const char *args);

typedef struct
{
    const char *command_name;
    command_func_t func;
} command_t;

// SDIO command functions
void sdio_init_cmd(const char *args) {
    printf("Initializing SDIO...\n");
    if (sdio_init()) {
        printf("SDIO hardware initialized successfully\n");
        if (sdio_card_init()) {
            printf("SD card initialized successfully\n");
        } else {
            printf("SD card initialization failed\n");
        }
    } else {
        printf("SDIO hardware initialization failed\n");
    }
}

void sdio_status_cmd(const char *args) {
    sdio_print_status();
}

void sdio_test_cmd(const char *args) {
    uint32_t num_chunks = 1;
    if (args && strlen(args) > 0) {
        num_chunks = atoi(args);
        if (num_chunks == 0) num_chunks = 1;
    }
    
    printf("Starting SDIO stress test with %lu chunks...\n", num_chunks);
    if (sdio_stress_test_write(num_chunks, false)) {
        printf("SDIO stress test completed successfully\n");
    } else {
        printf("SDIO stress test failed\n");
    }
}

// DSP command functions
void dsp_init_cmd(const char *args) {
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

void dsp_test_cmd(const char *args) {
    extern uint16_t buffer[SAMPLE_COUNT];  // Use ADC buffer
    
    printf("Testing DSP with current ADC buffer...\n");
    
    // Allocate output buffer  
    uint8_t* output = malloc(SAMPLE_COUNT / 2);
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

void dsp_status_cmd(const char *args) {
    dsp_print_config();
    dsp_print_statistics();
}

// Pipeline command functions
void pipeline_init_cmd(const char *args) {
    if (pipeline_init()) {
        printf("Pipeline initialized successfully\n");
    } else {
        printf("Pipeline initialization failed\n");
    }
}

void pipeline_start_cmd(const char *args) {
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

void pipeline_stop_cmd(const char *args) {
    pipeline_stop();
    printf("Pipeline stopped\n");
}

void pipeline_status_cmd(const char *args) {
    pipeline_print_status();
    pipeline_print_statistics();
}

void pipeline_test_cmd(const char *args) {
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
    {"sdio status", sdio_status_cmd},
    {"sdio test", sdio_test_cmd},
    {"dsp init", dsp_init_cmd},
    {"dsp test", dsp_test_cmd},
    {"dsp status", dsp_status_cmd},
    {"pipeline init", pipeline_init_cmd},
    {"pipeline start", pipeline_start_cmd},
    {"pipeline stop", pipeline_stop_cmd},
    {"pipeline status", pipeline_status_cmd},
    {"pipeline test", pipeline_test_cmd},
};

void process_command(char *input)
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
                    args = "0";
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

void read_input(char *buffer, int max_len)
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

int main()
{
    stdio_init_all();

    while (!stdio_usb_connected())
    {
        tight_loop_contents();
    }
    sleep_ms(100);
    pio_adc_init();
    sleep_ms(100);
    dac_init();
    sleep_ms(100);
    max14866_init();
    sleep_ms(100);
    char input[128];
    while (true)
    {
        printf("run> ");
        fflush(stdout);
        read_input(input, sizeof(input));
        process_command(input);
    }
}
