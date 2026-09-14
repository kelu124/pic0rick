# DONE — pic0rick

Completed actions, most recent first. Items are moved here from `TODO.md` when
finished. Each entry links to its commit log in `docs/claude/commits/`.

---

## 2026-09-14

- **example_simple + ndt_acquisition on the DSP CLI (py v0.1.8).** New
  `Pic0rick.set_gain/configure_pulse/arm_pulser/disarm_pulser`; `example_simple.ipynb`
  and `ndt_acquisition.from_probe()` now use `read_raw` (binary) instead of the
  stdio hex path. Hardware-verified (from_probe echoes present). See
  `commits/2026-09-14-example-simple-ndt-dsp-cli.md`.

- **example_dsp.ipynb: A-law + self-test + file saving, armed from start (py v0.1.7).**
  A-law decode-vs-envelope plot, DSP self-test summary, pulser-order check,
  per-capture saving to `captures/<name>/`. Hardware-verified. See
  `commits/2026-09-14-dsp-notebook-alaw-selftest-save.md`.

- **DSP notebook shows Pico text replies + us times (py v0.1.6).** Added
  `dsp.describe_status()`, `status()['raw']`, `capture().last_reply`; the notebook
  now prints version/status/OK replies and per-stage DSP µs. Hardware-verified.
  See `commits/2026-09-14-dsp-notebook-show-replies.md`.
- **docs/fw_history convention.** README for the maintainer-curated firmware
  baseline archive; rule that only the maintainer adds `.uf2` there (Claude never
  does). See `commits/2026-09-14-fw-history-convention.md`.

## 2026-09-13

- **MUX/pulser PIO1 SM conflict fix — echoes restored (fw v0.1.11).**
  `max14866_init` hardcoded pio1 sm0, clobbering the pulser drive SM; now claims
  an unused SM. Verified on the user's piezo: mux build echo `var[2000:3000]`
  0.4 → ~52k (matches v0.1.1 baseline). Root cause of the "no echo" report. See
  `commits/2026-09-13-mux-pulser-sm-conflict-fix.md`.
- **Pulser echo regression fix (fw v0.1.10).** `queue_pulse` now fires the
  bipolar pulse back-to-back then damps (was: damp between polarities, a P4b
  regression that killed echoes). Not OE (that's correct/active-high). Transmit
  confirmed strong on hardware; echo-vs-target left to the user's setup. See
  `commits/2026-09-13-pulser-echo-fix.md`.
- **DSP example notebook + DAC fix (py v0.1.5).** `python/example_dsp.ipynb`
  (step-by-step, executed via nbconvert on hardware); fixed the DSP DAC command
  (`dac write`). See `commits/2026-09-13-dsp-example-notebook.md`.
- **DSP test guide + example + firmware CHANGELOG (py v0.1.4).**
  `docs/dsp_test_guide.md` + `python/example_dsp.py` (new API, hardware-tested);
  `firmware/CHANGELOG.md` created and made a rule (update on every fw bump). See
  `commits/2026-09-13-dsp-guide-example-changelog.md`.
- **P8 — docs consolidation; unification complete.** README building/DSP
  sections + command table; plan marked done (P0–P8). Firmware v0.1.9, python
  v0.1.3, on `feature/unify-firmware-dsp` (not merged). See
  `commits/2026-09-13-p8-docs.md`.
- **P7 — retire experiments/onboard_dsp (py v0.1.3).** Removed `experiments/`;
  kept the format spec as `docs/dsp_output_formats.md`; fixed all references.
  See `commits/2026-09-13-p7-retire-onboard-dsp.md`.
- **P6 — 6-variant build matrix + CI (fw v0.1.9).** build.sh builds all 6
  (adds rp2350 mux/nomux +DSP) into dist/ with a shared CMSIS-DSP cache; CI
  caches it and releases all 6. See `commits/2026-09-13-p6-build-matrix-ci.md`.
- **P5 — DSP host protocol in python (py v0.1.2).** `pic0rick/dsp.py`
  (length-agnostic frame reader + CRC + status parser) and
  `Pic0rick.status()/capture()/read_fft()/read_raw()`; dropped the hard version
  gate. Verified on hardware. See `commits/2026-09-13-p5-host-dsp-protocol.md`.
- **P4b — unified HW drivers (fw v0.1.8).** Moved acquisition+pulser+spi1 DAC to
  shared `firmware/hw/`; stdio build now uses them (raw 8000); retired `adc/` +
  `max/dac.c`. Verified on RP2350 hardware. See
  `commits/2026-09-13-p4b-unify-hw-drivers.md`.
- **P4a — two-length acquisition (fw v0.1.7).** `read_raw` (8000, raw) /
  `read_fft` (4096, envelope); per-job sample_count threaded through
  acquisition/pipeline/dsp. Verified frame sizes on RP2350A hardware. See
  `commits/2026-09-13-p4a-two-length-reads.md`.
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
