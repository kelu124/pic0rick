//!
//! \file       adc.h
//! \author     kg
//! \date       2024-04-05
//!
//! \brief      adc pio.
//!

#ifndef ADC_H
#define ADC_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"

//---------------------------------------------------------------------------
// CONSTANTS
//---------------------------------------------------------------------------

#define PIN_BASE 0
#define SAMPLE_COUNT 16000
#define ADC_CLK 120000000

#define GPIO11 11
#define GPIO12 12
#define GPIO16 16
#define GPIO17 17

//---------------------------------------------------------------------------
// ADC INIT FUNCTION
//---------------------------------------------------------------------------

void pio_adc_init();

//---------------------------------------------------------------------------
// GPIO delays
//---------------------------------------------------------------------------
void delay_140ns();
void delay_50ns();

//---------------------------------------------------------------------------
// PULSE GPIOS INIT FUNCTION
//---------------------------------------------------------------------------
void pulse_gpios_init();

//---------------------------------------------------------------------------
// GPIO PULSE SEQUENCE FUNCTION
//---------------------------------------------------------------------------
void core1_entry();

//---------------------------------------------------------------------------
// ADC DMA FUNCTION
//---------------------------------------------------------------------------

void pio_adc_dma();

//---------------------------------------------------------------------------
// ADC CLEAN FIFO FUNCTION
//---------------------------------------------------------------------------

void pio_adc_clear_fifo();

//---------------------------------------------------------------------------
// ADC MAP PINS FUNCTION
//---------------------------------------------------------------------------

void pio_map_non_consecutive_pins(uint16_t *buffer_ptr);

//---------------------------------------------------------------------------
// ADC MAIN FUNCTION
//---------------------------------------------------------------------------

uint16_t *adc();

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------

#endif
