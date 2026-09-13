# 2026-09-13 — P3: runnable DSP build (raw-TinyUSB command loop)

Firmware **v0.1.6**. Branch `feature/unify-firmware-dsp`. First *runnable* DSP
firmware.

## What
- Added `firmware/main_dsp.c` (adapted from onboard_dsp `main.c`): the raw-TinyUSB
  line command loop that also emits binary frames. Extras vs onboard:
  - `version` command → the standard version block (FW_VERSION/board/chip/mux/
    build/release), so the existing `Pic0rick.version()` parser works.
  - `reboot-dfu` command → `reset_usb_boot(0,0)`.
  - Under `-DMUX`: `write mux <hex>`, `set mux`, `clear mux` (+ `max14866_init()`
    at boot); listed in `help`.
- CMake: split the executable into `if(DSP)` (raw TinyUSB) / `else` (stdio REPL):
  - DSP: `pico_enable_stdio_usb(0)`, sources `main_dsp.c` +
    `dsp/usb_transport.c` + `dsp/usb_descriptors.c`, include `firmware/dsp`,
    defines `DSP=1` + `U4RK_CMSIS_DSP_VERSION="1.17.0"`, links `tinyusb_device`
    + `p0rk_dsp`; MUX adds `max/max14866.c` + `max14866.pio`. The PIO DAC
    (`max/dac.c`) and mainline PIO ADC (`adc/adc.c`) are NOT built (spi1 DAC +
    acquisition come from `p0rk_dsp`).
  - non-DSP: unchanged stdio path (`main.c`, `adc/adc.c`, `max/dac.c`, …).

## Why
Design A (locked earlier): the DSP build owns the CDC via raw TinyUSB so text +
binary frames coexist; RP2040/non-DSP keeps SDK stdio.

## Verification (RP2350A / Pico 2 W hardware)
Flashed the DSP+MUX build via the reboot-dfu loop:
- `version` → v0.1.6, `board: pico2 (RP2350)`, `mux: enabled`.
- `help` → full command list incl. version/reboot-dfu/mux.
- `status` → `package=RP2350A samples=4096 dsp_backend=f32-rfft-hilbert
  cmsis=1.17.0 …`.
- `acq raw` → `OK capture started type=raw` + 8256 B (64 hdr + 8192 payload).
Also: DSP+noMUX builds; all 4 default (non-DSP) variants build unchanged.

## Fix
Added the missing `U4RK_CMSIS_DSP_VERSION` compile define (onboard set it in
CMake; `send_status()` uses it).
