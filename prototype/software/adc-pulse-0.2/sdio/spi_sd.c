//!
//! \file       spi_sd.c
//! \author     Abdelrahman Ali / Assistant
//! \date       2024-01-20
//!
//! \brief      High-Performance SPI SD Card Driver Implementation
//!

#include "spi_sd.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include <string.h> // For memset
#include "hardware/gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SPI SD Card Pin Definitions (avoid conflicts with max14866.h)
#define SPI_SD_PORT spi0
#define SPI_SD_SCK  18  // Clock pin
#define SPI_SD_MOSI 19  // Master Out (CMD pin)  
#define SPI_SD_MISO 20  // Master In (DAT0 pin)
#define SPI_SD_CS   26  // Chip Select (DAT3 pin)

// SD card commands
#define CMD0   (0x40 + 0)   // GO_IDLE_STATE
#define CMD1   (0x40 + 1)   // SEND_OP_COND
#define CMD8   (0x40 + 8)   // SEND_IF_COND
#define CMD9   (0x40 + 9)   // SEND_CSD
#define CMD17  (0x40 + 17)  // READ_SINGLE_BLOCK
#define CMD24  (0x40 + 24)  // WRITE_BLOCK
#define CMD25  (0x40 + 25)  // WRITE_MULTIPLE_BLOCK
#define CMD55  (0x40 + 55)  // APP_CMD
#define CMD58  (0x40 + 58)  // READ_OCR
#define ACMD41 (0x40 + 41)  // APP_SEND_OP_COND

// Response tokens
#define R1_IDLE_STATE   0x01
#define R1_READY        0x00
#define DATA_START_BLOCK 0xFE
#define DATA_START_BLOCK_MULTI 0xFC
#define STOP_TRAN_TOKEN 0xFD

// Global state
static int spi_dma_tx_chan = -1;
static int spi_dma_rx_chan = -1;
static bool spi_sd_initialized = false;

// Internal helper functions
// Bit-banging SPI functions for better compatibility
static void spi_bb_init(void) {
    // Configure all pins as GPIO outputs (except MISO as input)
    gpio_init(SPI_SD_SCK);
    gpio_init(SPI_SD_MOSI);
    gpio_init(SPI_SD_MISO);
    gpio_init(SPI_SD_CS);
    
    gpio_set_dir(SPI_SD_SCK, GPIO_OUT);
    gpio_set_dir(SPI_SD_MOSI, GPIO_OUT);
    gpio_set_dir(SPI_SD_MISO, GPIO_IN);
    gpio_set_dir(SPI_SD_CS, GPIO_OUT);
    
    // Set initial states
    gpio_put(SPI_SD_SCK, 0);   // Clock low
    gpio_put(SPI_SD_MOSI, 1);  // MOSI high
    gpio_put(SPI_SD_CS, 1);    // CS high (deselected)
    
    // Enable pull-up on MISO
    gpio_pull_up(SPI_SD_MISO);
}

static uint8_t spi_bb_transfer(uint8_t data) {
    uint8_t result = 0;
    
    for (int bit = 7; bit >= 0; bit--) {
        // Set MOSI
        gpio_put(SPI_SD_MOSI, (data >> bit) & 1);
        sleep_us(1);
        
        // Clock high
        gpio_put(SPI_SD_SCK, 1);
        sleep_us(1);
        
        // Read MISO
        if (gpio_get(SPI_SD_MISO)) {
            result |= (1 << bit);
        }
        
        // Clock low
        gpio_put(SPI_SD_SCK, 0);
        sleep_us(1);
    }
    
    return result;
}

static void spi_bb_send_bytes(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        spi_bb_transfer(data[i]);
    }
}

static uint8_t spi_bb_send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t crc = 0x01; // Default CRC
    if (cmd == CMD0) crc = 0x95;  // CRC for CMD0
    if (cmd == CMD8) crc = 0x87;  // CRC for CMD8
    
    // Send command
    spi_bb_transfer(cmd);
    spi_bb_transfer((arg >> 24) & 0xFF);
    spi_bb_transfer((arg >> 16) & 0xFF);
    spi_bb_transfer((arg >> 8) & 0xFF);
    spi_bb_transfer(arg & 0xFF);
    spi_bb_transfer(crc);
    
    // Wait for response (up to 16 bytes for slower cards)
    uint8_t response = 0xFF;
    for (int i = 0; i < 16; i++) {
        response = spi_bb_transfer(0xFF);
        if (response != 0xFF) break;
        sleep_us(10);
    }
    return response;
}

