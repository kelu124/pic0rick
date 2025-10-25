//!
//! \file       sdio.c
//! \author     Abdelrahman Ali
//! \date       2025-01-21
//!
//! \brief      SDIO SD card implementation for high-speed data storage.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "sdio.h"
#include "sdio.pio.h"

//---------------------------------------------------------------------------
// GLOBAL VARIABLES
//---------------------------------------------------------------------------

static sdio_config_t sdio_config;
static uint8_t sdio_temp_buffer[SDIO_BLOCK_SIZE];

//---------------------------------------------------------------------------
// UTILITY FUNCTIONS
//---------------------------------------------------------------------------

static uint8_t crc7(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x09;
            } else {
                crc <<= 1;
            }
        }
    }
    return (crc >> 1) & 0x7F;
}

static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

//---------------------------------------------------------------------------
// INITIALIZATION FUNCTIONS
//---------------------------------------------------------------------------

bool sdio_init(void) {
    memset(&sdio_config, 0, sizeof(sdio_config));
    
    // Use PIO2 for SDIO (should be free)
    sdio_config.pio = pio2;
    
    // Claim state machines
    sdio_config.sm_cmd = pio_claim_unused_sm(sdio_config.pio, true);
    sdio_config.sm_dat = pio_claim_unused_sm(sdio_config.pio, true);  
    sdio_config.sm_clk = pio_claim_unused_sm(sdio_config.pio, true);
    
    // Claim DMA channels
    sdio_config.dma_chan_tx = dma_claim_unused_channel(true);
    sdio_config.dma_chan_rx = dma_claim_unused_channel(true);
    
    // Add PIO programs
    sdio_config.offset_cmd = pio_add_program(sdio_config.pio, &sdio_cmd_program);
    sdio_config.offset_dat = pio_add_program(sdio_config.pio, &sdio_dat_program);
    sdio_config.offset_clk = pio_add_program(sdio_config.pio, &sdio_clk_program);
    
    // Initialize at slow speed for card initialization
    sdio_set_clock(SDIO_INIT_CLK_HZ);
    
    // Initialize PIO programs
    sdio_cmd_program_init(sdio_config.pio, sdio_config.sm_cmd, sdio_config.offset_cmd, 
                         SDIO_CMD_PIN, SDIO_INIT_CLK_HZ);
    sdio_dat_program_init(sdio_config.pio, sdio_config.sm_dat, sdio_config.offset_dat,
                         SDIO_DAT0_PIN, SDIO_INIT_CLK_HZ);
    sdio_clk_program_init(sdio_config.pio, sdio_config.sm_clk, sdio_config.offset_clk,
                         SDIO_CLK_PIN, SDIO_INIT_CLK_HZ);
    
    // Configure DMA for data transfers
    dma_channel_config dma_config = dma_channel_get_default_config(sdio_config.dma_chan_tx);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pio_get_dreq(sdio_config.pio, sdio_config.sm_dat, true));
    
    dma_channel_set_config(sdio_config.dma_chan_tx, &dma_config, false);
    
    // Enable state machines
    pio_sm_set_enabled(sdio_config.pio, sdio_config.sm_cmd, true);
    pio_sm_set_enabled(sdio_config.pio, sdio_config.sm_dat, true);
    pio_sm_set_enabled(sdio_config.pio, sdio_config.sm_clk, true);
    
    printf("SDIO: Hardware initialized\n");
    return true;
}

void sdio_set_clock(uint32_t freq_hz) {
    float div = (float)clock_get_hz(clk_sys) / freq_hz;
    
    // Update all state machines
    pio_sm_set_clkdiv(sdio_config.pio, sdio_config.sm_cmd, div);
    pio_sm_set_clkdiv(sdio_config.pio, sdio_config.sm_dat, div);
    pio_sm_set_clkdiv(sdio_config.pio, sdio_config.sm_clk, div);
    
    printf("SDIO: Clock set to %lu Hz (div=%.2f)\n", freq_hz, div);
}

//---------------------------------------------------------------------------
// COMMAND FUNCTIONS
//---------------------------------------------------------------------------

