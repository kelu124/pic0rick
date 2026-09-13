# firmware/ (root) — working notes

Scope: the repo-root **`firmware/`** folder. This is the **original / mainline**
pic0rick Pico firmware — CMake project **`adc-pulse`**, version `0.1`, authored by
Abdelrahman Ali. Do **not** confuse it with `experiments/onboard_dsp/firmware/`
(the newer RP2350A envelope/A-law DSP build). Key difference at a glance: this
mainline build **uses the MAX14866** HV mux; the onboard_dsp experiment
deliberately drops it.

## First-glance layout

```
firmware/
├── main.c              # tiny USB-stdio REPL + command dispatch
├── CMakeLists.txt      # project adc-pulse; PICO_BOARD default = pico (rp2040)
├── build.sh            # builds BOTH rp2040 and rp2350 targets
├── pico_sdk_import.cmake
├── adc/                # ADC capture over PIO + DMA, and the pulser
│   ├── adc.c / adc.h / adc.pio
├── max/                # MAX14866 HV mux driver (SPI bit-bang via PIO)
│   ├── max14866.c / max14866.h / max14866.pio
├── rp2040.uf2          # prebuilt (Pico / RP2040)
├── rp2350.uf2          # prebuilt (Pico 2 / RP2350)
└── build2040/ build2350/   # build trees (gitignored)
```

## What it does

USB-CDC serial **text REPL** (via `stdio_usb`, not a raw binary protocol like
onboard_dsp). On boot it waits for a USB connection, inits ADC → DAC → MAX14866,
then loops printing a `run> ` prompt and reading a line. `main.c`'s
`command_list` maps text to handlers:

| Command | Handler | Purpose |
|---|---|---|
| `start acq` | `pulse_adc_trigger` | Fire the pulser + trigger an ADC/DMA acquisition |
| `read` | `adc` | Dump the captured ADC samples |
| `write dac <v>` | `dac` | Write the gain DAC (MCP4812) |
| `version` | `version_cmd` | Print version, changes, board/chip, mux, git hash, release URL |
| `write mux <v>` | `max14866` | Write the MAX14866 mux word *(MUX builds only)* |
| `set mux` | `max14866_set` | MAX14866 SET *(MUX builds only)* |
| `clear mux` | `max14866_clear` | MAX14866 CLEAR *(MUX builds only)* |

Parsing is whitespace `strtok` (command + subcommand + rest-as-args); a missing
arg defaults to `"0"`. Unknown input prints `Unknown command: ...`. Output is
human-oriented `printf` text, not framed binary.

## Key constants

- `adc/adc.h`: `PIN_BASE 0`, `SAMPLE_COUNT 8000`, `ADC_CLK 120 MHz`,
  `PULSE_CLK 125 MHz`, pulser pins `GPIO11` & `GPIO16`, `DMA_TIMEOUT_MS 3000`.
- `max/max14866.h`: SPI bit-bang pins `DIN 18`, `SCLK 19`, `LE 20`, `SET 21`,
  `CLR 28`, `MAX14866_CLK 2 MHz`. (These are exactly the GPIOs the onboard_dsp
  build leaves uninitialised.)

## Build / flash

