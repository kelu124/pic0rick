
20251229

Separately, while digging into these PIO state machines, I think I found a fix for a common different bug I was seeing, I would randomly see DMA timeout when pulsing and plotting. I believe the fix for this would be to add a 1us delay in the adc trigger code:

"pio_sm_put_blocking(pio_adc, sm, SAMPLE_COUNT);
sleep_us(1);
pio_sm_put_blocking(pio_adc, sm3, numbers[2]);
pio_sm_put_blocking(pio_adc, sm2, numbers[0]);
pio_sm_put_blocking(pio_adc, sm2, numbers[1]);"

Which would ensure the ADC would reach its wait irq 0 state. As it is now, there is a very small window (2 cycle margin) for the adc to catch the irq from the pulse1.


20250501
* mux_ok_butclrset.uf2 works .. not CLEAR / SET though


20250405
* Change MUX code (faster)
* Remove 5V generator from MUX board
* WebAPI for the pico2 ?
* Add version / hash of binary
* Read ID pico (and type of pico)



# Scheduled changes

## DONE

* FW: Tie the pulses to the PIO code so that pulses strictly cohappen with the acquisition start (done)

## TODO

* HW: Slight tweaks on the main board to allow more space for the PMODs (Oct 21, 2024)
