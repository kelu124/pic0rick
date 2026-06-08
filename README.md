![GitHub repo size](https://img.shields.io/github/repo-size/kelu124/pic0rick?style=plastic)
![GitHub language count](https://img.shields.io/github/languages/count/kelu124/pic0rick?style=plastic)
![GitHub top language](https://img.shields.io/github/languages/top/kelu124/pic0rick?style=plastic)
![GitHub last commit](https://img.shields.io/github/last-commit/kelu124/pic0rick?color=red&style=plastic)

[![Slack replacement](https://badgen.net/badge/icon/Matrix.org?icon=matrix&label)](https://matrix.to/#/!dEbJSiragnEvzVBdUa:matrix.org?via=matrix.org)
[![made-with-Markdown](https://img.shields.io/badge/Made%20with-Markdown-1f425f.svg)](http://commonmark.org)


# the _pic0rick_ project

[![Patreon](https://img.shields.io/badge/patreon-donate-orange.svg)](https://www.patreon.com/kelu124)
[![Kofi](https://badgen.net/badge/icon/kofi?icon=kofi&label)](https://ko-fi.com/G2G81MT0G)

The pic0rick is a very central board for an ultrasound pulse-echo system. It is composed of a main board, based on the RP2040 (or RP2350) and easy to solder SMD, to which a single, and a double PMOD connector can connect to addons:

* The main board is equipped with a 60Msps, 10bit ADC. Front end is protected against high-voltage pulses, and features a proven time-gain compensation system consisting in a AD8331 (7.5 dB to 55.5dB) with a controlling (MCP4812) SPI DAC.
* The single PMOD connector can plug to the Pulser board, which can be equipped with a simple +-25V generation board. Together, they generate the pulse on behalf of the pic0rick main board. The setup can generate three-level pulses ( with a pair of MD1210 + TC6320 ).
* The double PMOD connector can be used for virtually anything. The current code allows for a VGA to be connected, which displays acquisitions from the board.

The current system uses both PIOs (one for the acquisition, the other for the VGA) which leaves the other resources of the rp2040 relatively free to use for your own priorities.

Published documents include:
* KiCad design files for the main board
* KiCad design files for the pulser + hv boards
* KiCad design files for the MUX
* KiCad design files for other boards =)
* rp2040/rp2350 firmware for the microcontroller.

I _know_ the PMODs aren't strictly speaking PMODs, I needed to have 5V facility on the header =)

And if you want to discuss the project - [meet us on our chat](https://matrix.to/#/!dEbJSiragnEvzVBdUa:matrix.org?via=matrix.org).

# Getting Started

## Prerequisites

* pic0rick main board + pulser board + HV board (assembled)
* USB-C cable
* Python 3.9+
* A piezoelectric transducer (e.g. 5 MHz single-element contact probe)

## Flashing the firmware

1. Hold the **BOOTSEL** button on the RP2040/RP2350 while connecting USB — the board mounts as a USB mass-storage device.
2. Copy the compiled `.uf2` file from `firmware/` into the drive. The board reboots automatically.
3. Verify: open a serial terminal at 115,200 baud. You should see the `run>` prompt.

## Installing the Python library

```bash
pip install pyserial numpy h5py scipy matplotlib
```

Clone this repo and add `pic0lib/` to your path, or run notebooks from the repo root.

## Quick start

```python
from pic0lib.device import Pic0rick
from pic0lib.ndt_acquisition import UltrasonicAcquisition

probe = Pic0rick()           # auto-detects USB serial port

# Set TGC gain (raw DAC value 0–1023; higher = more gain)
probe.dac(300)

# Trigger a pulse and read 8000 samples at 60 Msps
probe.pulse_adc_trigger(pon=70, poff=70, damp=6000)
raw = probe.read()
```

Or use the higher-level acquisition wrapper:

```python
acq = UltrasonicAcquisition.from_probe(
    Fech=60e6,
    pon=70,        # pulse-on duration [ns]
    poff=70,       # pulse-off duration [ns]
    damp=6000,     # damping duration [ns]
    gain=300,      # raw DAC value (0–1023)
    target="1018 steel 25 mm",
    h5_path="my_scan.h5",
)
acq.plot()
```

## Serial command reference

The firmware exposes a text protocol at 115,200 baud (USB CDC):

| Command | Arguments | Description |
|---------|-----------|-------------|
| `start acq <pon> <poff> <damp>` | nanoseconds | Fire pulse, DMA-capture 8000 samples |
| `write dac <N>` | 0–1023 | Set TGC gain (MCP4812 10-bit DAC) |
| `write mux <N>` | bitmask | Write MAX14866 multiplexer register |
| `set mux <N>` | channel | Enable a multiplexer channel |
| `clear mux <N>` | channel | Disable a multiplexer channel |
| `read` | — | Return last captured buffer (hex, one 10-bit value per line) |

### Parameter details

**`pon` / `poff` / `damp`** — expressed in **nanoseconds** from Python. The firmware converts them to 125 MHz clock cycles internally (divides by 8).

**`gain`** — raw 10-bit DAC value (0–1023) controlling the VGAIN pin of the AD8331 TGC amplifier (7.5 dB to 55.5 dB range). The example notebook uses values in the 0–500 range.

## Signal chain

```
Transducer
   │
   ├──► TX path: RP2040 PIO GPIO11/GPIO16 → MD1210 + TC6320 → ±25V three-level pulse
   │
   └──► RX path: T/R protection → AD8331 TGC (gain set by MCP4812 DAC)
                  → 60 Msps 10-bit ADC (PIO + DMA, 8000 samples/acquisition)
                  → USB CDC → Python
```

The ADC samples are 10-bit unsigned (0–1023, mid-scale = 512). The `ndt_acquisition` wrapper normalises them to float32 in the range ≈ [−1, 1]:

```python
signal = (adc_count - 512) / 512.0
```

# Setup

## The three boards assemble look like this

![](/documentation/images/v2/20250323_114927.jpg)

## Example of acquisitions

![](/documentation/images/pic0gain_at_6.jpg)

## Demo of output to VGA 

Beware. This is not a standard development, more of a proof of concept. It lives on a [separate branch here](https://github.com/kelu124/pic0rick/tree/VGA).

Only using the pico to setup gain and trigger acquisitions. The screen displays the gain value (0 to 9, with a 100x divider). Displays raw buffer of acquisition.

![](/documentation/images/VGA_demo.gif)


## Example of a compact assembly 

Within a game card footprint

![](/documentation/images/compact_assembly.jpg)

# Along with the other boards

![](/documentation/images/sister_boards.png)

# Thank you to

* Abdelrahman
* Lap

# License

This work is based on three previous TAPR projects, [the echOmods project](https://github.com/kelu124/echomods/), the [un0rick project](https://doi.org/10.5281/zenodo.377054), and the [lit3rick project](https://doi.org/10.5281/zenodo.5792245) - their boards are open hardware and software, developped with open-source elements as much as possible.

Copyright Luc Jonveaux (<kelu124@gmail.com>) 2024

* The hardware is licensed under TAPR Open Hardware License (<www.tapr.org/OHL>)
* The software components are free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
* The documentation is licensed under a [Creative Commons Attribution-ShareAlike 3.0 Unported License](http://creativecommons.org/licenses/by-sa/3.0/).

## Disclaimer

This project is distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A PARTICULAR PURPOSE.
