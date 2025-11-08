/*
 * SDIO Performance Benchmark for Pico
 * Based on SdFat library benchmark - modified for RP2040/2350
 */
#include <stdio.h>
#include <string.h>
#include "SdFat.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 0
/*
  Change the value of SD_CS_PIN if you are using SPI and
  your hardware does not use the default value, SS.
  Common values are:
  Arduino Ethernet shield: pin 4
  Sparkfun SD shield: pin 8
  Adafruit SD shields and modules: pin 10
*/
// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
// Assume built-in SD is used.
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(50)

// Try to select the best SD card configuration.
#ifdef HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif defined ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

// Set PRE_ALLOCATE true to pre-allocate file clusters.
const bool PRE_ALLOCATE = true;

// Set SKIP_FIRST_LATENCY true if the first read/write to the SD can
// be avoid by writing a file header or reading the first record.
const bool SKIP_FIRST_LATENCY = true;

// Size of read/write.
const size_t BUF_SIZE = 32768;

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 50;//BUF_SIZE*5;

// Write pass count.
const uint8_t WRITE_COUNT = 2;

// Read pass count.
const uint8_t READ_COUNT = 2;
//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------
// File size in bytes.
const uint32_t FILE_SIZE = 2000*BUF_SIZE;//1000000UL*FILE_SIZE_MB;

// Insure 4-byte alignment.
uint32_t buf32[(BUF_SIZE + 3)/4];
uint8_t* buf = (uint8_t*)buf32;

#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

#define error(s) printf("ERROR: %s\n", s)
#define millis() to_ms_since_boot(get_absolute_time())
#define micros() to_us_since_boot(get_absolute_time())

//------------------------------------------------------------------------------
void cidDmp() {
  cid_t cid;
  if (!sd.card()->readCID(&cid)) {
    error("readCID failed");
    return;
  }
  printf("\nManufacturer ID: 0x%02X\n", cid.mid);
  printf("OEM ID: %c%c\n", cid.oid[0], cid.oid[1]);
  printf("Product: ");
  for (uint8_t i = 0; i < 5; i++) {
    printf("%c", cid.pnm[i]);
  }
  printf("\nRevision: %d.%d\n", cid.prvN(), cid.prvM());
  printf("Serial number: 0x%08lX\n", cid.psn());
  printf("Manufacturing date: %d/%d\n", cid.mdtMonth(), cid.mdtYear());
  printf("\n");
}
//------------------------------------------------------------------------------
/*
void clearSerialInput() {
  uint32_t m = micros();
  do {
    if (Serial.read() >= 0) {
      m = micros();
    }
  } while (micros() - m < 10000);
}
*/
//------------------------------------------------------------------------------
void setup() {
  printf("\n=== SDIO Benchmark Test ===\n");
  printf("Use a freshly formatted SD for best performance.\n");
  printf("Expected performance: 15-25+ MB/s with pull-up resistors\n\n");
}
//------------------------------------------------------------------------------
int main() {
  // Set CPU to 250MHz for maximum performance
  set_sys_clock_khz(250000, true);
  stdio_init_all();
  
  float s;
  uint32_t t;
  uint32_t maxLatency;
  uint32_t minLatency;
  uint32_t totalLatency;
  bool skipLatency;
  
  // Wait for USB connection
  while (!stdio_usb_connected()) {
    tight_loop_contents();
  }
  sleep_ms(500);
  
  setup(); // Print banner
  
  // Initialize SDIO
  printf("Initializing SDIO...\n");
  if (!sd.begin(SD_CONFIG)) {
    printf("ERROR: SDIO initialization failed!\n");
    printf("Check: SD card insertion, wiring, pull-up resistors\n");
    printf("Required: 10kΩ pull-ups on DAT0-DAT3 (pins 18-21)\n");
    return -1;
  }
  
  // Print card info
  if (sd.fatType() == FAT_TYPE_EXFAT) {
    printf("Type is exFAT\n");
  } else {
    printf("Type is FAT%d\n", sd.fatType());
  }

  printf("Card size: %.3f GB (GB = 1E9 bytes)\n", sd.card()->sectorCount()*512E-9);

  cidDmp();

  // open or create file - truncate existing file.
  if (!file.open("bench.dat", O_RDWR | O_CREAT | O_TRUNC)) {
    error("open failed");
  }

  // fill buf with known data
  if (BUF_SIZE > 1) {
    for (size_t i = 0; i < (BUF_SIZE - 2); i++) {
      buf[i] = 'A' + (i % 26);
    }
    buf[BUF_SIZE-2] = '\r';
  }
  buf[BUF_SIZE-1] = '\n';

  printf("FILE_SIZE_MB = %lu\n", FILE_SIZE_MB);
  printf("BUF_SIZE = %zu bytes\n", BUF_SIZE);
  printf("Starting write test, please wait.\n\n");
  
  // do write test
  uint32_t n = FILE_SIZE/BUF_SIZE;
  printf("write speed and latency\n");
  printf("speed,max,min,avg\n");
  printf("KB/Sec,usec,usec,usec\n");
  float speed;
  uint32_t latencies[n]={};
  for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++) {
    file.truncate(0);
    if (PRE_ALLOCATE) {
      if (!file.preAllocate(FILE_SIZE)) {
        error("preAllocate failed");
      }
    }
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;
    uint32_t currentsector= file.firstSector();
    sd.card()->writeStart(currentsector, FILE_SIZE/512);
    sleep_ms(500);
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      uint32_t m = micros();
      if (file.write(buf, BUF_SIZE) != BUF_SIZE) {
        error("write failed");
      }
      m = micros() - m;
      latencies[i]=m;
      totalLatency += m;
      if (skipLatency) {
        // Wait until first write to SD, not just a copy to the cache.
        skipLatency = file.curPosition() < 512;
      } else {
        if (maxLatency < m) {
          maxLatency = m;
        }
        if (minLatency > m) {
          minLatency = m;
        }
      }
    }
    file.sync();
    t = millis() - t;
    s = file.fileSize();
    printf("%.1f,%lu,%lu,%lu\n", s/t, maxLatency, minLatency, totalLatency/n);
  }
  printf("\nStarting read test, please wait.\n");
  printf("\nread speed and latency\n");
  printf("speed,max,min,avg\n");
  printf("KB/Sec,usec,usec,usec\n");

  // do read test
  for (uint8_t nTest = 0; nTest < READ_COUNT; nTest++) {
    file.rewind();
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    skipLatency = SKIP_FIRST_LATENCY;
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      buf[BUF_SIZE-1] = 0;
      uint32_t m = micros();
      int32_t nr = file.read(buf, BUF_SIZE);
      if (nr != BUF_SIZE) {
        error("read failed");
      }
      m = micros() - m;
      totalLatency += m;
      if (buf[BUF_SIZE-1] != '\n') {

        error("data check error");
      }
      if (skipLatency) {
        skipLatency = false;
      } else {
        if (maxLatency < m) {
          maxLatency = m;
        }
        if (minLatency > m) {
          minLatency = m;
        }
      }
    }
    s = file.fileSize();
    t = millis() - t;
    printf("%.1f,%lu,%lu,%lu\n", s/t, maxLatency, minLatency, totalLatency/n);
  }
  printf("\nDone\n");
  printf("\n=== BENCHMARK COMPLETE ===\n");
  printf("Results saved to 'bench.dat' on SD card\n");
  printf("Performance numbers above show KB/Sec (divide by 1024 for MB/s)\n");
  
  file.close();
  sd.end();

  return 0;
}
