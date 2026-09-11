# onboard_dsp firmware — working notes

Scope: `experiments/onboard_dsp/`. RP2350A (Pico 2) firmware that captures 4096
ADC samples @ 60 MS/s, computes the Hilbert envelope via a float32 real FFT
(CMSIS-DSP), and optionally A-law compresses it. Talks to the PC over USB-CDC.

## Folder layout (important: split across two levels)

```
experiments/onboard_dsp/
├── Readme.md                 # package overview
├── test_guide.md             # step-by-step firmware test guide
├── understanding_figures.md  # <-- reference for EVERY firmware output (status line, frames, ERR codes)
├── DSP_Tests.ipynb           # Python port of test_guide.md; imports the capture tool
├── tools/
│   ├── pic0rick_capture.py   # PC capture / CRC / self-test validation + status parser
│   └── requirements.txt
└── firmware/
    ├── CMakeLists.txt         # sets version + PICO_BOARD=pico2, fetches CMSIS-DSP v1.17.0
    ├── build.sh               # cmake -B build2350 -DPICO_BOARD=pico2 && build && cp uf2 to root
    ├── pic0rick-envelope.uf2  # shipped prebuilt (package root of firmware/)
    ├── pic0rick/              # firmware C sources + PIO
    └── build2350/             # build tree (git-tracked; also holds _deps/cmsisdsp-src)
```

Note the `Readme.md`/`test_guide.md`/`tools/` live in `onboard_dsp/`, but
`CMakeLists.txt`/`pic0rick/`/prebuilt uf2 live in `onboard_dsp/firmware/`. The
Readme's prose sometimes talks as if it's all one flat folder — don't be
confused by that.

## Firmware source map (`firmware/pic0rick/`)

- `main.c` — core-0 command loop. `send_status()` builds the status line;
  `send_help()` the help line; `process_command()` handles every text command.
- `dsp.c` — `u4rk_dsp_envelope()` is the 6-stage pipeline (see below); also the
  A-law LUT (A=87.6) and the 7 deterministic self-test vectors.
- `acquisition.c` — ADC PIO + DMA capture, and the two-SM PIO pulser
  (drive P+/P-, gate PDAMP/OE). Pulse durations round to the 8 ns PIO tick,
  min 5 ticks = 40 ns.
- `pipeline.c` — core-1 worker; queues, drop accounting, frame assembly.
- `protocol.c` — 64-byte little-endian frame header + IEEE CRC32.
- `dac.c` — MCP4812 over spi1, 2 MHz mode-0, CS active-low.
- `u4rk.h` — all constants (pins, rates, sample count, flags).

## Key constants

- samples 4096, sample_rate 60 MHz.
- Max stream rates: raw 100 / envelope 50 / A-law 70 Hz (`U4RK_*_MAX_RATE_HZ`).
- DSP time budget target 4500 µs (`U4RK_DSP_TARGET_US`); real worst case is
  ~12 ms, so `performance=over-budget` is normal for this build.
- Pins: ADC clk 0, data 1–10; DAC CS/SCK/MOSI 13/14/15; pulser P+ 11, P- 12,
  PDAMP 16, OE 17. GPIO18–21 & 28 (old MAX14866) are untouched.

## The 6 DSP stages (`stages_us` order)

`preprocess / forward_fft / mask / inverse_fft / magnitude / alaw`.
Raw and legacy captures skip the FFT, so they report `stages_us=0/…`, `dsp_us=0`,
`worst_us=0`. `worst_us` is a running max inside `dsp.c`, so it re-appears after
the next envelope/A-law frame. Full field-by-field meaning is in
`understanding_figures.md`.

## Two output channels

Text lines (`OK …` / `ERR <code> <msg>`) for control; 64-byte-header binary
frames for data (types 1=raw uint16, 2=envelope float32, 3=A-law uint8). No text
is interleaved while frames are in flight.

## Build / flash

```
cd experiments/onboard_dsp/firmware && ./build.sh
```
Toolchain on this machine: `arm-none-eabi-gcc` present; SDK resolved via
`~/.pico-sdk/sdk/2.3.0` (the CMakeLists includes the pico-vscode cmake if
present). `PICO_SDK_PATH` env may point elsewhere (`~/projets/pico-sdk`) but the
vscode include wins. GitHub reachable for the CMSIS-DSP FetchContent.

**Gotcha:** a stale `build2350/CMakeCache.txt` can pin the *old* source path
(pre-`experiments/` move) and fail with "does not match the source used to
generate cache". Fix: `rm -rf build2350` then `./build.sh`.

## Version coupling (bites easily)

The firmware version appears in THREE places that must agree:
1. `firmware/CMakeLists.txt` → `pico_set_program_version("X.Y")`
2. `tools/pic0rick_capture.py` → `EXPECTED_FIRMWARE = "X.Y"` (the tool REJECTS a
   board reporting anything else)
3. `test_guide.md` (status example + "reports firmware `X.Y`")
Also rebuild the prebuilt uf2 so the embedded string matches, and the notebook's
saved example outputs. Currently all aligned at **1.6**.

Verify the embedded version:
`strings firmware/pic0rick-envelope.uf2 | grep -xE '1\.[0-9]'`

## Status-line parser (added for humans)

`tools/pic0rick_capture.py` has `parse_status(line) -> dict` and
`describe_status(line) -> str`. `STATUS_STAGE_NAMES` names the `stages_us`
slots. The notebook demonstrates them right after the `status` command
(section 3).

## Work log

- 2026-09-11/12: Audited Readme/test_guide against firmware — all substantive
  claims (pins, commands, DSP, pulser, protocol) matched. Fixed a broken guide
  cross-reference (`PIC0RICK_TEST_GUIDE.md` → `test_guide.md`) in Readme + the
  notebook. Bumped firmware 1.5→1.6 (user did CMake+tool; I synced test_guide,
  rebuilt the uf2, corrected stale RP2040/RP2350b wording + 5 Hz limits in the
  notebook to RP2350A / 50–70 Hz). Wrote `understanding_figures.md`, added the
  status parser, wired it into the notebook.
