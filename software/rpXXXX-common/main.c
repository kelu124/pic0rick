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

//---------------------------------------------------------------------------
// GLOBALS
//--------------------------------------------------------------------------

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