static inline void spi_cs_select(void) {
    gpio_put(SPI_SD_CS, 0);
}

static inline void spi_cs_deselect(void) {
    gpio_put(SPI_SD_CS, 1);
}

void spi_sd_force_spi_mode_cmd(const char *args) {
    printf("=== Force SD Card Into SPI Mode (FIXED) ===\n");
    printf("Using pure hardware SPI with proper CS pull-up timing\n");
    
    // Strategy 1: Pure Hardware SPI approach
    printf("\n--- Strategy 1: Pure Hardware SPI Init ---\n");
    
    // Initialize CS as GPIO first with pull-up (critical for SPI mode entry)
    gpio_init(SPI_SD_CS);
    gpio_set_dir(SPI_SD_CS, GPIO_OUT);
    gpio_put(SPI_SD_CS, 1); // CS high FIRST
    gpio_pull_up(SPI_SD_CS); // Add pull-up for power-up detection
    
    printf("CS configured with pull-up, waiting for card power-up...\n");
    sleep_ms(100); // Let CS settle high
    
    // Now initialize hardware SPI at very low speed
    spi_init(SPI_SD_PORT, 100 * 1000); // 100kHz 
    gpio_set_function(SPI_SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SD_MOSI, GPIO_FUNC_SPI); 
    gpio_set_function(SPI_SD_MISO, GPIO_FUNC_SPI);
    
    printf("Hardware SPI initialized at 100kHz\n");
    printf("Pin check - MISO state: %d (should vary during communication)\n", gpio_get(SPI_SD_MISO));
    
    // Long power-up delay for card initialization
    printf("Card power-up delay (500ms)...\n");
    sleep_ms(500);
    
    // Send 80+ power-up clocks with CS HIGH (pure hardware SPI)
    printf("Sending 80+ power-up clocks via hardware SPI...\n");
    uint8_t ff_clocks[12]; // 96 clock cycles
    memset(ff_clocks, 0xFF, sizeof(ff_clocks));
    spi_write_blocking(SPI_SD_PORT, ff_clocks, sizeof(ff_clocks));
    
    sleep_ms(10); // Additional settling time
    
    // Now attempt CMD0 with hardware SPI
    printf("Attempting CMD0 with hardware SPI...\n");
    
    for (int retry = 0; retry < 10; retry++) {
        // Assert CS (select card)
        gpio_put(SPI_SD_CS, 0);
        sleep_ms(1); // CS setup time
        
        // Send CMD0 (0x40, 0x00000000, CRC=0x95)
        uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
        spi_write_blocking(SPI_SD_PORT, cmd0, sizeof(cmd0));
        
        // Read response (up to 8 attempts)
        uint8_t r1 = 0xFF;
        uint8_t dummy = 0xFF;
        for (int i = 0; i < 8 && r1 == 0xFF; i++) {
            spi_write_read_blocking(SPI_SD_PORT, &dummy, &r1, 1);
        }
        
        // Deassert CS and send extra clock
        gpio_put(SPI_SD_CS, 1);
        spi_write_blocking(SPI_SD_PORT, &dummy, 1); // Extra 8 clocks as recommended
        
        printf("  CMD0 attempt %d: 0x%02X", retry + 1, r1);
        
        if (r1 == 0x01) {
            printf(" <- SUCCESS! Card entered SPI mode!\n");
            
            // Try CMD8 to verify communication
            printf("Testing CMD8 for SDHC support...\n");
            gpio_put(SPI_SD_CS, 0);
            sleep_ms(1);
            
            uint8_t cmd8[] = {0x48, 0x00, 0x00, 0x01, 0xAA, 0x87}; // CMD8 with CRC
            spi_write_blocking(SPI_SD_PORT, cmd8, sizeof(cmd8));
            
            // Read R1 + 4-byte response
            uint8_t r1_cmd8 = 0xFF;
            for (int i = 0; i < 8 && r1_cmd8 == 0xFF; i++) {
                spi_write_read_blocking(SPI_SD_PORT, &dummy, &r1_cmd8, 1);
            }
            
            if (r1_cmd8 == 0x01) {
                uint8_t ocr[4];
                spi_read_blocking(SPI_SD_PORT, 0xFF, ocr, 4);
                printf("CMD8 OK - SDHC card detected (OCR: %02X %02X %02X %02X)\n", 
                       ocr[0], ocr[1], ocr[2], ocr[3]);
            } else {
                printf("CMD8 response: 0x%02X (older SD card or error)\n", r1_cmd8);
            }
            
            gpio_put(SPI_SD_CS, 1);
            spi_write_blocking(SPI_SD_PORT, &dummy, 1);
            
            printf("✓✓✓ HARDWARE SPI COMMUNICATION WORKING! ✓✓✓\n");
            printf("Your U3/V30 card is now in SPI mode and ready!\n");
            return;
            
        } else if (r1 != 0xFF) {
            printf(" <- Card responding but error state (0x%02X)\n", r1);
        } else {
            printf(" <- No response\n");
        }
        
        sleep_ms(50); // Delay between retries
    }
    
    // Strategy 2: Check hardware connectivity
    printf("\n--- Strategy 2: Hardware Diagnostic ---\n");
    
    // Test MISO line behavior
    printf("Testing MISO line behavior:\n");
    printf("MISO with CS high: %d\n", gpio_get(SPI_SD_MISO));
    
    gpio_put(SPI_SD_CS, 0);
    sleep_ms(1);
    printf("MISO with CS low:  %d\n", gpio_get(SPI_SD_MISO));
    gpio_put(SPI_SD_CS, 1);
    
    // Send some data and check for any MISO changes
    printf("Sending test pattern to check MISO response:\n");
    gpio_put(SPI_SD_CS, 0);
    uint8_t test_pattern[] = {0xAA, 0x55, 0x00, 0xFF};
    uint8_t responses[4];
    spi_write_read_blocking(SPI_SD_PORT, test_pattern, responses, 4);
    gpio_put(SPI_SD_CS, 1);
    
    bool miso_changing = false;
    for (int i = 0; i < 4; i++) {
        printf("  Sent: 0x%02X, Got: 0x%02X\n", test_pattern[i], responses[i]);
        if (responses[i] != 0xFF) miso_changing = true;
    }
    
    printf("\n=== DIAGNOSIS FOR U3/V30 CARD ===\n");
    if (!miso_changing) {
        printf("❌ MISO stuck high - Hardware issues:\n");
        printf("  1. Check wiring (most common):\n");
        printf("     - GPIO18 → microSD CLK (pin 5)\n");
        printf("     - GPIO19 → microSD CMD (pin 3) \n");
        printf("     - GPIO20 ← microSD DAT0 (pin 7)\n");
        printf("     - GPIO26 → microSD DAT3/CS (pin 2)\n");
        printf("     - 3.3V → pin 4, GND → pin 6\n");
        printf("  2. SD card not inserted properly\n");  
        printf("  3. Power supply issue (card needs 3.3V)\n");
        printf("  4. Add 47kΩ pull-up resistor from CS to 3.3V\n");
        printf("  5. If using breakout board, check DI/DO labels\n");
    } else {
        printf("✓ MISO changing - hardware might be OK\n");
        printf("Issue might be timing or protocol related\n");
        printf("Try power-cycling the SD card completely\n");
    }
    
    printf("\n💡 TIPS FOR U3/V30 CARDS:\n");
    printf("  - These are high-speed cards, may be picky about timing\n");  
    printf("  - Try a slower, older SD card first to verify wiring\n");
    printf("  - Ensure no previous SDIO usage without power cycle\n");
    printf("  - Consider 100Ω series resistors on clock/data lines\n");
    
    printf("Force SPI mode test complete\n");
}

