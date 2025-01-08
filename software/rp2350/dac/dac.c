//!
//! \file       dac.c
//! \author     pic0rick_v2
//! \date       2024-01-24
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
    *data = (uint16_t)(input);
    *data = ((*data << 2) | config_bits);
}

//---------------------------------------------------------------------------
// DAC SPI WRITE FUNCTION
//---------------------------------------------------------------------------
void dac_spi_write(uint16_t data)
{
    for (int16_t bit = 15; bit >= 0; --bit)
    {
        gpio_put(PIN_MOSI, (data >> bit) & 1);
        sleep_us(2);
        gpio_put(PIN_SCLK, 1);
        sleep_us(2);
        gpio_put(PIN_SCLK, 0);
        sleep_us(2);
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
void dac(const char *input)
{
    printf("DAC writing started\n");
    dac_data_calculation(&data, atoi(input), 0x3000);
    dac_write(data);
    printf("DAC writing ended\n");
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
