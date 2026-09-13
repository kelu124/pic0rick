# Plan: unify onboard_dsp into the mainline firmware (branch `feature/unify-firmware-dsp`)

**Goal:** one firmware source under `firmware/`. On **RP2350** a compile flag
pulls in the `experiments/onboard_dsp/` DSP features (Hilbert envelope + A-law,
dual-core, binary framed protocol), exposed as CLI commands. RP2040 keeps the
simple text build. When done, **retire `experiments/onboard_dsp/`**.

Decisions taken by the user (2026-09-13): fold DSP modules in and gate with a
flag (RP2350 only); DSP steps become new CLI commands; **SDK bump to 2.3.0 OK**;
retire onboard_dsp after merge.

---

## The pivotal constraint — USB model (confirm before Phase 3)

onboard_dsp bypasses SDK stdio (`pico_enable_stdio_usb 0`) and drives TinyUSB
directly (`usb_transport.c`/`usb_descriptors.c`/`tusb_config.h`) so text + binary
frames share one CDC. The mainline REPL uses SDK `stdio_usb`. **Only one USB
stack can own the CDC.**

- **Design A (recommended):** the DSP build uses onboard_dsp's raw-TinyUSB
  transport for *all* I/O; its `main.c` is already a line REPL that also emits
  frames. RP2040 (no DSP) keeps SDK stdio. Result: RP2350+DSP command set ≈
  onboard_dsp set + `version` (+ optional `mux`). Lowest risk — reuses the
  already-working transport.
- **Design B:** keep SDK stdio everywhere and push binary frames through stdout
  with `putchar_raw`. One command loop for both boards, but re-treads exactly
  what onboard_dsp deliberately avoided (stdio corrupting frames). Higher risk.

**Plan below assumes Design A.**

---

## Command reconciliation (overlaps with different semantics)

| Command | Mainline (RP2040/stdio) | onboard_dsp (RP2350/DSP) |
|---|---|---|
| `start acq` | `start acq <pon> <poff> <damp>` fires pulser+capture | `start acq` = legacy capture (no args) |
| `read` | dump 8000 hex | dump 4096 hex |
| DAC | `write dac <n>` (PIO) | `dac write <n>` (spi1) |
| MUX | `write/set/clear mux` (opt) | none |
| version | `version` | (uses `status` + version handshake) |