void spi_sd_bitbang_cmd(const char *args) {
    printf("=== SPI Bit-Bang SD Card Test ===\n");
    
    // Initialize bit-banging SPI
    spi_bb_init();
    
    printf("Bit-bang SPI initialized\n");
    printf("Pin states after init:\n");
    printf("  SCK: %d, MOSI: %d, MISO: %d, CS: %d\n",
           gpio_get(SPI_SD_SCK), gpio_get(SPI_SD_MOSI), 
           gpio_get(SPI_SD_MISO), gpio_get(SPI_SD_CS));
    
    // Test basic communication
    printf("\nTesting bit-bang communication:\n");
    gpio_put(SPI_SD_CS, 1); // CS high
    uint8_t test1 = spi_bb_transfer(0xAA);
    uint8_t test2 = spi_bb_transfer(0x55);
    printf("  Sent 0xAA, got 0x%02X\n", test1);
    printf("  Sent 0x55, got 0x%02X\n", test2);
    
    // Power-up sequence
    printf("\nSending power-up clocks...\n");
    for (int i = 0; i < 80; i++) {
        spi_bb_transfer(0xFF);
    }
    
    // Try CMD0
    printf("Attempting CMD0 with bit-bang SPI...\n");
    gpio_put(SPI_SD_CS, 0); // Select card
    sleep_ms(1);
    
    for (int retry = 0; retry < 5; retry++) {
        uint8_t r1 = spi_bb_send_cmd(CMD0, 0);
        printf("  CMD0 attempt %d: 0x%02X\n", retry + 1, r1);
        if (r1 == R1_IDLE_STATE || r1 != 0xFF) {
            printf("  Got response! (0x01 = success, others = card responding)\n");
            break;
        }
        sleep_ms(10);
    }
    
    gpio_put(SPI_SD_CS, 1); // Deselect
    printf("Bit-bang test complete\n");
}