bool sdio_send_command(uint8_t cmd, uint32_t arg, uint8_t response_type, uint32_t* response) {
    // Construct command packet (48 bits)
    uint8_t cmd_packet[6];
    cmd_packet[0] = 0x40 | cmd;  // Start bit + transmission bit + command
    cmd_packet[1] = (arg >> 24) & 0xFF;
    cmd_packet[2] = (arg >> 16) & 0xFF;
    cmd_packet[3] = (arg >> 8) & 0xFF;
    cmd_packet[4] = arg & 0xFF;
    cmd_packet[5] = (crc7(cmd_packet, 5) << 1) | 1;  // CRC7 + end bit
    
    // Convert to 32-bit words for PIO
    uint32_t cmd_words[2];
    cmd_words[0] = (cmd_packet[0] << 24) | (cmd_packet[1] << 16) | (cmd_packet[2] << 8) | cmd_packet[3];
    cmd_words[1] = (cmd_packet[4] << 24) | (cmd_packet[5] << 16);
    
    // Send command via PIO (first 32 bits, then remaining 16 bits)
    pio_sm_put_blocking(sdio_config.pio, sdio_config.sm_cmd, cmd_words[0]);  // First 32 bits
    pio_sm_put_blocking(sdio_config.pio, sdio_config.sm_cmd, cmd_words[1]);  // Remaining 16 bits
    
    // Wait for response if needed
    if (response_type != 0 && response != NULL) {
        uint32_t timeout = 1000;
        while (pio_sm_is_rx_fifo_empty(sdio_config.pio, sdio_config.sm_cmd) && timeout--) {
            sleep_us(1);
        }
        
        if (timeout == 0) {
            printf("SDIO: Command %d timeout\n", cmd);
            return false;
        }
        
        *response = pio_sm_get_blocking(sdio_config.pio, sdio_config.sm_cmd);
    }
    
    return true;
}

bool sdio_send_app_command(uint8_t cmd, uint32_t arg, uint8_t response_type, uint32_t* response) {
    uint32_t cmd55_response;
    
    // Send CMD55 first
    if (!sdio_send_command(CMD55_APP_CMD, sdio_config.rca << 16, SDIO_R1_RESPONSE, &cmd55_response)) {
        return false;
    }
    
    // Send the actual application command
    return sdio_send_command(cmd, arg, response_type, response);
}

//---------------------------------------------------------------------------
// CARD INITIALIZATION
//---------------------------------------------------------------------------

bool sdio_card_init(void) {
    uint32_t response;
    
    printf("SDIO: Starting card initialization\n");
    
    // CMD0: Go to idle state
    sleep_ms(1);
    if (!sdio_send_command(CMD0_GO_IDLE_STATE, 0, 0, NULL)) {
        printf("SDIO: CMD0 failed\n");
        return false;
    }
    
    // CMD8: Send interface condition
    if (!sdio_send_command(CMD8_SEND_IF_COND, 0x1AA, SDIO_R7_RESPONSE, &response)) {
        printf("SDIO: CMD8 failed - might be SDSC card\n");
    }
    
    // ACMD41: SD send operating condition (loop until ready)
    uint32_t timeout = 1000;
    do {
        if (!sdio_send_app_command(ACMD41_SD_SEND_OP_COND, 0x40300000, SDIO_R3_RESPONSE, &response)) {
            printf("SDIO: ACMD41 failed\n");
            return false;
        }
        sleep_ms(1);
    } while (!(response & 0x80000000) && timeout--);
    
    if (timeout == 0) {
        printf("SDIO: Card initialization timeout\n");
        return false;
    }
    
    // CMD2: All send CID
    if (!sdio_send_command(CMD2_ALL_SEND_CID, 0, SDIO_R2_RESPONSE, &response)) {
        printf("SDIO: CMD2 failed\n");
        return false;
    }
    
    // CMD3: Send relative address
    if (!sdio_send_command(CMD3_SEND_RELATIVE_ADDR, 0, SDIO_R6_RESPONSE, &response)) {
        printf("SDIO: CMD3 failed\n");
        return false;
    }
    sdio_config.rca = (response >> 16) & 0xFFFF;
    
    // CMD7: Select card
    if (!sdio_send_command(CMD7_SELECT_CARD, sdio_config.rca << 16, SDIO_R1_RESPONSE, &response)) {
        printf("SDIO: CMD7 failed\n");
        return false;
    }
    
    // CMD16: Set block length
    if (!sdio_send_command(CMD16_SET_BLOCKLEN, SDIO_BLOCK_SIZE, SDIO_R1_RESPONSE, &response)) {
        printf("SDIO: CMD16 failed\n");
        return false;
    }
    
    // Switch to high speed
    sdio_set_clock(SDIO_FAST_CLK_HZ);
    sdio_config.high_speed = true;
    sdio_config.initialized = true;
    
    printf("SDIO: Card initialized successfully (RCA=0x%04X)\n", sdio_config.rca);
    return true;
}

