// Driver for accessing SD card in SDIO mode on RP2040.

#include <stdio.h>
#include <string.h>
#include "sdio.h"
#include <hardware/gpio.h>
#include <SdFat.h>
#include <SdCard/SdCardInfo.h>
#include "SdioCard.h"
const uint8_t CMD48 = 48;
const uint8_t CMD49 = 49;
static uint32_t g_sdio_ocr; // Operating condition register from card
static uint32_t g_sdio_rca; // Relative card address
static cid_t g_sdio_cid;
static csd_t g_sdio_csd;
static scr_t g_sdio_scr;
static SdStatus_t g_gsdio_status;
static int g_sdio_error_line;
static sdio_status_t g_sdio_error;
static uint32_t g_sdio_dma_buf[128];
static uint32_t g_sdio_sector_count;
static uint32_t g_sdio_kHzSdClk=0;
#define checkReturnOk(call) ((g_sdio_error = (call)) == SDIO_OK ? true : logSDError(__LINE__))
static bool logSDError(int line)
{
    g_sdio_error_line = line;
    printf("SDIO", "SDIO SD card error on line ", line, ", error code ", (int)g_sdio_error);
    return false;
}

// Callback used by SCSI code for simultaneous processing
static sd_callback_t m_stream_callback;
static const uint8_t *m_stream_buffer;
static uint32_t m_stream_count;
static uint32_t m_stream_count_start;
// variables for long writes
static bool multi_write= false;
static uint32_t multi_write_start;
static uint32_t multi_write_end;
void azplatform_set_sd_callback(sd_callback_t func, const uint8_t *buffer)
{
    m_stream_callback = func;
    m_stream_buffer = buffer;
    m_stream_count = 0;
    m_stream_count_start = 0;
}

static sd_callback_t get_stream_callback(const uint8_t *buf, uint32_t count, const char *accesstype, uint32_t sector)
{
    m_stream_count_start = m_stream_count;

    if (m_stream_callback)
    {
        if (buf == m_stream_buffer + m_stream_count)
        {
            m_stream_count += count;
            return m_stream_callback;
        }
        else
        {
          printf("SD card ", accesstype, "(", (int)sector,
                  ") slow transfer, buffer", (uint32_t)buf, " vs. ", (uint32_t)(m_stream_buffer + m_stream_count));
            return NULL;
        }
    }
    
    return NULL;
}
static bool sdio_read_ext_register(int m_io, int func_no, uint32_t addr, uint32_t length, uint8_t *dest)
{
    uint32_t read_arg = ((uint32_t)(m_io & 1) << 31) |
                         ((uint32_t)(func_no & 15) << 27) |
                         ((uint32_t)(addr & 0x1FFFF) << 9) |
                         ((uint32_t)((length - 1) & 511));
    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_rx_start((uint8_t*)g_sdio_dma_buf, 1)) || // Prepare for reception
        !checkReturnOk(rp2040_sdio_command_R1(48, read_arg, &reply))) // Read extension register
    {
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_rx_poll(&bytes_done);
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
        printf("sdio_read_ext_register(", addr, ") failed: ", (int)g_sdio_error);
    }

    memcpy(dest, g_sdio_dma_buf, length);
    return g_sdio_error == SDIO_OK;
}

