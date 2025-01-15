//!
//! \file       max14866.c 
//! \date       2025-01-06
//!
//! \brief      max14866 spi bit banging.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "max14866.h"

//---------------------------------------------------------------------------
// MAX INIT FUNCTION
//---------------------------------------------------------------------------
void max14866_init()
{
    gpio_init(MAX14866_SPI_DIN);
    gpio_init(MAX14866_SPI_SCLK);
    gpio_init(MAX14866_SPI_LE);
    gpio_init(MAX14866_SPI_SET);
    gpio_init(MAX14866_SPI_CLR);
    gpio_set_dir(MAX14866_SPI_DIN, GPIO_OUT);
    gpio_set_dir(MAX14866_SPI_SCLK, GPIO_OUT);
    gpio_set_dir(MAX14866_SPI_LE, GPIO_OUT);
    gpio_set_dir(MAX14866_SPI_SET, GPIO_OUT);
    gpio_set_dir(MAX14866_SPI_CLR, GPIO_OUT);
    gpio_put(MAX14866_SPI_DIN, 0);
    gpio_put(MAX14866_SPI_SCLK, 0);
    gpio_put(MAX14866_SPI_SET, 0);
    gpio_put(MAX14866_SPI_CLR, 0);
    gpio_put(MAX14866_SPI_LE, 1);
}

//---------------------------------------------------------------------------
// MAX SPI WRITE FUNCTION
//---------------------------------------------------------------------------
void max14866_spi_write(uint16_t data)
{
    for (int16_t bit = 15; bit >= 0; --bit)
    {
        gpio_put(MAX14866_SPI_DIN, (data >> bit) & 1);
        sleep_ms(200);
        gpio_put(MAX14866_SPI_SCLK, 1);
        sleep_ms(200);
        gpio_put(MAX14866_SPI_SCLK, 0);
        sleep_ms(200);
    }
}

//---------------------------------------------------------------------------
// MAX WRITE FUNCTION
//---------------------------------------------------------------------------
void max14866_write(uint16_t data)
{
    max14866_spi_write(data);
    gpio_put(MAX14866_SPI_LE, 0);
    sleep_us(2);
    gpio_put(MAX14866_SPI_LE, 1);
}

//---------------------------------------------------------------------------
// MAX SET FUNCTION
//---------------------------------------------------------------------------
void max14866_set(const char *input) {
    gpio_put(MAX14866_SPI_SET, 1);
    sleep_us(2);
    gpio_put(MAX14866_SPI_SET, 0);
}

//---------------------------------------------------------------------------
// MAX CLEAR FUNCTION
//---------------------------------------------------------------------------
void max14866_clear(const char *input) {
    gpio_put(MAX14866_SPI_CLR, 1);
    sleep_us(2);
    gpio_put(MAX14866_SPI_CLR, 0);
}

//---------------------------------------------------------------------------
// MAX MAIN FUNCTION
//---------------------------------------------------------------------------
void max14866(const char *input)
{
    uint16_t data = (uint16_t)strtol(input, NULL, 16);
    printf("MAX14866 writing started\n");
    max14866_write(data);
    printf("MAX14866 writing ended\n");
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