//---------------------------------------------------------------------------
// DATA TRANSFER FUNCTIONS
//---------------------------------------------------------------------------

bool sdio_write_block(uint32_t block_addr, const uint8_t* data) {
    if (!sdio_config.initialized) {
        printf("SDIO: Not initialized\n");
        return false;
    }
    
    uint32_t response;
    
    // CMD24: Write single block
    if (!sdio_send_command(CMD24_WRITE_BLOCK, block_addr, SDIO_R1_RESPONSE, &response)) {
        printf("SDIO: CMD24 failed\n");
        return false;
    }
    
    // Calculate CRC16 for data
    uint16_t crc = crc16(data, SDIO_BLOCK_SIZE);
    
    // Send data via DMA
    dma_channel_config dma_config = dma_channel_get_default_config(sdio_config.dma_chan_tx);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pio_get_dreq(sdio_config.pio, sdio_config.sm_dat, true));
    
    dma_channel_configure(sdio_config.dma_chan_tx, 
                         &dma_config,
                         &sdio_config.pio->txf[sdio_config.sm_dat],
                         data,
                         SDIO_BLOCK_SIZE / 4,  // 32-bit transfers
                         true);
    
    // Wait for completion
    dma_channel_wait_for_finish_blocking(sdio_config.dma_chan_tx);
    
    // Send CRC (simplified - should send proper CRC for each data line)
    pio_sm_put_blocking(sdio_config.pio, sdio_config.sm_dat, crc);
    
    return true;
}

bool sdio_write_multiple_blocks(uint32_t start_block, const uint8_t* data, uint32_t num_blocks) {
    if (!sdio_config.initialized) {
        printf("SDIO: Not initialized\n");
        return false;
    }
    
    uint32_t response;
    
    // CMD25: Write multiple blocks
    if (!sdio_send_command(CMD25_WRITE_MULTIPLE, start_block, SDIO_R1_RESPONSE, &response)) {
        printf("SDIO: CMD25 failed\n");
        return false;
    }
    
    // Send all blocks
    for (uint32_t i = 0; i < num_blocks; i++) {
        const uint8_t* block_data = data + (i * SDIO_BLOCK_SIZE);
        
        // Calculate CRC for this block
        uint16_t crc = crc16(block_data, SDIO_BLOCK_SIZE);
        
        // Send block via DMA
        dma_channel_config dma_config = dma_channel_get_default_config(sdio_config.dma_chan_tx);
        channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
        channel_config_set_read_increment(&dma_config, true);
        channel_config_set_write_increment(&dma_config, false);
        channel_config_set_dreq(&dma_config, pio_get_dreq(sdio_config.pio, sdio_config.sm_dat, true));
        
        dma_channel_configure(sdio_config.dma_chan_tx,
                             &dma_config,
                             &sdio_config.pio->txf[sdio_config.sm_dat],
                             block_data,
                             SDIO_BLOCK_SIZE / 4,
                             true);
        
        // Wait for completion
        dma_channel_wait_for_finish_blocking(sdio_config.dma_chan_tx);
        
        // Send CRC
        pio_sm_put_blocking(sdio_config.pio, sdio_config.sm_dat, crc);
    }
    
    // CMD12: Stop transmission (for multiple block write)
    sdio_send_command(12, 0, SDIO_R1_RESPONSE, &response);
    
    return true;
}

//---------------------------------------------------------------------------
// HIGH-LEVEL FUNCTIONS
//---------------------------------------------------------------------------

