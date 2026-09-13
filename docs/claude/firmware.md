# firmware/ (root) — working notes

Scope: the repo-root **`firmware/`** folder — the **unified** pic0rick Pico
firmware (CMake project `adc-pulse`). One tree with build variants: `-DMUX`
(MAX14866 mux, default on) and `-DDSP` (RP2350 Hilbert-envelope DSP + binary
protocol, off by default). The former `experiments/onboard_dsp/` firmware was
merged in here on 2026-09-13 (see `onboard-dsp-firmware.md`).

## First-glance layout

```
firmware/
├── main.c              # stdio REPL (non-DSP builds) + command dispatch
├── main_dsp.c          # raw-TinyUSB command loop (-DDSP builds)
├── CMakeLists.txt      # project adc-pulse; -DDSP / -DMUX / PICO_BOARD toggles
├── build.sh            # builds all 4 non-DSP variants into dist/
├── version.yaml        # single source of truth for the version
├── hw/                 # SHARED, USB-free drivers (both builds)
│   ├── acquisition.c/.h/.pio   # ADC capture + pulser (u4rk_*)
│   ├── pulser.pio              # 4-pin pulser (P+/P-/PDAMP/OE)
│   ├── dac.c/.h                # spi1 MCP4812 gain DAC (u4rk_dac_*)
│   └── u4rk.h                  # pins, sample counts, protocol structs
├── dsp/                # DSP-only (-DDSP): dsp/pipeline/protocol + raw TinyUSB
│   ├── dsp.c/.h pipeline.c/.h protocol.c/.h
│   └── usb_transport.* usb_descriptors.c tusb_config.h
├── max/                # MAX14866 HV mux driver (SPI bit-bang via PIO)
│   ├── max14866.c / max14866.h / max14866.pio
├── rp2040.uf2 / rp2350.uf2   # prebuilt (mux, non-DSP) defaults
└── build*/ dist/       # build trees / outputs (gitignored)
```

## What it does

USB-CDC serial **text REPL** (via `stdio_usb`, not a raw binary protocol like
onboard_dsp). On boot it waits for a USB connection, inits ADC → DAC → MAX14866,
then loops printing a `run> ` prompt and reading a line. `main.c`'s
`command_list` maps text to handlers:

| Command | Handler | Purpose |
|---|---|---|
| `start acq [pon] [poff] [damp]` | `start_acq_cmd` | Configure+arm shared pulser, capture 8000 samples |
| `read` | `read_cmd` | Dump the captured 8000 ADC samples (hex) |
| `write dac <v>` | `write_dac_cmd` | Write the gain DAC (MCP4812, spi1) |
| `version` | `version_cmd` | Print version, changes, board/chip, mux, git hash, release URL |
| `reboot-dfu` | `reboot_dfu_cmd` | Reboot into the USB bootloader (BOOTSEL) via `reset_usb_boot` |
| `write mux <v>` | `max14866` | Write the MAX14866 mux word *(MUX builds only)* |
| `set mux` | `max14866_set` | MAX14866 SET *(MUX builds only)* |
| `clear mux` | `max14866_clear` | MAX14866 CLEAR *(MUX builds only)* |

Parsing is whitespace `strtok` (command + subcommand + rest-as-args); a missing
arg defaults to `"0"`. Unknown input prints `Unknown command: ...`. Output is
human-oriented `printf` text, not framed binary.

## Key constants

- `hw/u4rk.h`: `U4RK_SAMPLE_COUNT 4096` (FFT), `U4RK_RAW_SAMPLE_COUNT 8000`
  (raw), sample rate 60 MHz; pins: ADC clk 0 / data 1–10, DAC CS/SCK/MOSI
  13/14/15, pulser P+ 11 / P- 12 / PDAMP 16 / OE 17.
- `hw/acquisition.c`: ADC PIO 120 MHz, pulse PIO 125 MHz, 8 ns tick (min 5),
  2 ms DMA timeout.
- `max/max14866.h`: SPI bit-bang pins `DIN 18`, `SCLK 19`, `LE 20`, `SET 21`,
  `CLR 28`, `MAX14866_CLK 2 MHz`.

## Build / flash

```
cd firmware && ./build.sh          # builds SIX variants into firmware/dist/:
#   rp2040-mux, rp2040-nomux, rp2350-mux, rp2350-nomux    (stdio / non-DSP)
#   rp2350-mux-dsp, rp2350-nomux-dsp                      (raw-TinyUSB DSP)
#   + copies the two rp2350/rp2040 mux non-DSP builds to rp2040.uf2 / rp2350.uf2
# DSP builds share one CMSIS-DSP clone in firmware/.deps (gitignored).
```
- SDK **2.3.0**, toolchain **14_2_Rel1** (per CMakeLists; bumped from 2.2.0 in
  fw v0.1.3 on the unify branch — needed for the onboard-DSP/CMSIS-DSP merge).
  SDK resolved via the pico-vscode cmake include, else `PICO_SDK_PATH` (that's
  what CI uses; the workflow checks out pico-sdk `2.3.0`).
- `PICO_BOARD` defaults to `pico` (RP2040) if not passed.
- **`-DMUX=ON` (default) / `-DMUX=OFF`** toggles the MAX14866 mux module. It
  gates `max/max14866.c` in/out of the build and defines/undefines `MUX`, which
  in `main.c` registers or omits the `write/set/clear mux` commands and the
  `max14866_init()` call. The gain DAC lives in `max/dac.c` and is **always**
  built (it reuses the `max14866.pio` SPI-shifter program, always generated).
