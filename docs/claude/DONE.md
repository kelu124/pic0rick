# DONE — pic0rick

Completed actions, most recent first. Items are moved here from `TODO.md` when
finished. Each entry links to its commit log in `docs/claude/commits/`.

---

## 2026-09-13

- **`reboot-dfu` command + hardware test (fw v0.1.2).** Added `reboot-dfu`
  (reset_usb_boot → BOOTSEL). Flashed and verified on a real RP2350A/Pico 2 W:
  `version` + the flash→run→`reboot-dfu`→BOOTSEL loop. New `hardware-testing.md`.
  See `commits/2026-09-13-reboot-dfu.md`. (branch feature/unify-firmware-dsp)
- **Python parses the firmware `version` command (py v0.1.1).**
  `Pic0rick.version()` queries `version` and returns a parsed dict. See
  `commits/2026-09-13-python-version-parse.md`.
- **`version` command build metadata + dual-version release notes (fw v0.1.1).**
  `version` now prints board/chip, git build hash (`-dirty` aware) and the
  release URL; CI release body lists both firmware + python versions. See
  `commits/2026-09-13-version-metadata.md`.
- **Firmware `version` command + versioning.** `firmware/version.yaml` (A.B.C +
  changes) → CMake-generated `version.h` → `version` CLI command. Firmware v0.1.0.
  See `commits/2026-09-13-firmware-version-mux-ci.md`.
- **MUX made optional at compile time (`-DMUX`).** Split the gain DAC out to
  `max/dac.c` (always built); `max/max14866.c` + mux commands gated by `-DMUX`.
  Same commit log.
- **CI build + release workflow.** `.github/workflows/firmware.yml` builds all 4
  variants (rp2040/rp2350 × mux/nomux), uploads artifacts, and releases on push
  to main. `build.sh` now builds all 4 into `dist/`. Same commit log.
- **Python library versioning.** `python/pic0rick/__init__.py` +
  `version.yaml` expose `pic0rick.__version__`, v0.1.0. See
  `commits/2026-09-13-python-versioning.md`.

## 2026-09-12

- **Firmware↔Python command parity.** Audited the 6 firmware serial commands
  against `python/pic0rick`; added the 3 missing MUX methods (`write_mux`,
  `set_mux`, `clear_mux`) to `Pic0rick`. See
  `commits/2026-09-12-mux-command-parity.md`.
- **Set up task tracking.** Created `docs/claude/TODO.md`, this file, and the
  `docs/claude/commits/` log folder; transformed `QUESTIONS.md` into TODO actions
  and removed it. See `commits/2026-09-12-setup-todo-tracking.md`.
