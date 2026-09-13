# 2026-09-13 — DSP test guide + example, firmware CHANGELOG + rule

Python library **v0.1.4**. Branch `feature/unify-firmware-dsp`.

## What
- **`docs/dsp_test_guide.md`** — new: build/flash/serial/Python walkthrough of
  the `-DDSP` firmware against the new API (`read_raw`/`read_fft`, `pic0rick.dsp`,
  `Pic0rick.status/read_fft/read_raw/capture`), replacing the dropped
  onboard_dsp `test_guide.md`/notebook.
- **`python/example_dsp.py`** — new: end-to-end example (status → read_raw →
  read_fft → pulsed read_fft; optional `--save` .npy / `--plot` PNG).
- **`firmware/CHANGELOG.md`** — new: per-version history (0.1.0 → 0.1.9).
- **Rule added:** every firmware version bump must add a `firmware/CHANGELOG.md`
  entry; `version.yaml` + `CHANGELOG.md` are the version record and editing only
  them does not trigger a bump. Recorded in `docs/claude/README.md` and the
  `versioning-rule` memory.

## Verified on hardware (RP2350A, DSP v0.1.9 build)
`python example_dsp.py --save /tmp/dsp_out`:
- status → firmware 0.1.9, RP2350A, backend f32-rfft-hilbert.
- read_raw → 8000 uint16 (min/mean/max 138/342/480).
- read_fft → 4096 float32 envelope, peak 113 (idle).
- read_fft with pulser armed → peak 388.7 (pulser path confirmed).
- raw.npy / envelope.npy saved.

## Note
Creating `firmware/CHANGELOG.md` did not bump the firmware version (it is version
bookkeeping). `example_dsp.py` is a python/ addition → python v0.1.4.
