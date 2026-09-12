//!
//! \file       dac.h
//! \author     Abdelrahman Ali (split from max14866 by Claude, 2026-09-12)
//! \brief      MCP4812 gain DAC over the shared SPI-shifter PIO program.
//!
//! The DAC lives on the main pulse-echo board and is ALWAYS built. It reuses
//! the SPI-shifter PIO program defined in max14866.pio (that program is a
//! generic shifter; it is not specific to the MUX). Keeping the DAC separate
//! from max14866.c lets the MAX14866 MUX driver be excluded via -DMUX without
//! losing gain control.
//!

#ifndef DAC_H
#define DAC_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

//---------------------------------------------------------------------------
// CONSTANTS (MCP4812 DAC pins / clock)
//---------------------------------------------------------------------------

#define DAC_MOSI 15
#define DAC_CS   13
#define DAC_SCLK 14
#define DAC_CLK  2000000

//---------------------------------------------------------------------------
// DAC INIT FUNCTION
//---------------------------------------------------------------------------
void dac_init();

//---------------------------------------------------------------------------
// DAC DATA CALCULATION
//---------------------------------------------------------------------------
void dac_data_calculation(uint16_t *data, uint16_t input, uint16_t config_bits);

//---------------------------------------------------------------------------
// DAC WRITE FUNCTION
//---------------------------------------------------------------------------
void dac_write(uint16_t data);

//---------------------------------------------------------------------------
// DAC MAIN FUNCTION
//---------------------------------------------------------------------------
void dac(const char *input);

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------

#endif
