//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------
#include "adc/adc.h"
#include "max/max14866.h"
#include "wifi/wifi_server.h"

#include "pico/cyw43_arch.h"

//---------------------------------------------------------------------------
// WIFI CREDENTIALS  –  change these or pass via CMake -D flags
//---------------------------------------------------------------------------
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

//---------------------------------------------------------------------------
// SERIAL COMMAND TABLE  (unchanged from original)
//---------------------------------------------------------------------------

typedef void (*command_func_t)(const char *args);

typedef struct
{
    const char *command_name;
    command_func_t func;
} command_t;

command_t command_list[] = {
    {"start acq", pulse_adc_trigger},
    {"write dac", dac},
    {"write mux", max14866},
    {"set mux", max14866_set},
    {"clear mux", max14866_clear},
    {"read", adc},
};

//---------------------------------------------------------------------------
// SERIAL COMMAND PROCESSING (unchanged)
//---------------------------------------------------------------------------

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
                    args = "0";
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

//---------------------------------------------------------------------------
// NON-BLOCKING SERIAL INPUT
//---------------------------------------------------------------------------

static char serial_buf[128];
static int  serial_idx = 0;

// Returns true when a complete line has been received
static bool serial_poll(void)
{
    int ch = getchar_timeout_us(0);          // non-blocking
    if (ch == PICO_ERROR_TIMEOUT)
        return false;

    if (ch == '\r' || ch == '\n')
    {
        if (serial_idx == 0)
            return false;                    // ignore empty lines
        serial_buf[serial_idx] = '\0';
        serial_idx = 0;
        printf("\n");
        return true;
    }
    else if (ch == 127 || ch == '\b')
    {
        if (serial_idx > 0)
        {
            serial_idx--;
            printf("\b \b");
        }
    }
    else if (ch >= 32 && ch <= 126)
    {
        if (serial_idx < (int)sizeof(serial_buf) - 1)
        {
            serial_buf[serial_idx++] = (char)ch;
            putchar(ch);
        }
    }
    return false;
}

//---------------------------------------------------------------------------
// MAIN
//---------------------------------------------------------------------------

int main()
{
    stdio_init_all();

    // Wait for USB serial (optional – handy for debug)
    while (!stdio_usb_connected())
        tight_loop_contents();
    sleep_ms(100);

    // ---- Initialise WiFi --------------------------------------------------
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_FRANCE))
    {
        printf("ERROR: cyw43_arch_init failed\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    printf("Connecting to WiFi '%s' ...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 15000))
    {
        printf("ERROR: WiFi connection failed\n");
        // Continue anyway – serial still works
    }
    else
    {
        printf("WiFi connected!  IP: %s\n",
               ip4addr_ntoa(netif_ip4_addr(netif_list)));
    }

    // ---- Initialise peripherals -------------------------------------------
    pio_adc_init();
    sleep_ms(100);
    dac_init();
    sleep_ms(100);
    max14866_init();
    sleep_ms(100);

    // ---- Start HTTP server ------------------------------------------------
    http_server_init();
    printf("HTTP server listening on port 80\n");

    // ---- Main loop: service both serial and web requests ------------------
    printf("run> ");
    fflush(stdout);

    while (true)
    {
        // 1) Check if the web UI requested a pulse acquisition
        if (web_pulse_requested && !web_pulse_busy)
        {
            web_pulse_busy = true;
            web_pulse_requested = false;

            // Build the parameter string from web_pulse_params
            char params[64];
            snprintf(params, sizeof(params), "%u %u %u",
                     web_pulse_params[0],
                     web_pulse_params[1],
                     web_pulse_params[2]);

            pulse_adc_trigger(params);

            web_pulse_done = true;
            web_pulse_busy = false;
        }

        // 2) Poll serial (non-blocking)
        if (serial_poll())
        {
            process_command(serial_buf);
            printf("run> ");
            fflush(stdout);
        }

        // 3) Give the CPU a tiny break
        sleep_us(100);
    }
}