Approach: on the DSP build, adopt the onboard_dsp command names/semantics
(they're the richer set) and **add** `version`; keep `write/set/clear mux` when
`-DMUX`. Accept that `start acq`/`read` mean the 4096-sample DSP-build behaviour
on RP2350 vs the 8000-sample behaviour on RP2040 (documented, board-dependent).
Sample count differs by build (8000 vs 4096) — keep per-build constants.

## Hardware reconciliation
- **DAC:** unify on the onboard_dsp **spi1** driver (`u4rk_dac`, pins 13/14/15).
  Optionally switch the RP2040 build to it too (same pins/chip) to drop the PIO
  DAC — *needs a quick confirm it works on RP2040's spi1*. Default: DSP build
  uses spi1; RP2040 keeps PIO DAC unless we choose to unify.
- **Pulser:** onboard_dsp's 4-pin pulser (P+11/P-12/PDAMP16/OE17, `pulser.pio`)
  is a superset of the mainline 2-pin (GPIO11+16). **Needs hardware confirmation**
  of which pins the boards actually wire before merging the pulser. Until then,
  keep each build's own pulser.
- **MUX:** stays orthogonal (`-DMUX`); onboard_dsp leaves 18-21,28 free, so it
  can coexist on the DSP build.

---

## Phases (incremental; each ends buildable + committed with a commit log)

**P0 — SDK 2.3.0. ✅ DONE (fw v0.1.3, 2026-09-13).** Bumped
`firmware/CMakeLists.txt` sdk/picotool 2.2.0→2.3.0 (toolchain kept 14_2_Rel1 —
the installed one; onboard_dsp's 15_2 not required) and CI workflow pico-sdk ref
→2.3.0. All 4 variants build; rp2350 flashed + confirmed running on hardware.

**P1 — DSP option scaffolding. ✅ DONE (fw v0.1.4, 2026-09-13).** Added
`option(DSP ... OFF)`, the `pico2` guard (FATAL_ERROR on rp2040), and the
CMSIS-DSP `FetchContent` + `cmsisdsp_p0rk` static lib inside `if(DSP)`. No
firmware sources yet. Verified: default builds unchanged; guard rejects rp2040;
`-DDSP=ON -DPICO_BOARD=pico2` configures (fetches CMSIS-DSP v1.17.0) and builds
`libcmsisdsp_p0rk.a`. Note: the CMSIS-DSP shallow clone takes >2 min — CI time
cost to watch in P6.

**P2 — Land the modules. ✅ DONE (fw v0.1.5, 2026-09-13).** Copied all onboard
sources into `firmware/dsp/` (kept `u4rk_`/`U4RK_` names). Built the USB-free
compute/hardware set (`dsp`, `pipeline`, `protocol`, `acquisition`, spi1 `dac`)
as a **`p0rk_dsp` static library** under `if(DSP)` (with `acquisition.pio` +
`pulser.pio` headers, CMSIS-DSP link). `usb_transport.c`/`usb_descriptors.c`/
`tusb_config.h` are copied in-tree but **not compiled yet** — they belong with
the P3 raw-TinyUSB swap (they conflict with the stdio_usb REPL). Verified:
default builds unchanged; `libp0rk_dsp.a` builds under `-DDSP=ON -DPICO_BOARD=pico2`.

**P3 — Command loop. ✅ DONE (fw v0.1.6, 2026-09-13).** Added `main_dsp.c` (from
onboard_dsp `main.c`) selected in CMake under `if(DSP)`: SDK stdio OFF, links
`p0rk_dsp` + `usb_transport.c`/`usb_descriptors.c` + `tinyusb_device`, adds the
`version` + `reboot-dfu` commands and, under `-DMUX`, `write/set/clear mux`.
Non-DSP builds keep `main.c` (stdio) unchanged. Also fixed a missing
`U4RK_CMSIS_DSP_VERSION` compile define on the exe. **Verified on hardware
(RP2350A/Pico 2 W):** flashed the DSP+MUX build via reboot-dfu; `version`/`help`/
`status` return correctly over the raw-TinyUSB CDC and `acq raw` returns the
64-byte header + 8192-byte payload (8256 B). DSP+noMUX and all 4 default variants
also build.

**P4 — Reconcile hardware. ✅ DONE (fw v0.1.7 P4a + v0.1.8 P4b, 2026-09-13).**
Decisions (2026-09-13):
- **DAC:** unify both builds on the spi1 `u4rk_dac` (bit-identical to the PIO DAC:
  `0x3000|(value<<2)`, same pins 13/14/15). PIO DAC (`max/dac.c`) retired.
- **Pulser pins already match** (11/12/16/17) across both firmwares.
- **Two-length acquisition (user design):** sample count is **mode-dependent**,
  not build-dependent, because the 4096 cap is only the CMSIS `arm_rfft_fast_f32`
  limit (max power-of-two = 4096), not memory/ADC.
  - `read_raw` → **8000** samples, raw ADC, no FFT (both builds).
  - `read_fft` → **4096** samples, envelope/FFT (DSP build only).
  Frame header already carries `sample_count`; max payload unchanged (4096 floats
  = 16384 B ≥ 8000×u16 = 16000 B). Threaded a per-job `sample_count` through
  acquisition/pipeline/dsp; raw buffers sized to 8000.
- **stdio/RP2040 build:** uses the shared acquisition+pulser+dac in raw-8000 mode
  (one implementation).

**P5 — Host + version. ✅ DONE (python v0.1.2, 2026-09-13).** Added
`python/pic0rick/dsp.py` (binary frame reader + CRC, A-law decode, `parse_status`),
made **length-agnostic** (payload size derived from the header `sample_count`, so
read_raw 8000 / read_fft 4096 both parse). Added `Pic0rick.status()` /
`capture()` / `read_fft()` / `read_raw()`. Dropped the hard `EXPECTED_FIRMWARE
== "1.6"` gate — the host now just parses whatever `firmware=A.B.C` the board
reports. Verified on hardware: status + read_fft (4096 f32) + read_raw (8000 u16).

**P6 — Build matrix + CI.** `build.sh` variants: rp2040 (mux/nomux),
rp2350 (mux/nomux × dsp/nodsp) = 6 uf2. Update `.github/workflows/firmware.yml`
artifact/release list. Firmware patch bump.

**P7 — Retire onboard_dsp.** Move keep-worthy docs (`understanding_figures.md`,
`test_guide.md`, notebooks) under `docs/` or `python/`; `git rm -r
experiments/onboard_dsp`; fix all references (README, docs/claude notes,
`check-links`). Update `docs/claude/onboard-dsp-firmware.md` to point at the new
locations (or fold into `firmware.md`).

**P8 — Docs + versions.** Update `firmware.md`, README, DONE/commit logs, memory.

---

## Decisions (locked 2026-09-13)
1. **USB model: Design A** — DSP build adopts onboard_dsp's raw-TinyUSB transport
   + CLI loop; RP2040 keeps SDK stdio. ✅
2. **DAC: spi1 on the DSP build, PIO DAC unchanged on RP2040.** ✅ (no RP2040
   behaviour change; PIO DAC + `max14866.pio` shifter stay for the non-DSP path.)

## Still needed (later phases, not blocking P0–P3)
3. **Pulser pin map:** which pins the real RP2040 vs RP2350 boards wire — blocks
   the P4 pulser merge. Until then each build keeps its own pulser.
4. **Version handshake:** intend to replace onboard_dsp's
   `EXPECTED_FIRMWARE == "1.6"` gate with the `version.yaml` A.B.C scheme in P5 —
   confirm when we get there.
