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
| `write dac <v>` | `dac` | Write the DAC |
| `write mux <v>` | `max14866` | Write the MAX14866 mux word |
| `set mux <v>` | `max14866_set` | MAX14866 SET |
| `clear mux <v>` | `max14866_clear` | MAX14866 CLEAR |

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
cd firmware && ./build.sh          # builds BOTH:
#   build2040 (-DPICO_BOARD=pico)  -> rp2040.uf2
#   build2350 (-DPICO_BOARD=pico2) -> rp2350.uf2
```
- SDK **2.2.0**, toolchain **14_2_Rel1** (per CMakeLists; note onboard_dsp uses
  the newer SDK 2.3.0). SDK resolved via the pico-vscode cmake include.
- `PICO_BOARD` defaults to `pico` (RP2040) if not passed.
- To build just one: `cmake -B build2350 -DPICO_BOARD=pico2 && cmake --build build2350`.

## Gotchas

- **Wi-Fi secrets (resolved 2026-09-12):** a `firmware/.env` with plaintext
  `WIFI_SSID` / `WIFI_PASSWORD` used to sit here. It was **never committed**
  (untracked), the build never read it (no CYW43/Wi-Fi linked). It has been
  **deleted** and `.env` is now in both the root and `firmware/.gitignore`.
  If a `firmware/.env` reappears, it's a local-only secret — do not commit it.
- Two prebuilt UF2s (`rp2040.uf2`, `rp2350.uf2`) — pick by target board.
- Version here is `0.1` and there is **no** `status`/`help`/version-check
  handshake, unlike onboard_dsp. No host-side capture tool lives with it.

## Relationship to other firmware

- `experiments/onboard_dsp/firmware/` — see `onboard-dsp-firmware.md`. Newer,
  RP2350A-only, binary framed protocol, real-FFT Hilbert envelope, no MAX14866,
  its own `status` line (documented in
  `experiments/onboard_dsp/understanding_figures.md`).

## Work log

- 2026-09-12: First-glance survey and this note created. Removed the untracked
  `firmware/.env` (Wi-Fi creds) and added `.env` to `firmware/.gitignore`. No
  source-code changes made to `firmware/`.
