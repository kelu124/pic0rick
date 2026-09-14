# 2026-09-13 — DSP example notebook + DAC command fix

Python library **v0.1.5**. Branch `feature/unify-firmware-dsp`.

## What
- Added `python/example_dsp.ipynb` — a step-by-step notebook version of
  `example_dsp.py` (imports → connect → status → set gain → read_raw → read_fft →
  pulsed read_fft → optional save), with inline matplotlib plots.
- **Bug fix:** the DSP firmware's DAC command is `dac write <n>`, not the stdio
  build's `write dac <n>`. `example_dsp.py` (and the notebook) used
  `probe.dac()` / `write dac`, which the DSP build rejected with
  `ERR COMMAND unknown command` — the gain was silently not set. Both now send
  `dac write <n>` over serial.

## Verified on hardware (RP2350A, DSP v0.1.9)
Executed the notebook with `jupyter nbconvert --execute`:
- DAC cell now returns `OK dac=300` (was ERR); the gain visibly raises the raw
  mean (397 → 563).
- status, read_raw (8000 u16), read_fft (4096 f32), and the pulsed capture
  (idle peak 63.6 → pulsed 181.0) all succeed; 3 plots render; no errors.
- `example_dsp.py` carries the same fix.

## Notes
- Notebook uses `nbformat_minor: 4` (no per-cell id warning).
- Running the notebook writes `raw.npy` / `envelope.npy` into `python/` (the
  optional save cell) — local artifacts, not committed.
