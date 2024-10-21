//!
//! \file       dac.c
//! \author     kg
//! \date       2024-04-05
//!
//! \brief      dac spi bit banging.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "dac.h"

//---------------------------------------------------------------------------
// GLOBAL VARIABLES
//---------------------------------------------------------------------------

uint16_t input;
uint16_t data;

//---------------------------------------------------------------------------
// DAC INIT FUNCTION
//---------------------------------------------------------------------------
void dac_init()
{
    gpio_init(PIN_MOSI);
    gpio_init(PIN_CS);
    gpio_init(PIN_SCLK);
    gpio_set_dir(PIN_MOSI, GPIO_OUT);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_SCLK, GPIO_OUT);
    gpio_put(PIN_MOSI, 0);
    gpio_put(PIN_CS, 1);
    gpio_put(PIN_SCLK, 0);
}

//---------------------------------------------------------------------------
// DAC DATA PRESENTAGE CALCULATION
//---------------------------------------------------------------------------
void dac_data_calculation(uint16_t *data, uint16_t input, uint16_t config_bits)
{
    *data = 0;
    if (input == 0)
    {
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 1)
    {
        *data = (uint16_t)(38);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 2)
    {
        *data = (uint16_t)(76);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 3)
    {
        *data = (uint16_t)(115);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 4)
    {
        *data = (uint16_t)(153);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 5)
    {
        *data = (uint16_t)(191);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 6)
    {
        *data = (uint16_t)(230);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 7)
    {
        *data = (uint16_t)(268);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 8)
    {
        *data = (uint16_t)(306);
        *data = ((*data << 2) | config_bits);
    }
    else if (input == 9)
    {
        *data = (uint16_t)(358);
        *data = ((*data << 2) | config_bits);
    }
}

//---------------------------------------------------------------------------
// DAC SPI WRITE FUNCTION
//---------------------------------------------------------------------------
void dac_spi_write(uint16_t data)
{
    for (int16_t bit = 15; bit >= 0; --bit)
    {
        gpio_put(PIN_MOSI, (data >> bit) & 1);
        sleep_us(1);
        gpio_put(PIN_SCLK, 1);
        sleep_us(1);
        gpio_put(PIN_SCLK, 0);
        sleep_us(1);
    }
}

//---------------------------------------------------------------------------
// DAC WRITE FUNCTION
//---------------------------------------------------------------------------
void dac_write(uint16_t data)
{
    gpio_put(PIN_CS, 0);
    dac_spi_write(data);
    gpio_put(PIN_CS, 1);
}

//---------------------------------------------------------------------------
// DAC MAIN FUNCTION
//---------------------------------------------------------------------------
uint16_t dac()
{
    while (true)
    {
        printf("Enter a number between 0 and 9: ");
        input = getchar();
        printf("%d\n", input - '0');
        if (input >= '0' && input <= '9')
        {
            input = input - '0';
            dac_data_calculation(&data, input, 0x3000);
            break;
        }
    }

    dac_write(data);

    return input;
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