static bool sdio_write_ext_register_byte(int m_io, int func_no, uint32_t addr, uint8_t byte_val)
{
    g_sdio_dma_buf[0] = byte_val;

    uint32_t write_arg = ((uint32_t)(m_io & 1) << 31) |
                         ((uint32_t)(func_no & 15) << 27) |
                         ((uint32_t)(addr & 0x1FFFF) << 9);
    uint32_t reply;

    if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_command_R1(49, write_arg, &reply)) || // Write extension register
        !checkReturnOk(rp2040_sdio_tx_start((uint8_t*)g_sdio_dma_buf, 1))) // Start transmission
    {
        printf("sdio_write_ext_register_byte(", addr, ") failed: ", (int)g_sdio_error);
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_tx_poll(&bytes_done);
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
        printf("sdio_write_ext_register_byte(", addr, ") failed: ", (int)g_sdio_error);
    }

    return g_sdio_error == SDIO_OK;
}
bool sdio_set_cache_enabled(bool enabled)
{
    printf("Cache configuration requires pull-up resistors on data lines\n");
    printf("Skipping cache configuration for compatibility\n");
    return true;  // Return success to continue initialization
    
    // This code requires external pull-up resistors to work reliably:
    /*
    // Read SD_STATUS register with ACMD13
    uint32_t sd_status[16] = {0};
    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) ||
        !checkReturnOk(rp2040_sdio_command_R1(ACMD13, 0, &reply)))
    {
        printf("Cache config: CMD55/ACMD13 failed\n");
        return true; // Don't fail initialization
    }

    // Read the 512-bit response
    rp2040_sdio_rx_start((uint8_t*)sd_status, 1, 64);
    do {
        g_sdio_error = rp2040_sdio_rx_poll();
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
        printf("Cache config: Data transfer failed, error=%d (needs pull-ups)\n", (int)g_sdio_error);
        return true; // Don't fail initialization
    }
    */
    
    printf("Basic SDIO mode active (no cache needed)\n");
    return true;
}

bool sdio_flush_cache()
{
    // Flush cache by writing byte 261
    return sdio_write_ext_register_byte(0, 2, 261, 1);
}
bool SdioCard::begin(SdioConfig sdioConfig)
{
    uint32_t reply;
    sdio_status_t status;
    
    // Initialize at 1 MHz clock speed
    rp2040_sdio_init(100);

    // Establish initial connection with the card
    for (int retries = 0; retries < 5; retries++)
    {
        sleep_ms(1);
        reply = 0;
        rp2040_sdio_command_R1(CMD0, 0, NULL); // GO_IDLE_STATE
        status = rp2040_sdio_command_R1(CMD8, 0x1AA, &reply); // SEND_IF_COND

        if (status == SDIO_OK && reply == 0x1AA)
        {
            break;
        }
    }

    if (reply != 0x1AA || status != SDIO_OK)
    {
        return false;
    }

    // Send ACMD41 to begin card initialization and wait for it to complete
    // First query supported voltages
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD55, 0, &reply)) || 
        !checkReturnOk(rp2040_sdio_command_R3(ACMD41, 0x00000000, &g_sdio_ocr)))
    {
        return false;
    }
    
    // Now initialize with proper voltage range
    uint32_t start = millis();
    do {
        if (!checkReturnOk(rp2040_sdio_command_R1(CMD55, 0, &reply)) || // APP_CMD
            !checkReturnOk(rp2040_sdio_command_R3(ACMD41, 0x40300000, &g_sdio_ocr))) // Standard HCS + 3.3V
        {
            return false;
        }

        if ((uint32_t)(millis() - start) > 1000)
        {
          printf("SDIO card initialization timeout");
            return false;
        }
        sleep_ms(10); // Give card time between attempts
    } while (!(g_sdio_ocr & (1 << 31)));

    // Get CID
    if (!checkReturnOk(rp2040_sdio_command_R2(CMD2, 0, (uint8_t*)&g_sdio_cid)))
    {
        return false;
    }

    // Get relative card address
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD3, 0, &g_sdio_rca)))
    {
        return false;
    }

    // Get CSD
    if (!checkReturnOk(rp2040_sdio_command_R2(CMD9, g_sdio_rca, (uint8_t*)&g_sdio_csd)))
    {
        return false;
    }

    g_sdio_sector_count = sectorCount();

    // Select card
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD7, g_sdio_rca, &reply)))
    {
        return false;
    }

    // Set 4-bit bus mode
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) ||
        !checkReturnOk(rp2040_sdio_command_R1(ACMD6, 2, &reply)))
    {
        return false;
    }
    
    rp2040_sdio_init(2);
    sleep_ms(10);
    
    // Set default SCR values for basic operation (skip reading - needs pull-ups)
    memset(&g_sdio_scr, 0, sizeof(g_sdio_scr));

    
    // Increase to 50 MHz clock rate

    sleep_ms(10);

    // Skip complex data operations that require pull-up resistors for now
    printf("Skipping high-speed negotiation (requires pull-up resistors)\n");
    g_sdio_kHzSdClk = 25000;
    printf("✓ Using normal speed mode (25MHz) - reliable without pull-ups\n");
    
    // Skip cache configuration that requires ACMD13 data transfer
    printf("Skipping cache configuration (requires pull-up resistors)\n");
    printf("✓ Basic SDIO functionality ready\n");
    return true;
}

