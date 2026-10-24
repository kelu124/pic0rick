# Editorial plan — pic0rick on Substack

Audience: makers, students and NDT/ultrasound tinkerers who can solder and run
a notebook, not necessarily signal-processing people. Tone: first person,
hands-on, honest about what goes wrong. Each post ≈ 1200–1800 words, 4–8
figures, one companion notebook, ends with a "next time" hook.

Cadence: one post every two weeks. Every post states the pic0rick python
library and firmware versions it was written against.

## Arc 1 — First steps (drafted)

| # | Title | Key idea | Status |
|---|---|---|---|
| 1 | Meet the pic0rick | boards, assembly, flashing, first frame | draft |
| 2 | Your first A-scan | reading a trace; spectrum; envelope on PC and Pico | draft |
| 3 | Turning the knobs | gain and pulse-width sweeps | draft |
| 4 | Measuring steel with sound | thickness from echo spacing; failure modes | draft |

## Arc 2 — Inside the box (ideas)

| # | Working title | Content | Needs |
|---|---|---|---|
| 5 | The DSP on a Pico | the `-DDSP` pipeline: FFT → mask → IFFT → magnitude → A-law; stage timings from `status`; frame format (`docs/dsp_output_formats.md`) | live board for timings |
| 6 | Streaming envelopes | `stream start`, achievable frame rates (envelope ≈50 Hz, A-law ≈70 Hz), a live matplotlib/pyqtgraph view | live board |
| 7 | PIO + DMA at 60 MS/s | how the RP2040/RP2350 PIO samples a parallel ADC; why PIO1 state machines must never be hard-coded (the MUX/pulser no-echo bug) | firmware reading |
| 8 | Building the firmware | `firmware/build.sh`, variants (`-DMUX`, `-DDSP`), CI releases, `reboot-dfu` loop | — |

## Arc 3 — Going further (ideas)

| # | Working title | Content | Needs |
|---|---|---|---|
| 9 | Without guessing the answer | period estimation from envelope autocorrelation instead of seeding `detect_echoes` (follow-up to #4; starting point: `python/example_process.ipynb`) | offline data |
| 10 | Many elements, one ADC | the MAX14866 MUX board, `write mux`, scanning a multi-element probe | MUX board + probe |
| 11 | A hand-held B-mode | sweep the probe, stack envelopes into an image | encoder or careful hand scan |
| 12 | Time-gain compensation for real | ramping the AD8331 gain during the acquisition | firmware work |
| 13 | Making your own boards | `hardware/build.sh` (KiBot): gerbers, BOM, 3D renders, ordering | — |
| 14 | Data hygiene for experiments | HDF5 archiving, dedup keys, reproducible notebooks | — |
| 15 | A VGA scope for €0 | the VGA-branch demo | VGA board |

## Open questions / to verify before publishing

- Part 4: the 25 mm step reads 24.3 mm consistently — caliper the block before
  publishing, adjust the text.
- Part 4: re-shoot the 20 mm step with the probe centred, keep the bad shot as
  the cautionary example.
- Part 1: confirm the kit contents/BOM wording and whether to link a shop.
- Part 2: re-shoot the on-board envelope (`read_fft`) on the 10 mm step at the
  same settings as the RF trace so the two figures are directly comparable.
