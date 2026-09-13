# 2026-09-13 — P4b: unify hardware drivers (stdio + DSP share firmware/hw)

Firmware **v0.1.8**. Branch `feature/unify-firmware-dsp`.

## What
- Moved the USB-free drivers out of `firmware/dsp/` into a shared
  **`firmware/hw/`**: `acquisition.[ch]`/`acquisition.pio`, `pulser.pio`,
  `dac.[ch]` (spi1), `u4rk.h`. (They can't live next to `tusb_config.h`, which
  the stdio build's SDK TinyUSB would wrongly pick up.)
- Rewrote the stdio `main.c` to use the shared drivers: `write dac` →
  `u4rk_dac_write`; `start acq [pon] [poff] [damp]` → configure+arm the shared
  pulser and capture 8000 samples (polled, single-threaded); `read` → dump 8000.
- **Retired** `firmware/adc/` (PIO pulser/ADC) and `firmware/max/dac.c` (PIO DAC).
- CMake: both branches compile `hw/acquisition.c` + `hw/dac.c`; `p0rk_dsp` sources
  them from `hw/`; stdio build links `hardware_spi`.

## Why
User: "backport the onboard pulser to stdio too" for one shared implementation.
Both builds now drive the same acquisition + pulser + spi1 DAC on the same pins
(11/12/16/17 pulser, 13/14/15 DAC).

## Verified on hardware (RP2350A / Pico 2 W, stdio rp2350-mux build)
Flashed via reboot-dfu:
- `version` → v0.1.8.
- `write dac 500` → `dac=500`.
- `start acq 200 200 2000` → "Acquisition of 8000 samples started / ended"
  (DMA completed, no timeout).
- `read` → ~8000 hex ADC samples, End-of-ACQ marker, values sane (mean ~410).
All 4 default variants + the DSP build compile.

## Notes / caveats
- The stdio `start acq` now drives the u4rk pulse sequence (positive-first,
  OE-gated) rather than the old PIO pulse1/pulse2 waveform. Mapping:
  positive_ns=pon, negative_ns=poff, damp_ns=damp. Validated at command/data
  level only (no scope, per the connected setup).
- `firmware/adc/` is now empty (untracked dir remains; git tracks no files there).