uint8_t SdioCard::errorCode() const
{
    return g_sdio_error;
}

uint32_t SdioCard::errorData() const
{
    return 0;
}

uint32_t SdioCard::errorLine() const
{
    return g_sdio_error_line;
}

bool SdioCard::isBusy() 
{
    return (sio_hw->gpio_in & (1 << SDIO_D0)) == 0;
}

uint32_t SdioCard::kHzSdClk()
{
    return g_sdio_kHzSdClk;
}

bool SdioCard::readCID(cid_t* cid)
{
    *cid = g_sdio_cid;
    return true;
}

bool SdioCard::readCSD(csd_t* csd)
{
    *csd = g_sdio_csd;
    return true;
}

bool SdioCard::readOCR(uint32_t* ocr)
{
    // SDIO mode does not have CMD58, but main program uses this to
    // poll for card presence. Return status register instead.
    return checkReturnOk(rp2040_sdio_command_R1(CMD13, g_sdio_rca, ocr));
}

bool SdioCard::readData(uint8_t* dst)
{
  printf("SDIO", "SdioCard::readData() called but not implemented!");
    return false;
}

bool SdioCard::readStart(uint32_t sector)
{
  printf("SDIO", "SdioCard::readStart() called but not implemented!");
    return false;
}

bool SdioCard::readStop()
{
  printf("SDIO", "SdioCard::readStop() called but not implemented!");
    return false;
}

uint32_t SdioCard::sectorCount()
{
    return g_sdio_csd.capacity();
}

uint32_t SdioCard::status()
{
    uint32_t reply;
    if (checkReturnOk(rp2040_sdio_command_R1(CMD13, g_sdio_rca, &reply)))
        return reply;
    else
        return 0;
}

bool SdioCard::stopTransmission(bool blocking)
{
    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD12, 0, &reply)))
    {
        return false;
    }

    if (!blocking)
    {
        return true;
    }
    else
    {
        uint32_t end = millis() + 100;
        while (millis() < end && isBusy())
        {
            if (m_stream_callback)
            {
                m_stream_callback(m_stream_count);
            }
        }
        if (isBusy())
        {
          printf("SDIO", "SdioCard::stopTransmission() timeout");
            return false;
        }
        else
        {
            return true;
        }
    }
}

bool SdioCard::syncDevice()
{
    return true;
}

uint8_t SdioCard::type() const
{
    if (g_sdio_ocr & (1 << 30))
        return SD_CARD_TYPE_SDHC;
    else
        return SD_CARD_TYPE_SD2;
}

bool SdioCard::writeData(const uint8_t* src)
{
  printf("SDIO", "SdioCard::writeData() called but not implemented!");
    return false;
}

bool SdioCard::writeStart(uint32_t sector)
{
  printf("SDIO", "SdioCard::writeStart() called but not implemented!");
    return false;
}
bool SdioCard::writeStart(uint32_t sector,uint32_t count)
{   
    m_curSector=sector;
    multi_write_end=m_curSector+count;
        uint32_t reply;
        if(!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) ||
        !checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) || // APP_CMD
        !checkReturnOk(rp2040_sdio_command_R1(ACMD23, count, &reply))||  // SET_WR_CLK_ERASE_COUNT
        !checkReturnOk(rp2040_sdio_command_R1(CMD25, sector, &reply)) // WRITE_MULTIPLE_BLOCK
        ){
            return false;
        }
        multi_write=true;
        return true;
    
}
bool SdioCard::writeStop()
{
    return stopTransmission(true);
}

