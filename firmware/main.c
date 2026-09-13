//---------------------------------------------------------------------------
// INCLUDES
//--------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "acquisition.h"   // shared pulser + ADC capture (firmware/hw)
#include "dac.h"           // shared spi1 MCP4812 DAC (firmware/hw)
#include "u4rk.h"
#include "version.h"
#include "version_git.h"
#ifdef MUX
#include "max/max14866.h"
#endif

//---------------------------------------------------------------------------
// GLOBALS
//--------------------------------------------------------------------------

typedef void (*command_func_t)(const char *args);

typedef struct
{
    const char *command_name;
    command_func_t func;
} command_t;

// Single-threaded stdio build: one raw capture buffer (8000 samples).
static uint16_t capture_buf[U4RK_RAW_SAMPLE_COUNT];
static bool capture_valid;

//---------------------------------------------------------------------------
// VERSION COMMAND
//--------------------------------------------------------------------------
void version_cmd(const char *args)
{
    printf("pic0rick firmware v%s\n", FW_VERSION);
    printf("changes: %s\n", FW_CHANGES);
    printf("board:   %s (%s)\n", FW_BOARD, FW_CHIP);
#ifdef MUX
    printf("mux:     enabled (MAX14866)\n");
#else
    printf("mux:     disabled\n");
#endif
    printf("build:   %s\n", FW_GIT_HASH);
    printf("release: %s\n", FW_RELEASE_URL);
}

//---------------------------------------------------------------------------
// REBOOT-DFU COMMAND
//--------------------------------------------------------------------------
// Reboot into the RP2040/RP2350 USB bootloader (BOOTSEL / UF2 mass storage,
// colloquially "DFU") so a new UF2 can be flashed without pressing BOOTSEL.
void reboot_dfu_cmd(const char *args)
{
    printf("Rebooting into USB bootloader (BOOTSEL)...\n");
    fflush(stdout);
    sleep_ms(100);          // give USB CDC time to drain the message
    reset_usb_boot(0, 0);   // does not return
}

//---------------------------------------------------------------------------
// DAC COMMAND (shared spi1 MCP4812)
//--------------------------------------------------------------------------
void write_dac_cmd(const char *args)
{
    int value = atoi(args);
    if (value < 0 || value > 1023) {
        printf("DAC value must be 0..1023\n");
        return;
    }
    u4rk_dac_write((uint16_t)value);
    printf("dac=%d\n", value);
}

//---------------------------------------------------------------------------
// START ACQ COMMAND (shared pulser + ADC capture)
//--------------------------------------------------------------------------
// Usage: start acq [pon_ns] [poff_ns] [damp_ns]  (defaults 200/200/2000)
// Mapped onto the shared pulser: positive_ns=pon, negative_ns=poff,
// damp_ns=damp, positive-first. Captures U4RK_RAW_SAMPLE_COUNT (8000) samples.
void start_acq_cmd(const char *args)
{
    uint32_t pon = 200, poff = 200, damp = 2000;
    if (args != NULL) {
        char copy[64];
        strncpy(copy, args, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';
        char *tok = strtok(copy, " ");
        if (tok) { pon = (uint32_t)atoi(tok); tok = strtok(NULL, " "); }
        if (tok) { poff = (uint32_t)atoi(tok); tok = strtok(NULL, " "); }
        if (tok) { damp = (uint32_t)atoi(tok); }
    }

    if (!u4rk_pulser_configure(poff, damp, pon, U4RK_PULSE_POSITIVE_FIRST)) {
        printf("pulse config out of range (min 40 ns each)\n");
        return;
    }
    u4rk_pulser_arm();

    if (!u4rk_capture_start(capture_buf, U4RK_RAW_SAMPLE_COUNT)) {
        u4rk_pulser_disarm();
        printf("acquisition busy\n");
        return;
    }
    printf("Acquisition of %u samples started\n",
           (unsigned)U4RK_RAW_SAMPLE_COUNT);

    u4rk_capture_state_t state;
    do {
        state = u4rk_capture_poll();
    } while (state == U4RK_CAPTURE_ACTIVE);
    u4rk_pulser_disarm();

    if (state == U4RK_CAPTURE_DONE) {
        capture_valid = true;
        printf("Acquisition ended\n");
    } else {
        capture_valid = false;
        printf("ADC timeout occured\n");
    }
}

//---------------------------------------------------------------------------
// READ COMMAND (dump last capture as hex, matching the historical format)
//--------------------------------------------------------------------------
void read_cmd(const char *args)
{
    printf("----------Start of ACQ----------\n");
    for (uint32_t i = 0; i < U4RK_RAW_SAMPLE_COUNT; ++i) {
        printf("%X,", ((capture_buf[i] >> 1) & 0x3FF));
    }
    printf("\n-----------End of ACQ-----------\n");
}

command_t command_list[] = {
    {"start acq", start_acq_cmd},
    {"write dac", write_dac_cmd},
    {"read", read_cmd},
    {"version", version_cmd},
    {"reboot-dfu", reboot_dfu_cmd},
#ifdef MUX
    {"write mux", max14866},
    {"set mux", max14866_set},
    {"clear mux", max14866_clear},
#endif
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
    u4rk_acquisition_init();
    sleep_ms(100);
    u4rk_dac_init();
    sleep_ms(100);
#ifdef MUX
    max14866_init();
    sleep_ms(100);
#endif
    char input[128];
    while (true)
    {
        printf("run> ");
        fflush(stdout);
        read_input(input, sizeof(input));
        process_command(input);
    }
}