static uint8_t spi_sd_send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t crc = 0x01; // Default CRC
    if (cmd == CMD0) crc = 0x95;  // CRC for CMD0
    if (cmd == CMD8) crc = 0x87;  // CRC for CMD8
    
    uint8_t buf[6] = {
        cmd,
        (arg >> 24) & 0xFF,
        (arg >> 16) & 0xFF, 
        (arg >> 8) & 0xFF,
        arg & 0xFF,
        crc
    };
    
    spi_write_blocking(SPI_SD_PORT, buf, 6);
    
    // Wait for response (up to 16 bytes for slower cards)
    uint8_t response = 0xFF;
    for (int i = 0; i < 16; i++) {
        spi_write_read_blocking(SPI_SD_PORT, &response, &response, 1);
        if (response != 0xFF) break;
        // Small delay between response checks
        sleep_us(10);
    }
    return response;
}

// Public API Implementation
bool spi_sd_init(void) {
    printf("=== High-Performance SPI SD Card Init ===\n");
    
    // Initialize SPI at very low speed for init (some cards need slower speeds)
    printf("Configuring SPI pins...\n");
    spi_init(SPI_SD_PORT, 250 * 1000); // 250 kHz for initialization
    
    // Configure SPI pins BEFORE checking their state
    gpio_set_function(SPI_SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SD_MISO, GPIO_FUNC_SPI);
    
    // Configure CS pin as GPIO output
    gpio_init(SPI_SD_CS);
    gpio_set_dir(SPI_SD_CS, GPIO_OUT);
    gpio_put(SPI_SD_CS, 1); // CS high (deselected)
    
    // Let pins settle
    sleep_ms(10);
    
    // Verify pin configuration
    printf("Pin setup: SCK=%d MOSI=%d MISO=%d CS=%d\n", 
           gpio_get_function(SPI_SD_SCK), gpio_get_function(SPI_SD_MOSI), 
           gpio_get_function(SPI_SD_MISO), gpio_get_function(SPI_SD_CS));
    
    // Power-up delay - SD cards need time after power-up
    printf("Waiting for card power-up...\n");
    sleep_ms(100);
    
    // Send 80+ clock cycles with CS high (card must be in SPI mode after this)
    printf("Sending power-up clocks...\n");
    uint8_t dummy[20] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    spi_write_blocking(SPI_SD_PORT, dummy, 20); // 160 clock cycles
    
    // Additional delay before first command
    sleep_ms(10);
    
    gpio_put(SPI_SD_CS, 0); // CS low (selected)
    sleep_ms(1); // Small delay after CS select
    
    // CMD0 - Reset card (try multiple times with delays)
    printf("Sending CMD0...\n");
    uint8_t r1 = 0xFF;
    for (int retry = 0; retry < 10; retry++) {
        r1 = spi_sd_send_cmd(CMD0, 0);
        printf("CMD0 attempt %d: 0x%02X\n", retry + 1, r1);
        if (r1 == R1_IDLE_STATE) {
            break;
        }
        sleep_ms(10); // Delay between retries
    }
    
    if (r1 != R1_IDLE_STATE) {
        printf("CMD0 failed after retries: 0x%02X\n", r1);
        gpio_put(SPI_SD_CS, 1); // CS high
        return false;
    }
    printf("CMD0 OK\n");
    
    // CMD8 - Check voltage range
    printf("Sending CMD8...\n");
    r1 = spi_sd_send_cmd(CMD8, 0x1AA);
    if (r1 == R1_IDLE_STATE) {
        // Read 4-byte response
        uint8_t ocr[4];
        spi_read_blocking(SPI_SD_PORT, 0xFF, ocr, 4);
        printf("CMD8 OK - SDHC supported (OCR: %02X %02X %02X %02X)\n", 
               ocr[0], ocr[1], ocr[2], ocr[3]);
    } else {
        printf("CMD8 response: 0x%02X (older SD card?)\n", r1);
    }
    
    // Initialize card with ACMD41
    printf("Sending ACMD41...\n");
    int timeout = 2000; // Increase timeout
    do {
        spi_sd_send_cmd(CMD55, 0);  // APP_CMD
        r1 = spi_sd_send_cmd(ACMD41, 0x40000000); // HCS bit set
        if (r1 == R1_READY) break;
        sleep_ms(1);
    } while (--timeout);
    
    if (r1 != R1_READY) {
        printf("ACMD41 failed: 0x%02X\n", r1);
        gpio_put(SPI_SD_CS, 1); // CS high
        return false;
    }
    printf("Card initialized\n");
    
    gpio_put(SPI_SD_CS, 1); // CS high (deselected)
    
    // Switch to high speed for data transfer
    spi_set_baudrate(SPI_SD_PORT, 12 * 1000 * 1000); // Start with 12 MHz (conservative)
    printf("SPI speed: 12 MHz\n");
    
    // Initialize DMA channels
    spi_dma_tx_chan = dma_claim_unused_channel(true);
    spi_dma_rx_chan = dma_claim_unused_channel(true);
    
    spi_sd_initialized = true;
    return true;
}