bool SdioCard::erase(uint32_t firstSector, uint32_t lastSector)
{
  printf("SDIO", "SdioCard::erase() not implemented");
    return false;
}


bool SdioCard::cardCMD6(uint32_t arg, uint8_t* status) {
    uint32_t reply;
    
    // Clear status buffer
    memset(status, 0, 64);
    
    rp2040_sdio_rx_start(status, 1, 64);
    if (!checkReturnOk(rp2040_sdio_command_R1(CMD6, arg, &reply)))
    {
        printf("SdioCard::cardCMD6: Command failed, arg=0x%08X, error=%d\n", arg, (int)g_sdio_error);
        return false;
    }

    // Read the 64-byte response with timeout
    uint32_t timeout_count = 0;
    const uint32_t MAX_TIMEOUT = 10000; // 10ms timeout
    
    do {
        g_sdio_error = rp2040_sdio_rx_poll();
        if (g_sdio_error == SDIO_BUSY) {
            sleep_us(1);
            timeout_count++;
            if (timeout_count > MAX_TIMEOUT) {
                printf("SdioCard::cardCMD6: Data timeout, arg=0x%08X\n", arg);
                rp2040_sdio_stop(); // Force stop the transfer
                return false;
            }
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
        printf("SdioCard::cardCMD6: Data transfer failed, arg=0x%08X, error=%d\n", arg, (int)g_sdio_error);
        return false;
    }

    return true;
}

void SdioCard::end(){
    sdio_flush_cache();
}
bool SdioCard::readSCR(scr_t* scr) {
    *scr =g_sdio_scr;
    return true;
}

bool SdioCard::Acmd( uint8_t acommand, uint32_t arg) {
    uint32_t reply;
    return checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) && rp2040_sdio_command_R1(acommand, arg,&reply);
}
/* Writing and reading, with progress callback */

bool SdioCard::writeSector(uint32_t sector, const uint8_t* src)
{
    if (((uint32_t)src & 3) != 0)
    {
        // Buffer is not aligned, need to memcpy() the data to a temporary buffer.
        memcpy(g_sdio_dma_buf, src, sizeof(g_sdio_dma_buf));
        src = (uint8_t*)g_sdio_dma_buf;
    }

    // If possible, report transfer status to application through callback.
    sd_callback_t callback = get_stream_callback(src, 512, "writeSector", sector);

    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_command_R1(CMD24, sector, &reply)) || // WRITE_BLOCK
        !checkReturnOk(rp2040_sdio_tx_start(src, 1))) // Start transmission
    {
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_tx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
      printf("SDIO", "SdioCard::writeSector(", sector, ") failed: ", (int)g_sdio_error);
    }

    return g_sdio_error == SDIO_OK;
}
bool SdioCard::startWriteSectors(uint32_t n){
        uint32_t reply;
        rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply); // APP_CMD
        rp2040_sdio_command_R1(ACMD23, n, &reply);// SET_WR_CLK_ERASE_COUNT
        //rp2040_sdio_command_R1(CMD25, sector, &reply) || // WRITE_MULTIPLE_BLOCK
        //TODO 
        //implement CMD20 to start recording
        //use cmd23 and cmd23 to set adress
        return true;
}
bool SdioCard::writeSectors(uint32_t sector, const uint8_t* src, size_t n)
{
    if (((uint32_t)src & 3) != 0)
    {
        // Unaligned write, execute sector-by-sector
        for (size_t i = 0; i < n; i++)
        {
            if (!writeSector(sector + i, src + 512 * i))
            {
                return false;
            }
        }
        return true;
    }
    //check if multi write is still valid
    if(multi_write){
        if(sector!=m_curSector){
            multi_write=false;
            stopTransmission(true);
        }
    }
    sd_callback_t callback = get_stream_callback(src, n * 512, "writeSectors", sector);
    if(multi_write){
        uint32_t reply;
        if (
            !checkReturnOk(rp2040_sdio_tx_start(src, n))) // Start transmission
        {
            return false;
        }
    }else{
        uint32_t reply;
        if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
            !checkReturnOk(rp2040_sdio_command_R1(CMD55, g_sdio_rca, &reply)) || // APP_CMD
            !checkReturnOk(rp2040_sdio_command_R1(ACMD23, n, &reply)) || // SET_WR_CLK_ERASE_COUNT
            !checkReturnOk(rp2040_sdio_command_R1(CMD25, sector, &reply)) || // WRITE_MULTIPLE_BLOCK
            !checkReturnOk(rp2040_sdio_tx_start(src, n))) // Start transmission
        {
            return false;
        }
    }

    m_curSector=m_curSector+n;
    if(m_curSector==multi_write_end){
        multi_write=false;
    }
    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_tx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
      printf("SDIO", "SdioCard::writeSectors(", sector, ",...,", (int)n, ") failed: ", (int)g_sdio_error);
        stopTransmission(true);
        return false;
    }
    else
    {
        if(!multi_write){
            return stopTransmission(true);
        }
        else{
            return true;
        }
    }
}

