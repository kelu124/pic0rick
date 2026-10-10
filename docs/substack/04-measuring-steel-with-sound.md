---
title: "Measuring steel with sound"
subtitle: "Turning the pic0rick into an ultrasonic thickness gauge — and where it goes wrong"
series: "pic0rick hands-on, part 4"
pic0rick_python: "0.1.8"
firmware: "0.1.11 (-DDSP, RP2350)"
notebook: "notebooks/04_thickness_gauge.ipynb"
status: draft
---

# Measuring steel with sound

*Turning the pic0rick into an ultrasonic thickness gauge — and where it goes
wrong.*

Ultrasonic thickness gauging is the bread and butter of [non-destructive
testing](https://un0rick.cc/UseCase/NDT): put a probe on one side of a pipe, a hull or a tank wall, and read
how much metal is left — without cutting anything. Commercial gauges cost
hundreds to thousands of euros. With the gain and pulse settings from part 3,
the pic0rick gets surprisingly close.

## The principle in one line

A pulse bounces back and forth inside the part. Two consecutive back-wall
echoes are separated by one round trip:

$$\Delta t = \frac{2d}{c} \quad\Longrightarrow\quad d = \frac{c\,\Delta t}{2}$$

With c = 5950 m/s (longitudinal waves in 1018 steel):

| step | expected Δt |
|---:|---:|
| 10 mm | 3.36 µs |
| 15 mm | 5.04 µs |
| 20 mm | 6.72 µs |
| 25 mm | 8.40 µs |

The trick is to measure the **spacing between echoes**, not the arrival time
of the first one. Whatever sits between the probe crystal and the steel —
wear plate, couplant, a few microns of paint — adds the same delay to every
echo and cancels out in the difference.

## The detector

`UltrasonicAcquisition.detect_echoes()` implements that idea in a few robust
steps:

1. band-pass the trace and rectify it, `|filtered|`;
2. smooth it (`smooth` passes of a `smooth_kernel`-wide moving average) into a
   "work signal";
3. find the largest peak in a time window;
4. predict the next echoes at multiples of the expected Δt and accept the
   local maximum within ±30 % of each prediction if it's above 10 % of the
   main peak;
5. measure Δt from the first to the last accepted echo, and back out d.

```python
a = UltrasonicAcquisition.from_probe(Fech=60e6, gain=350, pon=95, poff=95, damp=6000,
                                     piezo_central_freq=5e6, piezo_bandwidth=2e6,
                                     target="steel ~10mm", h5_path="thickness.h5")
r = a.detect_echoes(start_us=35, end_us=60, target_thickness=0.010,
                    speed_of_sound=5950, smooth=15, smooth_kernel=7)
r["measured_dt_us"], r["calculated_thickness"]
```

Note `pon = poff = 95` ns: the optimum we found in part 3.

## 10 mm: textbook

![Echo detection on the 10 mm step](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/04-echoes-10mm.png)

Seven back-wall echoes, 37.45 → 57.62 µs. Six intervals, Δt = 3.361 µs,
**d = 9.999 mm**. Using many intervals is what makes this precise: a single
sample at 60 MS/s is 16.7 ns, which would be 0.05 mm of steel on its own;
spread over six round trips, the timing error shrinks accordingly.

## 15 mm: still good

![Echo detection on the 15 mm step](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/04-echoes-15mm.png)

Three echoes kept, Δt = 5.07 µs, **d = 15.09 mm** (+0.6 %).

## 25 mm: consistent, but off

![Echo detection on the 25 mm step](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/04-echoes-25mm.png)

Two intervals, Δt = 8.16 µs, **d = 24.27 mm** — 0.7 mm (3 %) short. Look at
the whole trace though: the echoes at 17.2, 25.5, 33.4, 49.7 and 66.1 µs are
all spaced ≈8.15 µs. The detector is not confused; the data simply says 8.15
µs. Either this step is really ≈24.3 mm, or something else (the velocity, the
couplant layer, the step geometry) differs. The honest next step is a caliper
measurement of the block — a gauge is only as good as its calibration.

## 20 mm: the cautionary tale

![Echo detection on the 20 mm step](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/04-echoes-20mm.png)

Here the detector reports **22.6 mm**. Look at the trace: the strong echoes
come at 30.5, 35.5, 40.5, 45.6 µs — spaced **5 µs**, which is the 15 mm
period. The probe was very likely sitting partly on the 15 mm step, and the
detector, told to expect 6.72 µs, stitched together echoes that don't belong
to the same train (it skipped 40.5 µs and kept 45.6 µs).

![Measured vs nominal thickness](https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/04-summary.png)

Two lessons, both standard NDT practice:

- **Look at the A-scan, not just the number.** A thickness gauge that only
  shows digits would have reported 22.6 mm with a straight face.
- **Don't feed the answer to the detector.** Seeding with the expected
  thickness is convenient on a calibration block; on an unknown part, estimate
  the period first — the autocorrelation of the envelope is a good start —
  and only then track echoes. That's a nice exercise for a future post.

## Results

| nominal | Δt measured | d measured | error |
|---:|---:|---:|---:|
| 10 mm | 3.36 µs | 10.00 mm | −0.00 mm |
| 15 mm | 5.07 µs | 15.09 mm | +0.09 mm |
| 20 mm | 7.61 µs | 22.63 mm | +2.63 mm ⚠ probe on the step edge |
| 25 mm | 8.16 µs | 24.27 mm | −0.73 mm |

Not bad for a board you soldered yourself, and every number above can be
re-derived from the archived HDF5 file without touching hardware.

## Where next

That's the end of the "first steps" arc: assemble, acquire, tune, measure.
Next in the series: what the on-board DSP is actually doing, and how to stream
envelopes fast enough to scan by hand.

---

## Go further

**Reference documentation** (un0rick.cc):

- [NDT use case on un0rick.cc](https://un0rick.cc/UseCase/NDT)
- [All use cases — which board for which job](https://un0rick.cc/use_cases)
- [The analysis library (`ndt_acquisition.py`)](https://github.com/kelu124/pic0rick/blob/main/python/pic0rick/ndt_acquisition.py)
- [Get a board](https://un0rick.cc/pic0rick#get-one)

**Code, images and data** (GitHub — everything is open):

- Companion notebook: [`04_thickness_gauge.ipynb`](https://github.com/kelu124/pic0rick/blob/main/docs/substack/notebooks/04_thickness_gauge.ipynb) — runs offline on `docs/data/calibration_block_5steps_1018steel.h5`; set `LIVE = True` for your own board
- Python library: [`python/`](https://github.com/kelu124/pic0rick/tree/main/python) · firmware: [`firmware/`](https://github.com/kelu124/pic0rick/tree/main/firmware) · [releases](https://github.com/kelu124/pic0rick/releases) · hardware: [`hardware/`](https://github.com/kelu124/pic0rick/tree/main/hardware)
- Repository: [github.com/kelu124/pic0rick](https://github.com/kelu124/pic0rick)

*Written against pic0rick python library v0.1.8 and firmware v0.1.11 (`-DDSP`, RP2350).*