void spi_sd_deinit(void) {
    if (spi_dma_tx_chan >= 0) {
        dma_channel_unclaim(spi_dma_tx_chan);
        spi_dma_tx_chan = -1;
    }
    if (spi_dma_rx_chan >= 0) {
        dma_channel_unclaim(spi_dma_rx_chan);
        spi_dma_rx_chan = -1;
    }
    spi_sd_initialized = false;
}

bool spi_sd_write_block(uint32_t block_addr, const uint8_t* data) {
    if (!spi_sd_initialized) return false;
    
    spi_cs_select();
    
    uint8_t r1 = spi_sd_send_cmd(CMD24, block_addr);
    if (r1 != R1_READY) {
        printf("CMD24 failed: 0x%02X\n", r1);
        spi_cs_deselect();
        return false;
    }
    
    // Send data start token
    uint8_t token = DATA_START_BLOCK;
    spi_write_blocking(SPI_SD_PORT, &token, 1);
    
    // Send 512 bytes of data using DMA
    dma_channel_config tx_config = dma_channel_get_default_config(spi_dma_tx_chan);
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8);
    channel_config_set_dreq(&tx_config, spi_get_dreq(SPI_SD_PORT, true));
    channel_config_set_read_increment(&tx_config, true);
    channel_config_set_write_increment(&tx_config, false);
    
    dma_channel_configure(spi_dma_tx_chan, &tx_config,
                         &spi_get_hw(SPI_SD_PORT)->dr,  // dst
                         data,                           // src
                         512,                           // transfer count
                         true);                         // start immediately
    
    dma_channel_wait_for_finish_blocking(spi_dma_tx_chan);
    
    // Send dummy CRC (ignored in SPI mode)
    uint8_t crc[2] = {0xFF, 0xFF};
    spi_write_blocking(SPI_SD_PORT, crc, 2);
    
    // Read data response
    uint8_t response = 0xFF;
    spi_write_read_blocking(SPI_SD_PORT, &response, &response, 1);
    
    // Wait for card to finish
    int timeout = 1000;
    do {
        response = 0xFF;
        spi_write_read_blocking(SPI_SD_PORT, &response, &response, 1);
        if (response != 0x00) break;
        sleep_us(100);
    } while (--timeout);
    
    spi_cs_deselect();
    
    if ((response & 0x1F) != 0x05) {
        printf("Write failed: 0x%02X\n", response);
        return false;
    }
    
    return true;
}

