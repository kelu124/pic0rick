# 2026-09-14 — DSP notebook shows Pico text replies + us times

Python library **v0.1.6**. Branch `feature/unify-firmware-dsp`.

## What
- `pic0rick/dsp.py`: added `describe_status()` — a readable multi-line summary of
  a status line/dict, including the per-stage DSP microsecond times.
- `pic0rick/device.py`: `status()` now includes the raw Pico line under `"raw"`;
  `capture()` records the leading `OK capture started ...` control line as
  `self.last_reply`.
- `python/example_dsp.ipynb`: each step now prints the Pico's text replies —
  `version` (verbatim), `status` (raw line + `describe_status` breakdown),
  `dac write` reply, the `OK capture started ...` line per capture, the frame
  header params, and after `read_fft` the DSP stage/us times; pulser commands
  echo their `OK ...` replies.

## Why
User wanted to see the device's text answers (parameters + µs times) in the
notebook.

## Verified on hardware (RP2350A, DSP v0.1.11, mux)
Executed via `jupyter nbconvert --execute`, exit 0. Sample output:
- status stage times after a read_fft: preprocess 544, forward_fft 4539, mask
  159, inverse_fft 4528, magnitude 1066 us; dsp_us 10841 (over-budget).
- `OK dac=300`, `OK capture started type=raw samples=8000`, etc. all shown.
- Pulsed envelope peak 1164 vs idle 94.5 (echoes present on the mux build).
