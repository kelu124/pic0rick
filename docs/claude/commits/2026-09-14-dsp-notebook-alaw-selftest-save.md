# 2026-09-14 — example_dsp.ipynb: arm-from-start, A-law, self-test, file saving

Python library **v0.1.7**. Branch `feature/unify-firmware-dsp`.

## What (per user request)
Reworked `python/example_dsp.ipynb` to combine the user's early-arm tweak with
the richer steps from the retired onboard_dsp `DSP_Tests.ipynb`:
- **Pulser armed from the start** (gain + `dsp scale` + `pulse config` + `pulser
  arm` before any capture), disarmed at the end.
- **A-law** section: `acq alaw` → `dsp.alaw_decode(bytes, reference)` overlaid on
  the Hilbert envelope (the user wanted the A-law signal shown too).
- **DSP self-test**: streams the 21 vectors, read via `FrameReader`, summarized
  by case name (`dsp.SELFTEST_NAMES`) + envelope peak.
- **Pulser-order** check (pos-first vs neg-first transmit region).
- **Per-capture file saving** (`grab()` helper): `captures/<name>/<payload>.npy`
  + `header.json` + `status.txt`; a final cell lists the saved tree.
- Every Pico text reply + per-stage DSP µs times shown throughout.
- Added `dsp.SELFTEST_NAMES` to `pic0rick/dsp.py`.
- `.gitignore`: `captures/`, `python/raw.npy`, `python/envelope.npy`.

## Verified on hardware (RP2350A, DSP v0.1.11, mux, pulser armed)
Executed via `jupyter nbconvert --execute` (exit 0):
- raw (min/mean/max 1/567/1023 — saturating with the pulser), envelope
  (peak 984.8), A-law (uint8 1..255, ref 512) all captured + saved.
- self-test: 21 frames, cases zero/dc/sinusoid/am/two-bursts/impulse/clipping
  with expected increasing peaks (…impulse 510.9, clipping 1276.7).
- per-stage DSP µs shown (e.g. forward_fft ~4.4 ms, dsp_us ~10.7 ms).
- `captures/{raw,envelope,alaw}/` files written; cleaned after the test.

## Note
Full CLI-style `validate_selftest`/`run` from the old tool were not re-added; the
notebook reads + summarizes the self-test frames instead. Can port strict
numeric validation later if wanted.