bool spi_sd_read_block(uint32_t block_addr, uint8_t* data) {
    if (!spi_sd_initialized) return false;
    
    spi_cs_select();
    
    uint8_t r1 = spi_sd_send_cmd(CMD17, block_addr);
    if (r1 != R1_READY) {
        printf("CMD17 failed: 0x%02X\n", r1);
        spi_cs_deselect();
        return false;
    }
    
    // Wait for data start token
    uint8_t token;
    int timeout = 1000;
    do {
        token = 0xFF;
        spi_write_read_blocking(SPI_SD_PORT, &token, &token, 1);
        if (token == DATA_START_BLOCK) break;
        sleep_us(100);
    } while (--timeout);
    
    if (token != DATA_START_BLOCK) {
        printf("Read timeout\n");
        spi_cs_deselect();
        return false;
    }
    
    // Read 512 bytes using DMA
    dma_channel_config rx_config = dma_channel_get_default_config(spi_dma_rx_chan);
    channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
    channel_config_set_dreq(&rx_config, spi_get_dreq(SPI_SD_PORT, false));
    channel_config_set_read_increment(&rx_config, false);
    channel_config_set_write_increment(&rx_config, true);
    
    dma_channel_configure(spi_dma_rx_chan, &rx_config,
                         data,                           // dst
                         &spi_get_hw(SPI_SD_PORT)->dr,  // src
                         512,                           // transfer count
                         false);                        // don't start yet
    
    // Start DMA and send dummy bytes
    dma_channel_start(spi_dma_rx_chan);
    uint8_t dummy[512];
    memset(dummy, 0xFF, 512);
    spi_write_blocking(SPI_SD_PORT, dummy, 512);
    
    dma_channel_wait_for_finish_blocking(spi_dma_rx_chan);
    
    // Read CRC (ignored)
    uint8_t crc[2];
    spi_read_blocking(SPI_SD_PORT, 0xFF, crc, 2);
    
    spi_cs_deselect();
    return true;
}

bool spi_sd_write_multiple_blocks(uint32_t block_addr, const uint8_t* data, uint32_t block_count) {
    // For now, implement as sequential single-block writes
    // Could be optimized with CMD25 (WRITE_MULTIPLE_BLOCK) later
    for (uint32_t i = 0; i < block_count; i++) {
        if (!spi_sd_write_block(block_addr + i, data + i * 512)) {
            return false;
        }
    }
    return true;
}

bool spi_sd_is_initialized(void) {
    return spi_sd_initialized;
}

// Command interface implementations
void spi_sd_init_cmd(const char *args) {
    if (spi_sd_init()) {
        printf("SPI SD card initialized successfully!\n");
        printf("Pins: SCK=%d MOSI=%d MISO=%d CS=%d\n", 
               SPI_SD_SCK, SPI_SD_MOSI, SPI_SD_MISO, SPI_SD_CS);
    } else {
        printf("SPI SD card initialization failed\n");
    }
}

