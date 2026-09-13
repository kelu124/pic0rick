# 2026-09-13 — Fix MUX/pulser PIO1 SM conflict (the real no-echo cause)

Firmware **v0.1.11**. Branch `feature/unify-firmware-dsp`.

## The real root cause
Echoes worked on stdio v0.1.0 but not after unification. It was **not** OE, and
not (only) the damp ordering (v0.1.10). It was a **PIO1 state-machine collision**:

- `u4rk_acquisition_init()` (shared pulser) claims PIO1 `sm0` (pulser drive) and
  `sm1` (gate) via `pio_claim_unused_sm`.
- `max14866_init()` then **hardcoded `sm4 = 0`** and reconfigured PIO1 `sm0` for
  the mux SPI — clobbering the pulser's drive SM. So on **MUX builds** the P+/P-
  drive never ran: only digital crosstalk reached the ADC, no real HV pulse, no
  echo.

In v0.1.0/v0.1.1 there was no clash (old pulser on PIO0, mux on PIO1). The
unification moved the pulser to PIO1, creating the conflict.

## Fix
`max14866_init()` now `pio_claim_unused_sm(pio1, true)` instead of hardcoding
`sm0` (one line). Applies to both stdio and DSP mux builds.

## Verified on hardware (RP2350A, user's piezo)
Using the baseline method `pulse_adc_trigger(200,200,2000)` + `read`, measuring
`var[2000:3000]` (echo region) vs `var[6000:7000]` (quiet):

| build | var[2000:3000] | echoes |
|---|---|---|
| v0.1.10 mux (buggy) | ~0.4 | none |
| v0.1.10 **nomux** (isolation test) | 1442 | yes |
| **v0.1.11 mux (fixed)** | 1454 → **51,563** at gain 300 | yes |
| v0.1.1 baseline (reference) | 60,111 | yes |

Echo structure (main bang + 2–2.5k, 4.5–5k, 7k) matches the v0.1.1 baseline; at
gain 600+ the echo saturates. Confirmed the amplitude scales with the DAC.

## Notes
- The v0.1.10 damp-reorder (bipolar pulse then damp) is also correct and kept.
- This was found by predicting nomux would work (no mux ⇒ no SM clash) and
  testing it — it did, isolating the conflict.
