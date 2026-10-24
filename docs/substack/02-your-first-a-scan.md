---
title: "Your first A-scan"
subtitle: "Reading an ultrasound trace: main bang, echoes, spectrum and envelope"
series: "pic0rick hands-on, part 2"
pic0rick_python: "0.1.8"
firmware: "0.1.11 (-DDSP, RP2350)"
notebook: "notebooks/02_first_ascan.ipynb"
status: draft
---

# Your first A-scan

*Reading an ultrasound trace: main bang, echoes, spectrum and envelope.*

In part 1 we assembled the pic0rick and got a raw frame out of it. Now let's
point it at something. The "something" here is a 1018-steel calibration block
with five steps (10 to 25 mm), and the transducer is a 5 MHz single-element
contact probe with a drop of couplant. The plot of amplitude against time
you get from one shot is called an **A-scan** — the oldest and still the most
useful view in ultrasound.

## One line of Python

The `UltrasonicAcquisition` wrapper does the whole sequence for you — set the
gain, configure and arm the pulser, read a CRC-checked raw frame, normalise
the 10-bit codes to `(code − 512) / 512`:

```python
from pic0rick.ndt_acquisition import UltrasonicAcquisition

a = UltrasonicAcquisition.from_probe(
    Fech=60e6, gain=350,              # 60 MS/s, DAC gain code (0–1023)
    pon=95, poff=95, damp=6000,       # pulse half-widths and damping, ns
    piezo_id="my 5MHz probe", target="steel ~10mm",
    h5_path="my_first_ascan.h5",      # every shot is archived, metadata included
)
```

It also caches: run it again with the same parameters and it loads the shot
from the HDF5 file instead of firing. That makes notebooks reproducible — and
it's how the companion notebook runs without a board, on
`docs/data/calibration_block_5steps_1018steel.h5`.

## Anatomy of the trace

![A-scan on 10 mm steel](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/02-ascan-raw.png)

Three things to read in this picture:

- **The main bang** at t ≈ 0. That's our own transmit pulse leaking into the
  receiver. It saturates the ADC, and for a couple of microseconds the
  receiver is blind. Anything that echoes back in that window is lost — which
  is why thin parts are hard for contact probes.
- **Big echo groups** at ≈ 37, 74 and 111 µs, regularly spaced. These are
  the strong reverberations of the acoustic path between the probe and the
  part. Their exact cause depends on the probe and how it is held; for our
  purpose they're a convenient, very bright marker.
- **The back-wall echo train** riding behind each group: a comb of echoes
  decaying slowly. That's the pulse bouncing inside the 10 mm of steel. Its
  *spacing* is what we'll turn into a thickness in part 4.

At gain 350 we're deliberately over-driving: the big echoes clip at ±1 so that
the weaker train stands out. Part 3 is about choosing that trade-off properly.
More annotated A-scans from the un0rick family are on the
[pulse-echo](https://un0rick.cc/UseCase/pulse_echo) and [NDT](https://un0rick.cc/UseCase/NDT) use-case pages.

## What frequency are we hearing?

Take the 35–60 µs window, apply a Hann window, FFT it:

```python
seg = a.signal[w] - a.signal[w].mean()
spec = np.abs(np.fft.rfft(seg * np.hanning(seg.size), n=8192))
```

![Spectrum of the echo window](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/02-spectrum.png)

The peak sits at **5.05 MHz** — the probe's nominal frequency. Sampling at
60 MS/s gives us twelve samples per period, comfortably above Nyquist, and
leaves headroom for 10 MHz probes.

## From RF to envelope

The raw signal ("RF") oscillates at 5 MHz; what we usually want is its
*envelope* — how loud the echo is at each instant. Two steps:

1. a zero-phase Butterworth band-pass around the probe (`a.signal_filtered`,
   4–6 MHz here) to throw away out-of-band noise;
2. the magnitude of the analytic signal, `|hilbert(x)|`.

```python
from scipy.signal import hilbert
a.piezo_central_freq, a.piezo_bandwidth = 5e6, 2e6
env = np.abs(hilbert(a.signal_filtered))
```

![Filtered RF and envelope](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/02-envelope.png)

Now the back-wall train is unmistakable: one peak every ≈3.4 µs, each a bit
smaller than the last as the steel and the interfaces eat some energy per
round trip.

## Letting the Pico do it

The RP2350 has a floating-point unit, and the DSP firmware uses CMSIS-DSP to
run that same Hilbert envelope on the board — forward FFT, mask negative
frequencies, inverse FFT, magnitude. From Python it's a single call:

```python
probe.arm_pulser()
env = probe.read_fft().samples()     # 4096-sample float32 envelope, computed on the Pico
```

![Envelope computed on the RP2350](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/02-onboard-envelope.png)

(This frame is a different bench shot, at low gain, so only the strongest echo
shows.) The on-board envelope is half the size of a raw frame and ready to
plot or threshold, which matters once you want frame rates rather than single
shots — there's also an 8-bit A-law compressed version for streaming. We'll
come back to the DSP pipeline later in the series.

## Takeaways

- An A-scan is amplitude vs. time; time is distance once you know the speed of
  sound.
- The receiver is blind during the main bang.
- Band-pass, then Hilbert magnitude: that's your envelope, on the PC or on the
  Pico.

Next time: the two knobs that make or break a measurement — gain and pulse
width.

---

## Go further

**Reference documentation** (un0rick.cc):

- [Your first acquisition, step by step](https://un0rick.cc/getting-started#step-5-your-first-acquisition)
- [Pulse-echo use case — more A-scans from the un0rick boards](https://un0rick.cc/UseCase/pulse_echo)
- [NDT use case — raw signal, envelope and FFT](https://un0rick.cc/UseCase/NDT)
- [System architecture and signal chain](https://un0rick.cc/pic0rick#system-architecture)

**Code, images and data** (GitHub — everything is open):

- Companion notebook: [`02_first_ascan.ipynb`](https://github.com/kelu124/pic0rick/blob/main/docs/substack/notebooks/02_first_ascan.ipynb) — runs offline on `docs/data/calibration_block_5steps_1018steel.h5` and `docs/data/dsp_captures/`; set `LIVE = True` for your own board
- Python library: [`python/`](https://github.com/kelu124/pic0rick/tree/main/python) · firmware: [`firmware/`](https://github.com/kelu124/pic0rick/tree/main/firmware) · [releases](https://github.com/kelu124/pic0rick/releases) · hardware: [`hardware/`](https://github.com/kelu124/pic0rick/tree/main/hardware)
- Repository: [github.com/kelu124/pic0rick](https://github.com/kelu124/pic0rick)

*Written against pic0rick python library v0.1.8 and firmware v0.1.11 (`-DDSP`, RP2350).*