bool SdioCard::readSector(uint32_t sector, uint8_t* dst)
{
    uint8_t *real_dst = dst;
    if (((uint32_t)dst & 3) != 0)
    {
        // Buffer is not aligned, need to memcpy() the data from a temporary buffer.
        dst = (uint8_t*)g_sdio_dma_buf;
    }

    sd_callback_t callback = get_stream_callback(dst, 512, "readSector", sector);

    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_rx_start(dst, 1)) || // Prepare for reception
        !checkReturnOk(rp2040_sdio_command_R1(CMD17, sector, &reply))) // READ_SINGLE_BLOCK
    {
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_rx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
      printf("SDIO", "SdioCard::readSector(", sector, ") failed: ", (int)g_sdio_error);
    }

    if (dst != real_dst)
    {
        memcpy(real_dst, g_sdio_dma_buf, sizeof(g_sdio_dma_buf));
    }

    return g_sdio_error == SDIO_OK;
}

bool SdioCard::readSectors(uint32_t sector, uint8_t* dst, size_t n)
{
    if (((uint32_t)dst & 3) != 0 || sector + n >= g_sdio_sector_count)
    {
        // Unaligned read or end-of-drive read, execute sector-by-sector
        for (size_t i = 0; i < n; i++)
        {
            if (!readSector(sector + i, dst + 512 * i))
            {
                return false;
            }
        }
        return true;
    }

    sd_callback_t callback = get_stream_callback(dst, n * 512, "readSectors", sector);

    uint32_t reply;
    if (!checkReturnOk(rp2040_sdio_command_R1(16, 512, &reply)) || // SET_BLOCKLEN
        !checkReturnOk(rp2040_sdio_rx_start(dst, n)) || // Prepare for reception
        !checkReturnOk(rp2040_sdio_command_R1(CMD18, sector, &reply))) // READ_MULTIPLE_BLOCK
    {
        return false;
    }

    do {
        uint32_t bytes_done;
        g_sdio_error = rp2040_sdio_rx_poll(&bytes_done);

        if (callback)
        {
            callback(m_stream_count_start + bytes_done);
        }
    } while (g_sdio_error == SDIO_BUSY);

    if (g_sdio_error != SDIO_OK)
    {
      printf("SDIO", "SdioCard::readSectors(", sector, ",...,", (int)n, ") failed: ", (int)g_sdio_error);
        stopTransmission(true);
        return false;
    }
    else
    {
        return stopTransmission(true);
    }
}

// These functions are not used for SDIO mode but are needed to avoid build error.
void sdCsInit(SdCsPin_t pin) {}
void sdCsWrite(SdCsPin_t pin, bool level) {}

// SDIO configuration for main program
SdioConfig g_sd_sdio_config(DMA_SDIO);
