//!
//! \file       sdio.h
//! \author     Abdelrahman Ali
//! \date       2025-01-21
//!
//! \brief      SDIO SD card interface for high-speed data storage.
//!

#ifndef SDIO_H
#define SDIO_H

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

//---------------------------------------------------------------------------
// CONSTANTS
//---------------------------------------------------------------------------

// SDIO pin definitions (using pic0rick J6 connector: pins 18-22, 26-27)
#define SDIO_CLK_PIN    18  // SD card clock
#define SDIO_CMD_PIN    19  // SD card command
#define SDIO_DAT0_PIN   20  // SD card data line 0
#define SDIO_DAT1_PIN   21  // SD card data line 1  
#define SDIO_DAT2_PIN   22  // SD card data line 2
#define SDIO_DAT3_PIN   26  // SD card data line 3
// Note: GPIO 27 from J6 remains available for future expansion

// SDIO timing and protocol constants
#define SDIO_INIT_CLK_HZ    400000      // 400kHz for initialization
#define SDIO_FAST_CLK_HZ    25000000    // 25MHz for data transfer
#define SDIO_BLOCK_SIZE     512         // Standard SD block size
#define SDIO_CHUNKS_128KB   256         // 128KB / 512 bytes = 256 blocks

// Command definitions
#define CMD0_GO_IDLE_STATE      0
#define CMD2_ALL_SEND_CID       2
#define CMD3_SEND_RELATIVE_ADDR 3
#define CMD7_SELECT_CARD        7
#define CMD8_SEND_IF_COND       8
#define CMD9_SEND_CSD          9
#define CMD16_SET_BLOCKLEN     16
#define CMD17_READ_BLOCK       17
#define CMD18_READ_MULTIPLE    18
#define CMD24_WRITE_BLOCK      24
#define CMD25_WRITE_MULTIPLE   25
#define CMD55_APP_CMD          55
#define ACMD41_SD_SEND_OP_COND 41

// Response types
#define SDIO_R1_RESPONSE       1
#define SDIO_R2_RESPONSE       2
#define SDIO_R3_RESPONSE       3
#define SDIO_R6_RESPONSE       6
#define SDIO_R7_RESPONSE       7

//---------------------------------------------------------------------------
// STRUCTURES
//---------------------------------------------------------------------------

typedef struct {
    PIO pio;
    uint sm_cmd;        // State machine for command line
    uint sm_dat;        // State machine for data lines
    uint sm_clk;        // State machine for clock
    uint offset_cmd;
    uint offset_dat;
    uint offset_clk;
    uint dma_chan_tx;
    uint dma_chan_rx;
    uint32_t rca;       // Relative Card Address
    bool initialized;
    bool high_speed;
} sdio_config_t;

//---------------------------------------------------------------------------
// FUNCTION DECLARATIONS
//---------------------------------------------------------------------------

// Initialization functions
bool sdio_init(void);
bool sdio_card_init(void);
void sdio_set_clock(uint32_t freq_hz);

// Command functions
bool sdio_send_command(uint8_t cmd, uint32_t arg, uint8_t response_type, uint32_t* response);
bool sdio_send_app_command(uint8_t cmd, uint32_t arg, uint8_t response_type, uint32_t* response);

// Data transfer functions  
bool sdio_write_block(uint32_t block_addr, const uint8_t* data);
bool sdio_write_multiple_blocks(uint32_t start_block, const uint8_t* data, uint32_t num_blocks);
bool sdio_read_block(uint32_t block_addr, uint8_t* data);
bool sdio_read_multiple_blocks(uint32_t start_block, uint8_t* data, uint32_t num_blocks);

// High-level functions
bool sdio_write_128kb_chunk(uint32_t start_block, const uint8_t* data);
bool sdio_stress_test_write(uint32_t num_chunks, bool continuous);

// Utility functions
void sdio_print_status(void);
uint32_t sdio_get_write_speed_mbps(void);

// Clean up
void sdio_deinit(void);

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------

#endif // SDIO_H
