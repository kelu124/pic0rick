//!
//! \file       dac.h
//! \date       2024-01-24
//!
//! \brief      dac spi bit banging.
//!

#ifndef DAC_H
#define DAC_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

//---------------------------------------------------------------------------
// CONSTANTS
//---------------------------------------------------------------------------

#define PIN_MOSI 15
#define PIN_CS 13
#define PIN_SCLK 14

//---------------------------------------------------------------------------
// DAC INIT FUNCTION
//---------------------------------------------------------------------------
void dac_init();

//---------------------------------------------------------------------------
// DAC DATA PRESENTAGE CALCULATION
//---------------------------------------------------------------------------
void dac_data_calculation(uint16_t *data, uint16_t input, uint16_t config_bits);

//---------------------------------------------------------------------------
// DAC SPI WRITE FUNCTION
//---------------------------------------------------------------------------
void dac_spi_write(uint16_t data);

//---------------------------------------------------------------------------
// DAC WRITE FUNCTION
//---------------------------------------------------------------------------
void dac_write(uint16_t data);

//---------------------------------------------------------------------------
// DAC MAIN FUNCTION
void dac(const char *input);

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------

#endif