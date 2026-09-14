# 2026-09-13 — P2: land onboard-DSP modules as p0rk_dsp static lib

Firmware **v0.1.5**. Branch `feature/unify-firmware-dsp`.

## What
- Copied all onboard sources into **`firmware/dsp/`** (kept `u4rk_`/`U4RK_`
  names): `dsp.[ch]`, `pipeline.[ch]`, `protocol.[ch]`, `acquisition.[ch]` +
  `acquisition.pio`, `pulser.pio`, spi1 `dac.[ch]`, `u4rk.h`, plus
  `usb_transport.[ch]`, `usb_descriptors.c`, `tusb_config.h`.
- Added a **`p0rk_dsp` static library** (CMake, inside `if(DSP)`) compiling the
  USB-free compute/hardware set: `dsp/pipeline/protocol/acquisition/dac`. Wires
  the two PIO headers, CMSIS-DSP link (`cmsisdsp_p0rk`), and the pico libs
  (`pico_multicore`/`pico_util`/`hardware_spi`/…).

## Why (scope split)
`usb_transport.c`/`usb_descriptors.c` provide TinyUSB device callbacks that
conflict with the SDK `stdio_usb` REPL currently in `main.c`. They can't be
compiled into a build alongside stdio_usb, so they're deferred to **P3**, where
the DSP build switches to the raw-TinyUSB command loop. P2 keeps everything
buildable and verifies the compute modules compile in-tree.

## Verification
- Default (DSP off): all 4 variants build at v0.1.5 (unchanged).
- `-DDSP=ON -DPICO_BOARD=pico2`: `cmake --build --target p0rk_dsp` →
  `libp0rk_dsp.a` (1.87 MB), no errors/warnings. (Scratch build dir.)

## Notes
- The originals under `experiments/onboard_dsp/firmware/pic0rick/` are unchanged;
  they're removed in P7.
- `p0rk_dsp` is not yet linked into the `adc-pulse` executable (P3).
