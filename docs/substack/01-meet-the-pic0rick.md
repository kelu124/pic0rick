---
title: "Meet the pic0rick: an open ultrasound board you can solder"
subtitle: "Three small boards, a Raspberry Pi Pico, and your first echo"
series: "pic0rick hands-on, part 1"
pic0rick_python: "0.1.8"
firmware: "0.1.11 (-DDSP, RP2350)"
notebook: "notebooks/01_first_contact.ipynb"
status: draft
---

# Meet the pic0rick: an open ultrasound board you can solder

*Three small boards, a Raspberry Pi Pico, and your first echo.*

Ultrasound is one of those technologies that looks like magic from the outside
and like a shopping list of very specific chips from the inside. The [pic0rick](https://un0rick.cc/pic0rick)
is my attempt to make that shopping list short, open and hand-solderable: a
pulse-echo front end built around a Raspberry Pi Pico (RP2040 or RP2350) that
you can assemble on a kitchen table and drive from a Python notebook.

This is the first of a hands-on series. By the end of this post you will have
an assembled board, flashed firmware, and a first raw frame on screen. Every
figure in this series comes from a Jupyter notebook in the repository, so you
can re-run it — with your own board, or offline on recorded data.

![The three pic0rick boards assembled](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/images/v2/20250323_114927.jpg)

## What's on the boards

A pulse-echo system does two things: shout, then listen. The pic0rick splits
those jobs across three boards (full [specifications](https://un0rick.cc/pic0rick#specifications)
and [architecture](https://un0rick.cc/pic0rick#system-architecture) on un0rick.cc).

**The main board** does the listening. A transducer's echo goes through a
transmit/receive protection stage, then an **AD8331** variable-gain amplifier
whose gain is set by an **MCP4812** SPI DAC (from about 7.5 dB to 55.5 dB —
this is the "time-gain compensation" knob we'll turn in part 3). The amplified
signal is digitised by a **60 Msps, 10-bit ADC**, read by the Pico's PIO and
streamed by DMA into an 8000-sample buffer: 133 µs of signal per shot. (The
Pico's PIO is what lets a microcontroller [replace the FPGA](https://un0rick.cc/pic0rick#how-does-it-replace-the-fpga)
the earlier un0rick boards needed.)

**The pulser board** does the shouting. It plugs into the single PMOD-style
header and uses an **MD1213 + TC6320** pair (driver + MOSFET pair) to drive a three-level pulse
(positive, negative, and an active damping phase that stops the transducer
ringing).

**The HV board** generates the **±25 V** rails the pulser needs.

The double header on the main board is free for [add-ons](https://un0rick.cc/pmods) — a VGA output, a
[multiplexer](https://un0rick.cc/max14866) for multi-element probes, PSRAM. (Yes, the headers aren't strictly
PMODs: I needed 5 V on them.)

```
Transducer
   ├──► TX: Pico PIO → MD1213 + TC6320 → ±25 V three-level pulse
   └──► RX: T/R protection → AD8331 (gain ← MCP4812 DAC)
            → 60 Msps 10-bit ADC → PIO + DMA (8000 samples) → USB → Python
```

## Assembly

The kit is deliberately "easy SMD": the fine-pitch parts are already on the
boards, what's left is through-hole. The same steps are in the
[getting-started guide](https://un0rick.cc/getting-started#the-physical-assembly-of-the-device).

![What you get](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/images/assembly/Step0.jpg)

You'll need a few pin headers, a Raspberry Pi Pico (Pico or Pico 2), and SMA
connectors for the transducer.

![Before assembly: headers, Pico, SMA connectors](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/images/assembly/Step1.jpg)

Take your time on the pulser connector — it carries the high-voltage drive, so
check its orientation and that every pin is properly wetted.

![Focus on the pulser connector](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/images/assembly/Step2.jpg)

Once done, the stack looks like this from the top and from below:

![Assembled, top view](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/images/assembly/Step3.jpg)
![Assembled, bottom view](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/images/assembly/Step4.jpg)

If you prefer it small, the whole thing fits within a game-card footprint:

![A compact assembly](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/images/compact_assembly.jpg)

> **Safety note.** The HV board makes ±25 V and the pulser switches it in tens
> of nanoseconds. That isn't mains-dangerous, but it will happily kill a
> pin of your Pico if a header is misaligned. Power up with the transducer
> connected and don't probe the pulser output with a bare scope input.

## Firmware: one tree, a few flavours

The firmware lives in [`firmware/`](https://github.com/kelu124/pic0rick/tree/main/firmware) and builds in several variants: RP2040 or
RP2350, with or without the MAX14866 multiplexer (`-DMUX`), and — on the
RP2350 only — the **DSP build** (`-DDSP`) that computes the echo envelope on
the Pico and talks to the host with CRC-checked binary frames. That's the one
this series uses.

You don't need a toolchain: CI publishes all variants as
[release assets](https://github.com/kelu124/pic0rick/releases) (see also [flashing the firmware](https://un0rick.cc/getting-started#step-2-flash-the-firmware)). To
flash:

1. Hold **BOOTSEL** on the Pico while plugging it in. It mounts as a USB drive.
2. Copy the `.uf2` onto it. The board reboots by itself.
3. From then on, `reboot-dfu` over serial gets you back to the bootloader
   without touching the button.

## First contact, from Python

The host library is the `pic0rick` package in `python/` (this series is written
against **v0.1.8**; every notebook prints the version it runs and warns if it
differs). Dependencies are the usual suspects: `pyserial numpy scipy h5py
matplotlib`.

```python
from pic0rick.device import Pic0rick
from pic0rick import dsp

probe = Pic0rick()                 # auto-detects /dev/ttyACM*, COM*, tty.usbmodem*
print(probe.version()["raw"])      # firmware version, board, build options
print(dsp.describe_status(probe.status()))
```

On my bench board this prints (abridged):

```
pic0rick status
  board/package    pic0rick / RP2350A
  firmware         0.1.11
  dsp backend      f32-rfft-hilbert
  acquisition      4096 samples @ 60 MS/s
  pulse (ns)       neg 96 / damp 6000 / pos 96, neg-first
  dac / scale      50 / 512.0
```

Then fire once and read a raw frame:

```python
probe.set_gain(50)
probe.configure_pulse(negative_ns=96, damp_ns=6000, positive_ns=96, order="neg-first")
probe.arm_pulser()
frame = probe.read_raw()           # 64-byte header + 8000 × uint16, CRC32-checked
probe.disarm_pulser()
raw = frame.samples()
```

![First raw frame](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/01-first-frame.png)

## Is my board alive? Three checks

The companion notebook runs three sanity checks you can use on any freshly
assembled board:

| Check | What to expect | Bench board |
|---|---|---|
| Baseline level | close to mid-scale (512) | 549 codes |
| Baseline noise | a few codes rms | 3.4 codes rms |
| Main bang | the transmit pulse saturates the ADC near t = 0 | 0 … 1023 → pulser fired |

A baseline a few dozen codes off 512 is normal — it's the front end's DC
offset, and it's why the analysis code subtracts the mean before filtering.
If the main bang is missing, look at the pulser and HV boards before anything
else. If the baseline sits at 0 or 1023, look at the ADC and its supply. The
[troubleshooting section](https://un0rick.cc/getting-started#easy-troubleshooting) of the
getting-started guide covers the usual suspects.

## Next

In part 2 we'll put a transducer on a block of steel and read our first
A-scan: what the main bang, the echoes and the noise are, and how to get
from a raw trace to an envelope — on the PC and on the Pico itself.

---

## Go further

**Reference documentation** (un0rick.cc):

- [pic0rick reference page — specifications, architecture, board comparison](https://un0rick.cc/pic0rick)
- [Official getting-started guide — assembly, flashing, first acquisition](https://un0rick.cc/getting-started)
- [Troubleshooting (board not detected, no echo, noisy signal, Linux permissions)](https://un0rick.cc/getting-started#easy-troubleshooting)
- [Get a board (buy one assembled, or build your own)](https://un0rick.cc/pic0rick#get-one)
- [PMOD extensions (HV pulser, PSRAM, VGA…)](https://un0rick.cc/pmods)

**Code, images and data** (GitHub — everything is open):

- Companion notebook: [`01_first_contact.ipynb`](https://github.com/kelu124/pic0rick/blob/main/docs/substack/notebooks/01_first_contact.ipynb) — runs offline on `docs/data/dsp_captures/`; set `LIVE = True` for your own board
- Python library: [`python/`](https://github.com/kelu124/pic0rick/tree/main/python) · firmware: [`firmware/`](https://github.com/kelu124/pic0rick/tree/main/firmware) · [releases](https://github.com/kelu124/pic0rick/releases) · hardware: [`hardware/`](https://github.com/kelu124/pic0rick/tree/main/hardware)
- Repository: [github.com/kelu124/pic0rick](https://github.com/kelu124/pic0rick)

*Written against pic0rick python library v0.1.8 and firmware v0.1.11 (`-DDSP`, RP2350).*
