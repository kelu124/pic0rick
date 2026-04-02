//!
//! \file       vga.h
//! \date       2024-01-31
//!
//! \brief      vga pio.
//!

#ifndef VGA_H
#define VGA_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "vga_driver.h"

//---------------------------------------------------------------------------
// CONSTANTS
//---------------------------------------------------------------------------
#define SIGNAL_COUNT 16000
#define DECIMATED_SIGNAL 400
#define DECIMATED_FACTOR 40

#define WIDTH 640
#define HEIGHT 480

//---------------------------------------------------------------------------
// VGA DRAW CONTAINER FUNCTION
//---------------------------------------------------------------------------
void pio_vga_draw_container();

//---------------------------------------------------------------------------
// VGA GET MAX SIGNAL AMPLITUDE FUNCTION
//---------------------------------------------------------------------------
uint16_t pio_vga_get_max_signal_amplitude(uint16_t max_signal_amplitude[DECIMATED_FACTOR]);

//---------------------------------------------------------------------------
// VGA GET DECIMATED SIGNAL FUNCTION
//---------------------------------------------------------------------------
void pio_vga_get_decimated_signal(uint16_t decimate_signal[SIGNAL_COUNT]);

//---------------------------------------------------------------------------
// VGA DRAW FRAME FUNCTION
//---------------------------------------------------------------------------
void pio_vga_draw_frame(uint16_t dac);

//---------------------------------------------------------------------------
// VGA MAIN FUNCTION
//---------------------------------------------------------------------------
void vga(uint16_t signal[SIGNAL_COUNT], uint16_t dac_input);

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------

#endif