void spi_sd_speed_cmd(const char *args) {
    printf("=== High-Performance SPI SD Write Speed Test ===\n");
    
    if (!spi_sd_initialized) {
        printf("Initializing SPI SD card...\n");
        if (!spi_sd_init()) {
            printf("✗ SPI SD init failed\n");
            return;
        }
    }
    
    const uint32_t test_blocks = 64;  // 32 KB test
    uint8_t* buffer = malloc(test_blocks * 512);
    if (!buffer) {
        printf("✗ Failed to allocate buffer\n");
        return;
    }
    
    // Fill test pattern
    for (uint32_t i = 0; i < test_blocks * 512; i++) {
        buffer[i] = (uint8_t)(i & 0xFF);
    }
    
    printf("Testing SPI write speed (%lu KB, %lu blocks)...\n",
           (test_blocks * 512) / 1024, (unsigned long)test_blocks);
    
    const uint32_t base_block = 32768; // Safe area
    
    float total_speed = 0.0f;
    int successful_tests = 0;
    
    for (int test = 0; test < 5; test++) {
        printf("  Starting SPI test %d...\n", test + 1);
        absolute_time_t start = get_absolute_time();
        
        bool success = true;
        for (uint32_t i = 0; i < test_blocks; i++) {
            if (!spi_sd_write_block(base_block + test * test_blocks + i, 
                                   buffer + i * 512)) {
                printf("  Block %lu write failed\n", (unsigned long)i);
                success = false;
                break;
            }
        }
        
        if (success) {
            absolute_time_t end = get_absolute_time();
            int64_t duration_us = absolute_time_diff_us(start, end);
            float bytes_written = (float)(test_blocks * 512);
            float speed_mbps = (bytes_written * 1000000.0f) / (duration_us * 1024.0f * 1024.0f);
            total_speed += speed_mbps;
            successful_tests++;
            printf("  Test %d: %.2f MB/s (%lld us)\n", test + 1, speed_mbps, duration_us);
        }
    }
    
    free(buffer);
    
    if (successful_tests > 0) {
        float avg_speed = total_speed / successful_tests;
        printf("\nRESULT: %.2f MB/s (average of %d tests)\n", avg_speed, successful_tests);
        
        if (avg_speed >= 12.0f) {
            printf("STATUS: ✓ MEETS YOUR 12MB/s REQUIREMENT!\n");
            printf("INFO: This is %.1fx faster than your target!\n", avg_speed / 12.0f);
        } else if (avg_speed > 5.0f) {
            printf("STATUS: Good SPI performance at %.2f MB/s\n", avg_speed);
        } else {
            printf("STATUS: Limited SPI performance at %.2f MB/s\n", avg_speed);
        }
    } else {
        printf("RESULT: ALL SPI TESTS FAILED\n");
    }
}

void spi_sd_verify_cmd(const char *args) {
    printf("=== SPI SD Verify Test ===\n");
    
    if (!spi_sd_initialized) {
        printf("Initializing SPI SD card...\n");
        if (!spi_sd_init()) {
            printf("✗ SPI SD init failed\n");
            return;
        }
    }
    
    const uint32_t test_block = 65536; // Safe area
    uint8_t write_buffer[512];
    uint8_t read_buffer[512];
    
    // Create test pattern
    printf("Creating test pattern (512 bytes)...\n");
    for (int i = 0; i < 512; i++) {
        write_buffer[i] = (uint8_t)(0xA0 + (i & 0xFF));
    }
    
    // Write
    printf("Writing block %lu...\n", (unsigned long)test_block);
    if (!spi_sd_write_block(test_block, write_buffer)) {
        printf("✗ Write failed\n");
        return;
    }
    printf("✓ Write completed\n");
    
    // Small delay
    sleep_ms(10);
    
    // Read back
    printf("Reading block %lu...\n", (unsigned long)test_block);
    if (!spi_sd_read_block(test_block, read_buffer)) {
        printf("✗ Read failed\n");
        return;
    }
    printf("✓ Read completed\n");
    
    // Compare
    uint32_t mismatches = 0;
    for (int i = 0; i < 512; i++) {
        if (write_buffer[i] != read_buffer[i]) {
            if (mismatches < 5) {
                printf("  Mismatch at byte %d: wrote=0x%02X read=0x%02X\n", 
                       i, write_buffer[i], read_buffer[i]);
            }
            mismatches++;
        }
    }
    
    printf("\n=== VERIFICATION RESULT ===\n");
    if (mismatches == 0) {
        printf("✓✓✓ SUCCESS ✓✓✓\n");
        printf("All 512 bytes match perfectly!\n");
        printf("SPI SD card is working correctly!\n");
    } else {
        printf("✗ FAILED: %lu mismatches\n", (unsigned long)mismatches);
    }
}

