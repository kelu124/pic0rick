# python/ — host tools — working notes

Scope: repo-root **`python/`**. Host-side NDT (non-destructive testing) stack:
drive a pic0rick over serial, capture ADC traces, detect back-wall echoes,
estimate thickness, and store to HDF5. Full user-facing description lives in
`python/Readme.md` (created 2026-09-12).

## What targets what (important)

`python/` speaks to the **mainline `adc-pulse` firmware** (repo-root
`firmware/`) via its **text** commands `write dac` / `start acq` / `read`. It is
**not** the onboard_dsp stack — that firmware uses a binary framed protocol and
ships its own host tool (`experiments/onboard_dsp/tools/pic0rick_capture.py`).
Two independent host paths coexist; don't cross-wire them.

## Files

- `pic0rick/device.py` — `Pic0rick` serial driver. `_find_port()` auto-detects
  the CDC port per-OS; methods `dac(N)`, `pulse_adc_trigger(pon,poff,damp)`,
  `read()`. 115200 baud, `Fech=60e6`.
- `pic0rick/ndt_acquisition.py` — the real logic. `UltrasonicAcquisition`
  dataclass + `from_probe`, `detect_echoes`, `calibrate`, `amplitude`, `plot`,
  `info`, and HDF5 `save_h5`/`load_h5`. Hardware import is **lazy** (`get_probe`)
  so the module works offline for pure analysis.
- `example_acquisition.ipynb` — live-hardware workflow (gain sweep, calibrate
  best pon/poff, multi-thickness echo detection on a 1018-steel step block,
  5 MHz probe).
- `example_process.ipynb` — offline: `load_h5` a capture, pulse + autocorrelation
  analysis. No board needed.

## Notable details

- Signal decode in `from_probe`: `read()` returns comma-separated **hex** ADC
  codes; it parses the 3rd serial line (`C[2]`), maps each to
  `(int(code,16) - 512) / 512.0` → normalized `[-1, 1]` float32.
- HDF5 dedup key: `(gain, pon, poff, damp, target, piezo_id)`. A cache hit in
  `from_probe(h5_path=...)` skips the hardware unless `overwrite=True`.
- Notebooks read `../docs/data/calibration_block_5steps_1018steel.h5`.
- Deps: pyserial, numpy, scipy, h5py, matplotlib. No `requirements.txt` in
  `python/` yet.

## Work log

- 2026-09-12: Surveyed the folder and wrote `python/Readme.md` (points at both
  notebooks and both modules). No code changes.