- To build just one: `cmake -B build2350 -DPICO_BOARD=pico2 -DMUX=OFF && cmake --build build2350`.
- **`-DDSP=ON`** (RP2350/`pico2` only; guarded with a FATAL_ERROR otherwise)
  enables the onboard-DSP feature set. As of P1 it only sets up the option +
  CMSIS-DSP (`cmsisdsp_p0rk` static lib); firmware sources land in later phases.

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
- There is a `version` command now (see Versioning above); the stdio build has
  **no** `status`/`help` handshake (those live in the DSP build's `main_dsp.c`).

## The two build flavours (one tree)

- **stdio / non-DSP** (`main.c`): text REPL over SDK `stdio_usb`; RP2040 or
  RP2350. `start acq` / `write dac` / `read` (raw 8000).
- **DSP** (`main_dsp.c`, `-DDSP`, RP2350 only): raw-TinyUSB command loop with the
  binary framed protocol + Hilbert envelope; adds `read_fft` (4096), `read_raw`
  (8000), `acq`/`stream`/`dsp`/`status`. Frame/status formats:
  `docs/dsp_output_formats.md`. Host side: `pic0rick.dsp` (see
  `python-host-tools.md`).

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
- 2026-09-13: v0.1.2 — added `reboot-dfu` (reset_usb_boot → BOOTSEL; links
  `pico_bootrom`). **Flashed + tested on real RP2350A/Pico 2 W hardware**:
  `version` and the flash→run→`reboot-dfu`→BOOTSEL loop confirmed. See
  `hardware-testing.md`. (On branch `feature/unify-firmware-dsp`.)
- 2026-09-13: v0.1.3 — **P0 of the unify plan**: SDK 2.2.0→2.3.0 (CMakeLists +
  CI workflow ref). All 4 variants build; flashed rp2350 to hardware via the
  reboot-dfu loop and confirmed v0.1.3 runs. Toolchain kept at 14_2_Rel1 (the
  one installed; onboard_dsp nominally used 15_2 but 14_2 builds SDK 2.3.0 fine).
- 2026-09-13: v0.1.4 — **P1**: `-DDSP` option scaffolding (pico2-guarded) +
  CMSIS-DSP `cmsisdsp_p0rk` lib. No firmware sources yet; default builds
  unchanged. Verified guard + CMSIS-DSP fetch/build.
- 2026-09-13: v0.1.5 — **P2**: landed onboard-DSP modules in `firmware/dsp/`;
  built the USB-free set as the `p0rk_dsp` static lib (under `if(DSP)`). USB
  transport files copied but not compiled (P3). Default builds unchanged;
  `libp0rk_dsp.a` verified.
- 2026-09-13: v0.1.7 — **P4a**: two-length acquisition — `read_raw` (8000 raw)
  / `read_fft` (4096 envelope); per-job sample_count threaded through
  acquisition/pipeline/dsp. Verified frame sizes on hardware.
- 2026-09-13: v0.1.11 — **fix MUX/pulser PIO1 SM conflict (the real no-echo
  bug)**: `max14866_init` hardcoded pio1 sm0, clobbering the pulser drive SM
  (pio1 sm0). Now claims an unused SM. Echoes restored on MUX builds — verified
  vs the v0.1.1 baseline (`var[2000:3000]` 0.4 → ~52k at gain 300). **Watch out
  for hardcoded PIO SM indices** now that pulser+mux share PIO1.
- 2026-09-13: v0.1.10 — **pulser echo fix**: `hw/acquisition.c` `queue_pulse`
  now fires the bipolar pulse (both polarities back-to-back) **then** damps, as
  in v0.1.0. The shared `pulser.pio` path (P4b) had put PDAMP *between* the
  polarities, splitting the excitation (regression — user lost echoes). Transmit
  confirmed strong on hardware (~200 counts at sample ~31); echo-vs-target
  confirmation is up to the user's setup.
- 2026-09-13: v0.1.9 — **P6**: `build.sh` builds all 6 variants (adds the two
  rp2350 DSP builds) into `dist/`, sharing one CMSIS-DSP cache (`firmware/.deps`);
  CI caches it and builds/releases all 6.
- 2026-09-13: v0.1.8 — **P4b**: unified HW drivers. Moved the USB-free
  acquisition+pulser+spi1 DAC to `firmware/hw/` (shared by both builds); the
  stdio build's `start acq`/`write dac`/`read` now use them (raw 8000). Retired
  `adc/` (PIO pulser) and `max/dac.c` (PIO DAC). Verified on RP2350 hardware
  (stdio: write dac, 8000-sample start acq + read).
- 2026-09-13: v0.1.6 — **P3**: first runnable DSP build. `main_dsp.c` (raw
  TinyUSB command loop, selected under `if(DSP)`; stdio OFF) links `p0rk_dsp` +
  `usb_transport`/`usb_descriptors`; adds `version`/`reboot-dfu`/`write-set-clear
  mux`. Non-DSP `main.c` unchanged. **Tested on RP2350A hardware**:
  version/help/status + `acq raw` (8256-byte frame) all good. See
  `hardware-testing.md` (DSP CDC = raw TinyUSB, OK/ERR text + binary frames).