```
cd firmware && ./build.sh          # builds FOUR variants into firmware/dist/:
#   rp2040-mux, rp2040-nomux, rp2350-mux, rp2350-nomux
#   + copies the two mux builds to rp2040.uf2 / rp2350.uf2 (historical names)
```
- SDK **2.2.0**, toolchain **14_2_Rel1** (per CMakeLists; note onboard_dsp uses
  the newer SDK 2.3.0). SDK resolved via the pico-vscode cmake include, else
  `PICO_SDK_PATH` (that's what CI uses).
- `PICO_BOARD` defaults to `pico` (RP2040) if not passed.
- **`-DMUX=ON` (default) / `-DMUX=OFF`** toggles the MAX14866 mux module. It
  gates `max/max14866.c` in/out of the build and defines/undefines `MUX`, which
  in `main.c` registers or omits the `write/set/clear mux` commands and the
  `max14866_init()` call. The gain DAC lives in `max/dac.c` and is **always**
  built (it reuses the `max14866.pio` SPI-shifter program, always generated).
- To build just one: `cmake -B build2350 -DPICO_BOARD=pico2 -DMUX=OFF && cmake --build build2350`.

## Versioning & CI

- **`firmware/version.yaml`** (`version: "A.B.C"` + `changes: "..."`) is the
  single source of truth. CMake parses it, generates `version.h`
  (`FW_VERSION`, `FW_CHANGES`, `FW_BOARD`, `FW_CHIP`, `FW_RELEASE_URL`), and the
  `version` CLI command prints them.
- **Build metadata:** `FW_BOARD`/`FW_CHIP` come from `PICO_BOARD`;
  `FW_RELEASE_URL` is derived from the git remote + the `fw-v<version>` tag (it
  points at the release for the *clean* version even on a dirty build). The git
  hash is in a **separate** `version_git.h` regenerated on **every build** by
  `cmake/git_version.cmake` (`FW_GIT_HASH`, `-dirty` if `firmware/` has
  uncommitted changes).
- **Bump rule:** Claude bumps only the **patch (C)** on every change to
  `firmware/` files; the maintainer owns major/minor. A/B 0-99, C 0-999.
  (Python has its own identical scheme in `python/pic0rick/version.yaml`.)
- **`.github/workflows/firmware.yml`** builds all four variants on push/PR to
  `main`, uploads them as artifacts, and on **push to main** publishes a GitHub
  release tagged `fw-v<version>`. The release body lists **both** the firmware
  and python library versions + their `changes` (python has no separate release
  stream). It skips the release if that tag already exists; other branches never
  release.

## Gotchas

- **Wi-Fi secrets (resolved 2026-09-12):** a `firmware/.env` with plaintext
  `WIFI_SSID` / `WIFI_PASSWORD` used to sit here. It was **never committed**
  (untracked), the build never read it (no CYW43/Wi-Fi linked). It has been
  **deleted** and `.env` is now in both the root and `firmware/.gitignore`.
  If a `firmware/.env` reappears, it's a local-only secret — do not commit it.
- Two prebuilt UF2s (`rp2040.uf2`, `rp2350.uf2`) — pick by target board. These
  are the **mux** builds; nomux variants come from CI releases / `dist/`.
- There is a `version` command now (see Versioning above); still **no**
  `status`/`help` handshake like onboard_dsp. No host-side capture tool lives
  with it.

## Relationship to other firmware

- `experiments/onboard_dsp/firmware/` — see `onboard-dsp-firmware.md`. Newer,
  RP2350A-only, binary framed protocol, real-FFT Hilbert envelope, no MAX14866,
  its own `status` line (documented in
  `experiments/onboard_dsp/understanding_figures.md`).

## Work log

- 2026-09-12: First-glance survey and this note created. Removed the untracked
  `firmware/.env` (Wi-Fi creds) and added `.env` to `firmware/.gitignore`. No
  source-code changes made to `firmware/`.
- 2026-09-13: Added `version` command + `version.yaml`/`version.h.in`; made the
  MAX14866 mux optional via `-DMUX` (split the DAC out to `max/dac.c` so it is
  always built); `build.sh` now builds 4 variants into `dist/`; added the
  `.github/workflows/firmware.yml` build+release workflow. Firmware v0.1.0.
  Verified all 4 variants build locally.
- 2026-09-13: v0.1.1 — `version` command now also prints board/chip, git build
  hash (`version_git.h`, regenerated every build via `cmake/git_version.cmake`,
  `-dirty` when firmware/ is dirty) and the derived release URL. CI release body
  now lists both firmware + python versions. Verified generated headers per
  variant.