void spi_sd_debug_cmd(const char *args) {
    printf("=== SPI SD Card Debug Info ===\n");
    
    // Initialize SPI properly first
    printf("Initializing SPI for debug...\n");
    spi_init(SPI_SD_PORT, 250 * 1000); // 250 kHz
    
    // Configure SPI pins properly
    gpio_set_function(SPI_SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SD_MISO, GPIO_FUNC_SPI);
    
    // Configure CS pin as GPIO output
    gpio_init(SPI_SD_CS);
    gpio_set_dir(SPI_SD_CS, GPIO_OUT);
    gpio_put(SPI_SD_CS, 1); // Ensure CS starts high
    
    sleep_ms(10); // Let pins settle
    
    // Check pin functions after configuration
    printf("Pin functions (should be 1 for SPI pins):\n");
    printf("  SCK (GPIO %d): %d\n", SPI_SD_SCK, gpio_get_function(SPI_SD_SCK));
    printf("  MOSI (GPIO %d): %d\n", SPI_SD_MOSI, gpio_get_function(SPI_SD_MOSI));
    printf("  MISO (GPIO %d): %d\n", SPI_SD_MISO, gpio_get_function(SPI_SD_MISO));
    printf("  CS (GPIO %d): %d (should be 5 for GPIO)\n", SPI_SD_CS, gpio_get_function(SPI_SD_CS));
    
    // Check pin states
    printf("Pin states:\n");
    printf("  SCK: %d\n", gpio_get(SPI_SD_SCK));
    printf("  MOSI: %d\n", gpio_get(SPI_SD_MOSI));
    printf("  MISO: %d\n", gpio_get(SPI_SD_MISO));
    printf("  CS: %d (should be 1 when idle)\n", gpio_get(SPI_SD_CS));
    
    // Test basic SPI communication
    printf("\nTesting basic SPI...\n");
    
    // Send some test bytes and see what comes back
    uint8_t test_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t response[4];
    
    printf("Sending 0xFF bytes with CS high:\n");
    spi_write_read_blocking(SPI_SD_PORT, test_data, response, 4);
    printf("  Response: %02X %02X %02X %02X\n", 
           response[0], response[1], response[2], response[3]);
    
    gpio_put(SPI_SD_CS, 0); // CS low
    sleep_us(10);
    printf("Sending 0xFF bytes with CS low:\n");
    spi_write_read_blocking(SPI_SD_PORT, test_data, response, 4);
    printf("  Response: %02X %02X %02X %02X\n", 
           response[0], response[1], response[2], response[3]);
    gpio_put(SPI_SD_CS, 1); // CS high
    
    // Test wiring by trying to read MISO when we control MOSI
    printf("\nWiring test:\n");
    printf("If MISO and MOSI are connected, you should see the pattern we send\n");
    uint8_t pattern[] = {0xAA, 0x55, 0x0F, 0xF0};
    uint8_t readback[4];
    gpio_put(SPI_SD_CS, 0);
    spi_write_read_blocking(SPI_SD_PORT, pattern, readback, 4);
    gpio_put(SPI_SD_CS, 1);
    printf("  Sent: %02X %02X %02X %02X\n", pattern[0], pattern[1], pattern[2], pattern[3]);
    printf("  Read: %02X %02X %02X %02X\n", readback[0], readback[1], readback[2], readback[3]);
    
    printf("Debug complete\n");
}
