---
title: "Turning the knobs: gain and pulse width"
subtitle: "Two parameters, two sweeps, and how to pick them from data"
series: "pic0rick hands-on, part 3"
pic0rick_python: "0.1.8"
firmware: "0.1.11 (-DDSP, RP2350)"
notebook: "notebooks/03_gain_and_pulse.ipynb"
status: draft
---

# Turning the knobs: gain and pulse width

*Two parameters, two sweeps, and how to pick them from data.*

Every ultrasound acquisition is a compromise between hearing the faint echo
and not being deafened by the loud one. On the pic0rick you control that with
two knobs: the **receive gain** and the **transmit pulse width**. Rather than
guess, we'll sweep both and look.

Same setup as part 2: 5 MHz contact probe, 10 mm step of a 1018-steel block.

## Knob 1: gain

The gain is a DAC code from 0 to 1023 that drives the AD8331's gain pin
(roughly 7.5 dB at the bottom of its range, 55.5 dB at the top — see the
[specifications](https://un0rick.cc/pic0rick#specifications) and the official note on
[adjusting the gain](https://un0rick.cc/getting-started#adjusting-the-gain)). A sweep is
just a loop — `from_probe()` archives every shot in the HDF5 file:

```python
sweep = [UltrasonicAcquisition.from_probe(Fech=60e6, gain=g, pon=70, poff=70, damp=6000,
                                          target="steel ~10mm", h5_path="gain_sweep.h5")
         for g in (0, 100, 200, 300, 400, 500)]
```

![Gain sweep](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/03-gain-sweep.png)

Reading it from top to bottom:

- **Gain 0** — nothing clips, the first echo group peaks at 0.71 full-scale,
  but the back-wall train behind it is barely visible.
- **Gain 100–300** — the strong echoes start clipping, the back-wall train
  comes up, the baseline is still clean. This is the useful range for this
  target.
- **Gain 400–500** — everything is louder, including the noise. The baseline
  starts wandering (that slow drift after the main bang is the amplifier
  recovering from saturation) and the echo train drowns in grass.

Putting numbers on it:

| gain | 1st echo peak | samples clipped | noise rms (20–28 µs) |
|---:|---:|---:|---:|
| 0 | 0.71 | 0.0 % | 0.0015 |
| 100 | 1.00 | 0.6 % | 0.0021 |
| 200 | 1.00 | 1.0 % | 0.0079 |
| 300 | 1.00 | 1.7 % | 0.0059 |
| 400 | 1.00 | 3.4 % | 0.0178 |
| 500 | 1.00 | 5.7 % | 0.0436 |

![Echo vs noise](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/03-gain-metrics.png)

Noise grows roughly 30× between gain 0 and 500, while the signal we care
about stops growing as soon as it clips. **Rule of thumb:** raise the gain
until the *echo you want to measure* is comfortably above the noise and
unclipped — not the biggest echo in the trace. For a thickness measurement on
this block, 250–350 is the sweet spot. For the deeper, weaker 25 mm step we'll
actually go *down* to keep the first back-wall echo unclipped.

> Why "TGC"? The AD8331 is built for *time*-gain compensation: ramping the gain
> during the acquisition so late (deep, attenuated) echoes are amplified more
> than early ones. The pic0rick sets a fixed gain per shot today; a ramp is on
> the list of future experiments.

## Knob 2: pulse width

The [pulser](https://un0rick.cc/pmods#hvpulser--hv-source) sends a three-level pulse: `pon` ns high, `poff` ns low, then
`damp` ns of active damping. A transducer is a resonator — it responds best
when you push it at its own rhythm. For a 5 MHz probe a full period is 200 ns,
so each half (`pon = poff`) should be about **100 ns**.

The library has a helper that sweeps `pon = poff`, archives every shot, and
reports the width that maximises the echo in a window:

```python
calib = UltrasonicAcquisition.calibrate(
    "pulse_sweep.h5", Fech=60e6, start_us=35, end_us=40, gain=40,
    piezo_central_freq=5e6, piezo_bandwidth=4e6, target="steel ~10mm - calibration")
calib["best_pon_poff"]
```

Note the low gain (40): with a clipped echo, every pulse width would look
equally good. Here's the sweep from 25 to 155 ns:

![Pulse-width sweep](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/03-pulse-sweep.png)

The best width is **95 ns**, i.e. an equivalent drive frequency of 5.26 MHz —
right where theory said, within the 10 ns step of the sweep. The curve is
broad (anything between 65 and 105 ns is within 4 % of the best) which is
good news: the setting is forgiving. Too short (25 ns) and you lose 40 % of
the echo; too long and the second half of the pulse starts cancelling the
first.

The echoes themselves show the difference in amplitude, and a subtler change
in shape:

![First echo for three pulse widths](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/03-pulse-echoes.png)

## Takeaways

- Choose gain for the echo you need, not for the loudest one; watch clipping
  and the noise floor in a quiet window.
- Pulse half-width ≈ half the transducer period. Measure it — `calibrate()`
  does it in one call — at a gain where nothing clips.
- Archive every shot with its parameters (HDF5 here). Sweeps become data you
  can revisit, rather than screenshots.

Next time we put all this together and turn the pic0rick into a thickness
gauge.

---

## Go further

**Reference documentation** (un0rick.cc):

- [Adjusting the gain (getting-started guide)](https://un0rick.cc/getting-started#adjusting-the-gain)
- [Specifications — ADC, TGC amplifier, pulser](https://un0rick.cc/pic0rick#specifications)
- [HV pulser + HV source PMOD](https://un0rick.cc/pmods#hvpulser--hv-source)
- [Signal looks very noisy? (troubleshooting)](https://un0rick.cc/getting-started#signal-looks-very-noisy)

**Code, images and data** (GitHub — everything is open):

- Companion notebook: [`03_gain_and_pulse.ipynb`](https://github.com/kelu124/pic0rick/blob/main/docs/substack/notebooks/03_gain_and_pulse.ipynb) — runs offline on `docs/data/calibration_block_5steps_1018steel.h5`; set `LIVE = True` for your own board
- Python library: [`python/`](https://github.com/kelu124/pic0rick/tree/main/python) · firmware: [`firmware/`](https://github.com/kelu124/pic0rick/tree/main/firmware) · [releases](https://github.com/kelu124/pic0rick/releases) · hardware: [`hardware/`](https://github.com/kelu124/pic0rick/tree/main/hardware)
- Repository: [github.com/kelu124/pic0rick](https://github.com/kelu124/pic0rick)

*Written against pic0rick python library v0.1.8 and firmware v0.1.11 (`-DDSP`, RP2350).*
