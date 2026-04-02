//!
//! \file       adc.c
//! \author     Abdelrahman Ali
//! \date       2024-01-20
//!
//! \brief      adc dac pio.
//!

//---------------------------------------------------------------------------
// INCLUDES
//---------------------------------------------------------------------------

#include "adc.h"

#include "adc.pio.h"

//---------------------------------------------------------------------------
// GLOBAL VARIABLES
//---------------------------------------------------------------------------

PIO pio_adc;
uint sm;
uint offset;
uint sm2; // pulse GPIO11/12
uint offset2;
uint sm3; // pulse GPIO16/17
uint offset3;
uint dma_chan;
dma_channel_config dma_chan_cfg;
uint16_t buffer[SAMPLE_COUNT];

//---------------------------------------------------------------------------
// ADC INIT FUNCTION
//---------------------------------------------------------------------------

void pio_adc_init()
{
    pio_adc = pio0;
    sm = pio_claim_unused_sm(pio_adc, true);
    offset = pio_add_program(pio_adc, &adc_program);
    adc_program_init(pio_adc, sm, offset, PIN_BASE, ADC_CLK);
    sm2 = pio_claim_unused_sm(pio_adc, true);
    offset2 = pio_add_program(pio_adc, &pulse1_program);
    pulse1_program_init(pio_adc, sm2, offset2, GPIO11, PULSE_CLK);
    sm3 = pio_claim_unused_sm(pio_adc, true);
    offset3 = pio_add_program(pio_adc, &pulse2_program);
    pulse2_program_init(pio_adc, sm3, offset3, GPIO16, PULSE_CLK);
    dma_chan = dma_claim_unused_channel(true);
    dma_chan_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_chan_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_chan_cfg, false);
    channel_config_set_write_increment(&dma_chan_cfg, true);
    channel_config_set_dreq(&dma_chan_cfg, pio_get_dreq(pio_adc, sm, false));
    // pio_enable_sm_mask_in_sync(pio_adc, ((1u << sm) | (1u << sm2) | (1u << sm3)));
}

//---------------------------------------------------------------------------
// ADC DMA FUNCTION
//---------------------------------------------------------------------------
void pio_adc_dma()
{
    dma_channel_configure(dma_chan, &dma_chan_cfg, buffer, &pio_adc->rxf[sm], SAMPLE_COUNT, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
}

//---------------------------------------------------------------------------
// ADC CLEAN FIFO FUNCTION
//---------------------------------------------------------------------------

void pio_adc_clear_fifo()
{
    pio_sm_clear_fifos(pio_adc, sm);
    pio_sm_clear_fifos(pio_adc, sm2);
    pio_sm_clear_fifos(pio_adc, sm3);
}


//---------------------------------------------------------------------------
// PULSE ADC DMA TIMEOUT
//---------------------------------------------------------------------------
bool dma_wait_timeout(uint chan, uint32_t ms) {
    absolute_time_t deadline = make_timeout_time_ms(ms);
    while (dma_channel_is_busy(chan)) {
        if (absolute_time_diff_us(get_absolute_time(), deadline) >= 0) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------------------------
// PULSE ADC RESTART SMs
//---------------------------------------------------------------------------
void reset_all_sms() {
    uint sms[] = { sm, sm2, sm3 };
    for (int i = 0; i < 3; i++) {
        pio_sm_set_enabled(pio_adc, sms[i], false);
        pio_sm_restart(pio_adc, sms[i]);
        pio_sm_set_enabled(pio_adc, sms[i], true);
    }
}

//---------------------------------------------------------------------------
// PULSE ADC TRIGGER FUNCTION
//---------------------------------------------------------------------------
void pulse_adc_trigger(const char *data)
{
    char *token;
    uint32_t numbers[3];
    int i = 0;

    char data_copy[strlen(data) + 1];
    strcpy(data_copy, data);

    token = strtok(data_copy, " ");
    while (i < 3)
    {
        if (token != NULL)
            numbers[i] = (atoi(token) / 8); // divide by 8 as one cycle is 8 nanoseconds
        else
            numbers[i] = 125; // default value
        i++;
        token = strtok(NULL, " ");
    }
    pio_interrupt_clear(pio_adc, (1u<<0)|(1u<<1)|(1u<<2));
    pio_adc_clear_fifo();
    reset_all_sms();
    memset(buffer, 0x00, SAMPLE_COUNT);
    printf("Acquisition of %d samples started\n", SAMPLE_COUNT);
    dma_channel_configure(dma_chan, &dma_chan_cfg, buffer, &pio_adc->rxf[sm], SAMPLE_COUNT, true);
    pio_sm_put_blocking(pio_adc, sm, SAMPLE_COUNT);
    pio_sm_put_blocking(pio_adc, sm3, numbers[2]);
    pio_sm_put_blocking(pio_adc, sm2, numbers[0]);
    pio_sm_put_blocking(pio_adc, sm2, numbers[1]);
    if (!dma_wait_timeout(dma_chan, DMA_TIMEOUT_MS)) {
        printf("ADC timeout occured\n");
    }
    // dma_channel_wait_for_finish_blocking(dma_chan);
    printf("Acquisition ended\n");
}

//---------------------------------------------------------------------------
// ADC MAIN FUNCTION
//---------------------------------------------------------------------------

void adc(const char *data)
{
    printf("----------Start of ACQ----------\n");

    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i)
    {
        printf("%X,", ((buffer[i] >> 1) & 0x3FF));
    }

    printf("\n-----------End of ACQ-----------\n");
}

//---------------------------------------------------------------------------
// END OF FILE
//---------------------------------------------------------------------------
