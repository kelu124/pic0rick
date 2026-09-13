# DONE — pic0rick

Completed actions, most recent first. Items are moved here from `TODO.md` when
finished. Each entry links to its commit log in `docs/claude/commits/`.

---

## 2026-09-13

- **P3 — runnable DSP build (fw v0.1.6).** `main_dsp.c` raw-TinyUSB command loop
  (DSP builds; stdio OFF) with version/reboot-dfu/mux; non-DSP stdio unchanged.
  Verified on RP2350A hardware (version/help/status + `acq raw` 8256-B frame);
  DSP+noMUX and default variants build. See
  `commits/2026-09-13-p3-dsp-command-loop.md`.
- **P2 — land onboard-DSP modules (fw v0.1.5).** Copied sources to
  `firmware/dsp/`; built the USB-free set as `p0rk_dsp` static lib under
  `if(DSP)`. USB transport deferred to P3. Default builds unchanged;
  `libp0rk_dsp.a` verified. See `commits/2026-09-13-p2-land-dsp-modules.md`.
- **P1 — `-DDSP` scaffolding + CMSIS-DSP (fw v0.1.4).** pico2-guarded DSP option
  and `cmsisdsp_p0rk` lib; no sources yet, default builds unchanged. Verified
  guard + CMSIS-DSP fetch/build. See `commits/2026-09-13-p1-dsp-scaffolding.md`.
- **P0 — SDK bump to 2.3.0 (fw v0.1.3).** CMakeLists + CI workflow ref; all 4
  variants build; rp2350 flashed + confirmed on hardware. First phase of the
  onboard-DSP unification. See `commits/2026-09-13-p0-sdk-2.3.0.md`.
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
