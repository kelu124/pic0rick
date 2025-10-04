//!
//! \file       vga_driver.h
//! \author     kg
//! \date       2024-04-05
//!
//! \brief      vga driver.
//!

#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "font.c"

//---------------------------------------------------------------------------
// VGA DRIVER CONSTANTS
//---------------------------------------------------------------------------

#define HSYNC 21 
#define VSYNC 28 
#define VGA   18

#define BUS_R_BIT 0
#define BUS_G_BIT 1
#define BUS_B_BIT 2
#define RED   (1u << BUS_R_BIT)
#define GREEN (1u << BUS_G_BIT)
#define BLUE  (1u << BUS_B_BIT)

//---------------------------------------------------------------------------
// VGA DRIVER INIT
//---------------------------------------------------------------------------
void pio_vga_init();
//---------------------------------------------------------------------------
// VGA DRIVER DRAW PIXEL
//---------------------------------------------------------------------------
void pio_vga_draw_pixel(short x, short y, char color);
//---------------------------------------------------------------------------
// VGA DRIVER DRAW VERTICAL LINE
//---------------------------------------------------------------------------
void pio_vga_draw_v_line(short x, short y, short h, char color);
//---------------------------------------------------------------------------
// VGA DRIVER DRAW HORIZONTAL LINE
//---------------------------------------------------------------------------
void pio_vga_draw_h_line(short x, short y, short w, char color);
//---------------------------------------------------------------------------
// VGA DRIVER FILL RECTANGEL
//---------------------------------------------------------------------------
void pio_vga_fill_rect(short x, short y, short w, short h, char color);
//---------------------------------------------------------------------------
// VGA DRIVER DRAW CHARACTER
//---------------------------------------------------------------------------
void pio_vga_draw_char(short x, short y, unsigned char c, char color, char bg, unsigned char size);
//---------------------------------------------------------------------------
// VGA DRIVER SET CURSOR POSITION
//---------------------------------------------------------------------------
void pio_vga_set_cursor(short x, short y);
//---------------------------------------------------------------------------
// VGA DRIVER SET TEXT COLOR
//---------------------------------------------------------------------------
void pio_vga_set_text_color(char c);
//---------------------------------------------------------------------------
// VGA DRIVER SET TEXT SIZE
//---------------------------------------------------------------------------
void pio_vga_set_text_size(unsigned char s);
//---------------------------------------------------------------------------
// VGA DRIVER SET TEXT WRAP
//---------------------------------------------------------------------------
void pio_vga_set_text_wrap(char w);
//---------------------------------------------------------------------------
// VGA DRIVER WRITE CHARACTER
//---------------------------------------------------------------------------
void pio_vga_write_char(unsigned char c);
//---------------------------------------------------------------------------
// VGA DRIVER WRITE STRING
//---------------------------------------------------------------------------
void pio_vga_write_string(char *str);
//---------------------------------------------------------------------------
// VGA RESET FRAME FUNCTION
//---------------------------------------------------------------------------
void pio_vga_reset_vga();

#endif