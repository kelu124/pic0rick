//!
//! \file       vga_driver.c
//! \author     Abdelrahman Ali
//! \date       2024-01-31
//!
//! \brief      vga driver.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "vga_driver.h"

#include "vga_hsync.pio.h"
#include "vga_vsync.pio.h"
#include "vga_out.pio.h"

//---------------------------------------------------------------------------
// DEFINES AND GLOBAL VARIABLES
//---------------------------------------------------------------------------

#define H_ACTIVE 655
#define V_ACTIVE 479
#define OUT_ACTIVE 319
#define TXCOUNT 153600
unsigned char vga_data_array[TXCOUNT];
char *address_pointer = &vga_data_array[0];

#define TOPMASK 0b11000111
#define BOTTOMMASK 0b11111000

#define swap(a, b) \
  {                \
    short t = a;   \
    a = b;         \
    b = t;         \
  }

#define tabspace 4

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

unsigned short cursor_y, cursor_x, textsize;
char textcolor, textbgcolor, wrap;

#define _width 640
#define _height 480

//---------------------------------------------------------------------------
// VGA DRIVER INIT
//---------------------------------------------------------------------------
void pio_vga_init()
{
  PIO pio_vga = pio1;
  uint hsync_offset = pio_add_program(pio_vga, &hsync_program);
  uint vsync_offset = pio_add_program(pio_vga, &vsync_program);
  uint vga_offset = pio_add_program(pio_vga, &vga_program);
  uint hsync_sm = pio_claim_unused_sm(pio_vga, true);
  uint vsync_sm = pio_claim_unused_sm(pio_vga, true);
  uint vga_sm = pio_claim_unused_sm(pio_vga, true);
  hsync_program_init(pio_vga, hsync_sm, hsync_offset, HSYNC);
  vsync_program_init(pio_vga, vsync_sm, vsync_offset, VSYNC);
  vga_program_init(pio_vga, vga_sm, vga_offset, VGA);
  int vga_chan_1 = dma_claim_unused_channel(true);
  int vga_chan_2 = dma_claim_unused_channel(true);
  dma_channel_config c0 = dma_channel_get_default_config(vga_chan_1);
  channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);
  channel_config_set_read_increment(&c0, true);
  channel_config_set_write_increment(&c0, false);
  channel_config_set_dreq(&c0, pio_get_dreq(pio_vga, vga_sm, true));
  channel_config_set_chain_to(&c0, vga_chan_2);
  dma_channel_configure(vga_chan_1, &c0, &pio_vga->txf[vga_sm], &vga_data_array, TXCOUNT, false);
  dma_channel_config c1 = dma_channel_get_default_config(vga_chan_2);
  channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
  channel_config_set_read_increment(&c1, false);
  channel_config_set_write_increment(&c1, false);
  channel_config_set_chain_to(&c1, vga_chan_1);
  dma_channel_configure(vga_chan_2, &c1, &dma_hw->ch[vga_chan_1].read_addr, &address_pointer, 1, false);
  pio_sm_put_blocking(pio_vga, hsync_sm, H_ACTIVE);
  pio_sm_put_blocking(pio_vga, vsync_sm, V_ACTIVE);
  pio_sm_put_blocking(pio_vga, vga_sm, OUT_ACTIVE);
  pio_enable_sm_mask_in_sync(pio_vga, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << vga_sm)));
  dma_start_channel_mask((1u << vga_chan_1));
}

//---------------------------------------------------------------------------
// VGA DRIVER DRAW PIXEL
//---------------------------------------------------------------------------
void pio_vga_draw_pixel(short x, short y, char color)
{
  if (x > 639)
    x = 639;
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (y > 479)
    y = 479;

  int pixel = ((640 * y) + x);

  if (pixel & 1)
  {
    vga_data_array[pixel >> 1] = (vga_data_array[pixel >> 1] & TOPMASK) | (color << 3);
  }
  else
  {
    vga_data_array[pixel >> 1] = (vga_data_array[pixel >> 1] & BOTTOMMASK) | (color);
  }
}

