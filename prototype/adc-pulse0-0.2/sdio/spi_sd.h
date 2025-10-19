//!
//! \file       spi_sd.h
//! \author     Abdelrahman Ali / Assistant
//! \date       2024-01-20
//!
//! \brief      High-Performance SPI SD Card Driver
//!

#ifndef SPI_SD_H
#define SPI_SD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations
bool spi_sd_init(void);
void spi_sd_deinit(void);
bool spi_sd_write_block(uint32_t block_addr, const uint8_t* data);
bool spi_sd_read_block(uint32_t block_addr, uint8_t* data);
bool spi_sd_write_multiple_blocks(uint32_t block_addr, const uint8_t* data, uint32_t block_count);
bool spi_sd_is_initialized(void);

// Command interface functions
void spi_sd_init_cmd(const char *args);
void spi_sd_speed_cmd(const char *args);
void spi_sd_verify_cmd(const char *args);
void spi_sd_debug_cmd(const char *args);
void spi_sd_bitbang_cmd(const char *args);
void spi_sd_force_spi_mode_cmd(const char *args);

#ifdef __cplusplus
}
#endif

#endif // SPI_SD_H
