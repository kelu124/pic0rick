# 2026-09-13 — P1: -DDSP option scaffolding + CMSIS-DSP

Firmware **v0.1.4**. Branch `feature/unify-firmware-dsp`.

## What
In `firmware/CMakeLists.txt`:
- `option(DSP "..." OFF)`.
- Guard: `if(DSP AND NOT PICO_BOARD STREQUAL "pico2") -> FATAL_ERROR` (the DSP
  pipeline needs the RP2350 FPU).
- Inside `if(DSP)`: CMSIS-DSP v1.17.0 `FetchContent` + the `cmsisdsp_p0rk` static
  lib (the float32 transform subset only), copied from the onboard_dsp CMake.

No firmware sources added; the executable does not yet use DSP. Default builds
are byte-unchanged apart from the version string.

## Why
First build-system step of the unification: make the DSP toolchain available and
board-gated before landing the modules (P2).

## Verification
- Default (DSP off): all 4 variants build at v0.1.4; feature reports "disabled".
- Guard: `-DDSP=ON -DPICO_BOARD=pico` → FATAL_ERROR as intended.
- `-DDSP=ON -DPICO_BOARD=pico2`: configure exit 0, CMSIS-DSP fetched, and
  `cmake --build --target cmsisdsp_p0rk` produced `libcmsisdsp_p0rk.a` (736 KB).
  (Done in a scratch build dir, not the repo.)

## Finding (for P6/CI)
The CMSIS-DSP shallow clone takes >2 min locally; CI will pay this on DSP builds.
Consider caching `_deps` or a submodule/mirror when wiring DSP into CI.