//---------------------------------------------------------------------------
// VGA DRIVER DRAW VERTICAL LINE
//---------------------------------------------------------------------------
void pio_vga_draw_v_line(short x, short y, short h, char color)
{
  for (short i = y; i < (y + h); i++)
  {
    pio_vga_draw_pixel(x, i, color);
  }
}

//---------------------------------------------------------------------------
// VGA DRIVER DRAW HORIZONTAL LINE
//---------------------------------------------------------------------------
void pio_vga_draw_h_line(short x, short y, short w, char color)
{
  for (short i = x; i < (x + w); i++)
  {
    pio_vga_draw_pixel(i, y, color);
  }
}

//---------------------------------------------------------------------------
// VGA DRIVER FILL RECTANGEL
//---------------------------------------------------------------------------
void pio_vga_fill_rect(short x, short y, short w, short h, char color)
{
  for (int i = x; i < (x + w); i++)
  {
    for (int j = y; j < (y + h); j++)
    {
      pio_vga_draw_pixel(i, j, color);
    }
  }
}

//---------------------------------------------------------------------------
// VGA DRIVER DRAW CHARACTER
//---------------------------------------------------------------------------
void pio_vga_draw_char(short x, short y, unsigned char c, char color, char bg, unsigned char size)
{
  char i, j;
  if ((x >= _width) ||
      (y >= _height) ||
      ((x + 6 * size - 1) < 0) ||
      ((y + 8 * size - 1) < 0))
    return;

  for (i = 0; i < 6; i++)
  {
    unsigned char line;
    if (i == 5)
      line = 0x0;
    else
      line = pgm_read_byte(font + (c * 5) + i);
    for (j = 0; j < 8; j++)
    {
      if (line & 0x1)
      {
        if (size == 1)
          pio_vga_draw_pixel(x + i, y + j, color);
        else
        {
          pio_vga_fill_rect(x + (i * size), y + (j * size), size, size, color);
        }
      }
      else if (bg != color)
      {
        if (size == 1)
          pio_vga_draw_pixel(x + i, y + j, bg);
        else
        {
          pio_vga_fill_rect(x + i * size, y + j * size, size, size, bg);
        }
      }
      line >>= 1;
    }
  }
}

//---------------------------------------------------------------------------
// VGA DRIVER SET CURSOR POSITION
//---------------------------------------------------------------------------
inline void pio_vga_set_cursor(short x, short y)
{
  cursor_x = x;
  cursor_y = y;
}

//---------------------------------------------------------------------------
// VGA DRIVER SET TEXT SIZE
//---------------------------------------------------------------------------
inline void pio_vga_set_text_size(unsigned char s)
{
  textsize = (s > 0) ? s : 1;
}

//---------------------------------------------------------------------------
// VGA DRIVER SET TEXT COLOR
//---------------------------------------------------------------------------
inline void pio_vga_set_text_color(char c)
{
  textcolor = textbgcolor = c;
}

//---------------------------------------------------------------------------
// VGA DRIVER SET TEXT WRAP
//---------------------------------------------------------------------------
inline void pio_vga_set_text_wrap(char w)
{
  wrap = w;
}

//---------------------------------------------------------------------------
// VGA DRIVER WRITE CHARACTER
//---------------------------------------------------------------------------
void pio_vga_write_char(unsigned char c)
{
  if (c == '\n')
  {
    cursor_y += textsize * 8;
    cursor_x = 0;
  }
  else if (c == '\r')
  {
  }
  else if (c == '\t')
  {
    int new_x = cursor_x + tabspace;
    if (new_x < _width)
    {
      cursor_x = new_x;
    }
  }
  else
  {
    pio_vga_draw_char(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
    cursor_x += textsize * 6;
    if (wrap && (cursor_x > (_width - textsize * 6)))
    {
      cursor_y += textsize * 8;
      cursor_x = 0;
    }
  }
}

//---------------------------------------------------------------------------
// VGA DRIVER WRITE STRING
//---------------------------------------------------------------------------
inline void pio_vga_write_string(char *str)
{
  while (*str)
  {
    pio_vga_write_char(*str++);
  }
}

//---------------------------------------------------------------------------
// VGA RESET FRAME FUNCTION
//---------------------------------------------------------------------------
inline void pio_vga_reset_vga()
{
  memset(vga_data_array, 0, sizeof(vga_data_array));
}