bool sdio_write_128kb_chunk(uint32_t start_block, const uint8_t* data) {
    if (!sdio_config.initialized) {
        printf("SDIO: Not initialized\n");
        return false;
    }
    
    printf("SDIO: Writing 128KB chunk to block %lu\n", start_block);
    
    absolute_time_t start_time = get_absolute_time();
    
    bool success = sdio_write_multiple_blocks(start_block, data, SDIO_CHUNKS_128KB);
    
    absolute_time_t end_time = get_absolute_time();
    int64_t duration_us = absolute_time_diff_us(start_time, end_time);
    
    if (success) {
        float speed_mbps = (128.0 * 1000000) / duration_us;  // 128KB in MB/s
        printf("SDIO: 128KB written in %lld us (%.2f MB/s)\n", duration_us, speed_mbps);
    }
    
    return success;
}

bool sdio_stress_test_write(uint32_t num_chunks, bool continuous) {
    if (!sdio_config.initialized) {
        if (!sdio_card_init()) {
            return false;
        }
    }
    
    // Create test pattern (incrementing pattern for verification)
    uint8_t* test_data = malloc(128 * 1024);
    if (!test_data) {
        printf("SDIO: Failed to allocate test buffer\n");
        return false;
    }
    
    for (int i = 0; i < 128 * 1024; i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }
    
    printf("SDIO: Starting stress test - %lu chunks of 128KB\n", num_chunks);
    
    absolute_time_t total_start = get_absolute_time();
    uint32_t successful_writes = 0;
    
    for (uint32_t chunk = 0; chunk < num_chunks; chunk++) {
        uint32_t start_block = chunk * SDIO_CHUNKS_128KB;
        
        if (sdio_write_128kb_chunk(start_block, test_data)) {
            successful_writes++;
        } else {
            printf("SDIO: Chunk %lu failed\n", chunk);
            if (!continuous) break;
        }
        
        // Small delay between chunks if not continuous
        if (!continuous) {
            sleep_ms(10);
        }
    }
    
    absolute_time_t total_end = get_absolute_time();
    int64_t total_duration_us = absolute_time_diff_us(total_start, total_end);
    
    float total_mb = (successful_writes * 128.0) / 1024.0;  // In MB
    float avg_speed_mbps = (total_mb * 1000000) / total_duration_us;
    
    printf("SDIO: Stress test complete\n");
    printf("  - Chunks written: %lu/%lu\n", successful_writes, num_chunks);
    printf("  - Total data: %.2f MB\n", total_mb);
    printf("  - Total time: %lld us\n", total_duration_us);
    printf("  - Average speed: %.2f MB/s\n", avg_speed_mbps);
    
    free(test_data);
    return successful_writes == num_chunks;
}

//---------------------------------------------------------------------------
// UTILITY FUNCTIONS
//---------------------------------------------------------------------------

void sdio_print_status(void) {
    printf("SDIO Status:\n");
    printf("  - Initialized: %s\n", sdio_config.initialized ? "Yes" : "No");
    printf("  - High Speed: %s\n", sdio_config.high_speed ? "Yes" : "No");
    printf("  - RCA: 0x%04X\n", sdio_config.rca);
    printf("  - PIO: %d\n", pio_get_index(sdio_config.pio));
    printf("  - State Machines: CMD=%d, DAT=%d, CLK=%d\n", 
           sdio_config.sm_cmd, sdio_config.sm_dat, sdio_config.sm_clk);
}

uint32_t sdio_get_write_speed_mbps(void) {
    // Return theoretical max speed based on clock
    if (sdio_config.high_speed) {
        return (SDIO_FAST_CLK_HZ * 4) / (8 * 1024 * 1024);  // 4-bit bus, convert to MB/s
    } else {
        return (SDIO_INIT_CLK_HZ * 4) / (8 * 1024 * 1024);
    }
}

void sdio_deinit(void) {
    if (sdio_config.pio) {
        pio_sm_set_enabled(sdio_config.pio, sdio_config.sm_cmd, false);
        pio_sm_set_enabled(sdio_config.pio, sdio_config.sm_dat, false);
        pio_sm_set_enabled(sdio_config.pio, sdio_config.sm_clk, false);
    }
    
    memset(&sdio_config, 0, sizeof(sdio_config));
    printf("SDIO: Deinitialized\n");
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
