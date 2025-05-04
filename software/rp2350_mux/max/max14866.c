//!
//! \file       max14866.c
//! \author     Abdelrahman Ali
//! \date       2025-01-06
//!
//! \brief      max14866 spi bit banging.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "max14866.h"

#include "max14866.pio.h"


PIO pio_max;
uint sm4;

//---------------------------------------------------------------------------
// MAX INIT FUNCTION
//---------------------------------------------------------------------------
void max14866_init()
{
    pio_max = pio1;
    sm4 = 0;
    uint offset = pio_add_program(pio_max, &max14866_program);
    max14866_program_init(pio_max, sm4, offset, MAX14866_SPI_DIN, MAX14866_SPI_SCLK, MAX14866_CLK);
    gpio_init(MAX14866_SPI_LE);
    gpio_init(MAX14866_SPI_SET);
    gpio_init(MAX14866_SPI_CLR);
    gpio_set_dir(MAX14866_SPI_LE, GPIO_OUT);
    gpio_set_dir(MAX14866_SPI_SET, GPIO_OUT);
    gpio_set_dir(MAX14866_SPI_CLR, GPIO_OUT);
    gpio_put(MAX14866_SPI_SET, 0);
    gpio_put(MAX14866_SPI_CLR, 0);
    gpio_put(MAX14866_SPI_LE, 1);
}

//---------------------------------------------------------------------------
// MAX WRITE FUNCTION
//---------------------------------------------------------------------------
void max14866_write(uint16_t data)
{
    max14866_wait_idle(pio_max, sm4);
    gpio_put(MAX14866_SPI_LE, 0);
    sleep_us(10);
    gpio_put(MAX14866_SPI_LE, 1);
    max14866_put(pio_max, sm4, data);
    max14866_wait_idle(pio_max, sm4);
    gpio_put(MAX14866_SPI_LE, 0);
    sleep_us(10);
    gpio_put(MAX14866_SPI_LE, 1);
}

//---------------------------------------------------------------------------
// MAX SET FUNCTION
//---------------------------------------------------------------------------
void max14866_set(const char *input) {
    gpio_put(MAX14866_SPI_SET, 1);
    sleep_us(5);
    gpio_put(MAX14866_SPI_SET, 0);
}

//---------------------------------------------------------------------------
// MAX CLEAR FUNCTION
//---------------------------------------------------------------------------
void max14866_clear(const char *input) {
    gpio_put(MAX14866_SPI_CLR, 1);
    sleep_us(5);
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
