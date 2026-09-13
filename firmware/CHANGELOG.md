# Changelog — pic0rick firmware

All notable changes to the firmware (`firmware/`). Versions follow the `A.B.C`
scheme in `version.yaml` (major.minor.patch); the patch is bumped on every
change to `firmware/` source. Newest first.

This file and `version.yaml` are the version record — editing them does not
itself trigger a version bump. Every firmware version bump must add an entry
here (see `docs/claude/README.md`).

## 0.1.9 — 2026-09-13
- `build.sh` builds all 6 variants (rp2040 mux/nomux, rp2350 mux/nomux, rp2350
  mux/nomux +DSP) into `dist/`, sharing one CMSIS-DSP clone (`firmware/.deps`).
- CI caches `firmware/.deps` and builds/releases all 6 uf2.

## 0.1.8 — 2026-09-13
- Unified hardware drivers: the stdio and DSP builds now share one acquisition +
  pulser + spi1 DAC in `firmware/hw/`.
- The stdio build's `start acq` / `write dac` / `read` use the shared drivers
  (raw 8000).
- Retired the old PIO pulser (`adc/`) and PIO DAC (`max/dac.c`).

## 0.1.7 — 2026-09-13
- Two-length acquisition: `read_raw` = 8000-sample raw ADC (no FFT),
  `read_fft` = 4096-sample Hilbert envelope. Sample count is per-capture and
  self-described by the frame header. (DSP build.)

## 0.1.6 — 2026-09-13
- First runnable DSP build (`-DDSP`, RP2350): raw-TinyUSB command loop
  (`main_dsp.c`) with the onboard-DSP commands plus `version`, `reboot-dfu`, and
  (with `-DMUX`) `write/set/clear mux`. Non-DSP stdio builds unchanged.

## 0.1.5 — 2026-09-13
- Landed the onboard-DSP modules under `firmware/dsp/` as the `p0rk_dsp` static
  library (compiled only when `-DDSP=ON`). Not yet wired into the command loop.

## 0.1.4 — 2026-09-13
- Added the `-DDSP` build-option scaffolding (RP2350/`pico2`-guarded) that
  fetches CMSIS-DSP; firmware sources wired in later. Default builds unchanged.

## 0.1.3 — 2026-09-13
- Build against Pico SDK 2.3.0 (was 2.2.0), preparing for the onboard-DSP
  unification (needs 2.3.0 + CMSIS-DSP).

## 0.1.2 — 2026-09-13
- Added the `reboot-dfu` CLI command: reboots into the USB bootloader (BOOTSEL)
  so a new UF2 can be flashed without pressing BOOTSEL.

## 0.1.1 — 2026-09-13
- `version` command now also reports board/chip, MUX state, the git build hash
  (with `-dirty`) and the GitHub release URL.
- CI release notes list both the firmware and python library versions.

## 0.1.0 — 2026-09-13
- First versioned firmware: added the `version` CLI command (reads
  `version.yaml`); the MAX14866 MUX board is now optional at compile time via
  `-DMUX`.
