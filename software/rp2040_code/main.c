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
#include "dac/dac.h"
#include "adc/adc.h"
#include "vga/vga.h"

//---------------------------------------------------------------------------
// MAIN FUNCTION
//---------------------------------------------------------------------------

int main()
{
    stdio_init_all();
    dac_init();
    pio_adc_init();
    pio_vga_init();
    uint16_t dac_input;
    uint16_t *adc_buffer;
    multicore_launch_core1(core1_entry);
    while (true)
    {
        dac_input = dac();
        sleep_us(10);
        adc_buffer = adc();
        sleep_us(10);
        vga(adc_buffer, dac_input);
        sleep_ms(10);
    }
}
