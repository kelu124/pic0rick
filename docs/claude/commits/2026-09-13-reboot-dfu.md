# 2026-09-13 — reboot-dfu command + first hardware test

Firmware **v0.1.2**. Branch `feature/unify-firmware-dsp`.

## What
- Added `reboot-dfu` CLI command to `firmware/main.c`: prints a notice, flushes,
  then `reset_usb_boot(0, 0)` (`pico/bootrom.h`) to reboot into the USB
  bootloader (BOOTSEL / UF2 mass storage, aka "DFU").
- Linked `pico_bootrom` in `firmware/CMakeLists.txt`.

## Why
A physical RP2350A/Pico 2 W dev board is connected for testing. `reboot-dfu`
lets us re-enter the bootloader from the CLI so successive UF2 builds can be
flashed without pressing BOOTSEL — closing the flash/test iterate loop.

## Verified on hardware (RP2350A / Pico 2 W, pico2 build)
- Built all 4 variants; flashed `pic0rick-v0.1.2-rp2350-mux.uf2` by copying to
  the BOOTSEL mass storage at `/media/kelu/RP2350`.
- Device booted to CDC `/dev/ttyACM0`; `version` reported
  `board: pico2 (RP2350)`, `mux: enabled`, `release: .../fw-v0.1.2`.
- `reboot-dfu` dropped the CDC and re-entered BOOTSEL (`2e8a:000f`) in ~2 s;
  reflashing from the re-mounted MSD restored the running firmware.

## Notes / new docs
- Full flashing + serial + DFU-loop recipe recorded in
  `docs/claude/hardware-testing.md` (and memory).
- Build hash showed `-dirty` because the uf2 was built before committing.
- We build/flash `pico2` (not `pico2_w`) since wifi is unused; switch to
  `pico2_w` if CYW43 is ever needed.
