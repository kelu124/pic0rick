//!
//! \file       main.c
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
#include "hardware/clocks.h"

//---------------------------------------------------------------------------
// MAIN FUNCTION
//---------------------------------------------------------------------------

int main()
{
    stdio_init_all();

    // Configure system clock for VGA timing (125 MHz for proper VGA pixel clock)
    // VGA requires ~25.175 MHz pixel clock, with PIO clock divider of 5, we need 125.875 MHz system clock
    set_sys_clock_khz(125000, true);  // 125 MHz system clock

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
