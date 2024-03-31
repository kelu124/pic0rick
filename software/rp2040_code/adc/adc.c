//!
//! \file       adc.c
//! \author     Abdelrahman Ali
//! \date       2024-01-20
//!
//! \brief      adc dac pio.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "adc.h"

#include "adc.pio.h"

//---------------------------------------------------------------------------
// GLOBAL VARIABLES __asm volatile ("nop":);
//---------------------------------------------------------------------------

PIO pio_adc;
uint sm;
uint offset;
uint dma_chan;
dma_channel_config dma_chan_cfg;
uint16_t buffer[SAMPLE_COUNT];
uint8_t trigger;

uint8_t pinPositions[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

uint8_t newPositions[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

//---------------------------------------------------------------------------
// ADC INIT FUNCTION
//---------------------------------------------------------------------------

void pio_adc_init()
{
    pio_adc = pio0;
    sm = pio_claim_unused_sm(pio_adc, true);
    offset = pio_add_program(pio_adc, &adc_program);
    adc_program_init(pio_adc, sm, offset, PIN_BASE, ADC_CLK);
    dma_chan = dma_claim_unused_channel(true);
    dma_chan_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_chan_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_chan_cfg, false);
    channel_config_set_write_increment(&dma_chan_cfg, true);
    channel_config_set_dreq(&dma_chan_cfg, pio_get_dreq(pio_adc, sm, false));
}

//---------------------------------------------------------------------------
// GPIO delays
//---------------------------------------------------------------------------
void delay_140ns()
{
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
}
void delay_50ns()
{
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
    __asm volatile("nop" :);
}

//---------------------------------------------------------------------------
// PULSE GPIOS INIT FUNCTION
//---------------------------------------------------------------------------
void pulse_gpios_init()
{
    gpio_init(GPIO11);
    gpio_init(GPIO12);
    gpio_init(GPIO16);
    gpio_init(GPIO17);
    gpio_set_dir(GPIO11, GPIO_OUT);
    gpio_set_dir(GPIO12, GPIO_OUT);
    gpio_set_dir(GPIO16, GPIO_OUT);
    gpio_set_dir(GPIO17, GPIO_OUT);
    gpio_put(GPIO11, 0); // P+
    gpio_put(GPIO12, 0); // P- 
    gpio_put(GPIO16, 0); // Pdamp
    gpio_put(GPIO17, 1); // Activates OE for the pulser
}

//---------------------------------------------------------------------------
// GPIO PULSE SEQUENCE FUNCTION
//---------------------------------------------------------------------------
void core1_entry()
{
    pulse_gpios_init();
    while (true)
    {
        uint32_t triggered = multicore_fifo_pop_blocking();
        if (triggered == 1)
        {
       	    // waits 50ns
            delay_50ns();
            // P-Positive for 140ns
            gpio_put(GPIO11, 1);
            delay_140ns();
            gpio_put(GPIO11, 0);
       	    // waits 50ns
            delay_50ns();
            // P-Negative for 140ns
            gpio_put(GPIO12, 1);
            delay_140ns();
            gpio_put(GPIO12, 0);
            // Waits a bit
            delay_140ns();
            // Damps signal
            gpio_put(GPIO16, 1);
            sleep_us(7);
            gpio_put(GPIO16, 0);
        }
    }

    while (true)
    {
        tight_loop_contents();
    }
}

//---------------------------------------------------------------------------
// ADC DMA FUNCTION
//---------------------------------------------------------------------------

void pio_adc_dma()
{
    dma_channel_configure(dma_chan, &dma_chan_cfg, buffer, &pio_adc->rxf[sm], SAMPLE_COUNT, true);

    dma_channel_wait_for_finish_blocking(dma_chan);
}

//---------------------------------------------------------------------------
// ADC CLEAN FIFO FUNCTION
//---------------------------------------------------------------------------

void pio_adc_clear_fifos()
{
    pio_sm_clear_fifos(pio_adc, sm);
}

//---------------------------------------------------------------------------
// ADC MAP PINS FUNCTION
//---------------------------------------------------------------------------

void pio_map_non_consecutive_pins(uint16_t *buffer_ptr)
{
    for (uint16_t y = 0; y < SAMPLE_COUNT; ++y)
    {
        uint16_t targetPinMapping = 0;

        for (uint16_t i = 0; i < 10; ++i)
        {

            uint16_t pinValue = (buffer_ptr[y] >> pinPositions[i]) & 1;

            targetPinMapping |= (pinValue << newPositions[i]);
        }

        buffer_ptr[y] = targetPinMapping;
    }
}

// void pio_map_non_consecutive_pins(uint16_t *buffer_ptr)
// {
//     for (uint16_t y = 0; y < SAMPLE_COUNT; ++y)
//     {
//         *buffer_ptr = (*buffer_ptr >> 1 & 0x3FF);
//     }
// }

//---------------------------------------------------------------------------
// ADC MAIN FUNCTION
//---------------------------------------------------------------------------

uint16_t *adc()
{
    while (true)
    {
        printf("Enter a triggering number: ");
        trigger = getchar();
        printf("%d\n", trigger - '0');
        if (trigger == '1')
        {
            break;
        }
    }
    multicore_fifo_push_blocking(1);
    pio_adc_clear_fifos();
    pio_adc_dma();
    pio_map_non_consecutive_pins(buffer);

    printf("----------Start of ACQ----------\n");

    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i)
    {
        printf("%X,", buffer[i]);
    }

    printf("\n-----------End of ACQ-----------\n");

    return buffer;
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
