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

**P0 — SDK 2.3.0.** Bump `firmware/CMakeLists.txt` sdk/toolchain/picotool to
2.3.0/15_2_Rel1/2.3.0. Rebuild all 4 current variants; confirm still green.
Firmware patch bump.

**P1 — DSP option scaffolding.** Add `option(DSP ... OFF)`. Guard:
`if(DSP AND NOT PICO_BOARD STREQUAL "pico2") -> FATAL_ERROR`. Add the CMSIS-DSP
`FetchContent` + `cmsisdsp_p0rk` static lib (copy from onboard_dsp CMake), all
inside `if(DSP)`. No sources yet. Configure-only check.

**P2 — Land the modules.** Copy into `firmware/dsp/` (new subdir):
`dsp.[ch]`, `pipeline.[ch]`, `protocol.[ch]`, `acquisition.[ch]` + `.pio`,
`u4rk.h`, `usb_transport.[ch]`, `usb_descriptors.c`, `tusb_config.h`,
`pulser.pio`, and the spi1 `dac`. Compile them only under `if(DSP)`. Keep the
`u4rk_`/`U4RK_` names to minimise churn.

**P3 — Command loop (needs USB decision).** Under `-DDSP`, build the
raw-TinyUSB main loop (from onboard_dsp `main.c`) instead of the stdio REPL;
add the `version` command and `#ifdef MUX` mux commands to it. Under no-DSP,
keep today's stdio `main.c`. Split `main.c` accordingly (e.g. `main_stdio.c` /
`main_dsp.c`, selected in CMake).

**P4 — Reconcile hardware** per the table above (DAC unify decision; pulser
pending hardware confirm).

**P5 — Host + version.** Fold `experiments/onboard_dsp/tools/pic0rick_capture.py`
into `python/pic0rick/` (frame/CRC/status parser); reconcile the
`EXPECTED_FIRMWARE` handshake with our `version.yaml` scheme. Python patch bump.

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

## Open items to confirm before starting
1. **USB model: Design A** (adopt raw-TinyUSB on the DSP build) — confirm.
2. **DAC:** unify RP2040 onto spi1 too, or leave RP2040 on the PIO DAC?
3. **Pulser pin map:** which pins do the real RP2040 vs RP2350 boards wire? (blocks P4 pulser merge)
4. **Version handshake:** replace onboard_dsp's `EXPECTED_FIRMWARE == "1.6"` gate
   with the `version.yaml` A.B.C scheme (proposed) — confirm.
