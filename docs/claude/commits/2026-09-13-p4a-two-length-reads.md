# 2026-09-13 — P4a: two-length acquisition (read_raw 8000 / read_fft 4096)

Firmware **v0.1.7**. Branch `feature/unify-firmware-dsp`.

## What
Made the capture length **per-mode** instead of a single fixed 4096:
- `read_raw` → 8000-sample raw ADC (no FFT).
- `read_fft` → 4096-sample Hilbert envelope (CMSIS `arm_rfft_fast` max).
- Threaded a per-job `sample_count` through `u4rk.h` (job struct + new
  `U4RK_RAW_SAMPLE_COUNT=8000`, `U4RK_MAX_SAMPLE_COUNT`), `acquisition.c`
  (`u4rk_capture_start(dest, count)`), `dsp.c` (`u4rk_dsp_extract(.., count)`),
  and `pipeline.c` (raw buffers sized to max; header `sample_count`, payload
  size and latest-raw copy all length-aware; `copy_latest_raw` now returns the
  count). `main_dsp.c` picks 8000 for raw / 4096 for envelope|alaw, keeps
  selftest at 4096, and `legacy_read` dumps the stored count.

## Why (design, from the user)
The 4096 cap is only the CMSIS `arm_rfft_fast_f32` limit (max power-of-two),
not a memory/ADC limit — so raw can stay 8000 while the FFT path uses 4096, on
one firmware. Max payload is unchanged (4096 floats = 16384 B ≥ 8000 u16 =
16000 B); the frame header's `sample_count` makes frames self-describing.

## Verified on hardware (RP2350A / Pico 2 W, DSP+MUX)
Flashed via reboot-dfu:
- `read_raw` → `OK … type=raw samples=8000`; frame 16064 B = 64 hdr + 16000
  (8000×u16); header sample_count=8000.
- `read_fft` → `OK … type=envelope samples=4096`; frame 16448 B = 64 + 16384
  (4096×float); header sample_count=4096.
All 4 default (non-DSP) variants build unchanged.

## Next (P4b)
Unify the stdio/RP2040 build onto the shared acquisition+pulser+spi1 DAC (raw
8000). Requires moving the USB-free modules out of firmware/dsp/ (which holds
tusb_config.h) into a shared dir so the stdio build can include them without
clashing with SDK stdio's TinyUSB.
