# python/ — host tools — working notes

Scope: repo-root **`python/`**. Host-side NDT (non-destructive testing) stack:
drive a pic0rick over serial, capture ADC traces, detect back-wall echoes,
estimate thickness, and store to HDF5. Full user-facing description lives in
`python/Readme.md` (created 2026-09-12).

## What targets what (important)

`python/` speaks to the **mainline `adc-pulse` firmware** (repo-root
`firmware/`) via its **text** commands `write dac` / `start acq` / `read`. It is
The DSP (`-DDSP`) firmware's binary framed protocol is handled by
`pic0rick/dsp.py` + `Pic0rick.status()/capture()/read_fft()/read_raw()` (added
2026-09-13 when the onboard_dsp experiment was folded into the mainline tree).

## Files

- `pic0rick/__init__.py` — exposes `pic0rick.__version__` / `__changes__`, parsed
  from `pic0rick/version.yaml` (A.B.C, mirrors the firmware scheme). No pyserial
  import at package import time (kept lightweight).
- `pic0rick/version.yaml` — library version + `changes`. **Bump rule:** Claude
  bumps only the **patch (C)** on changes to `python/` files; maintainer owns
  major/minor. Independent of the firmware version.
- `pic0rick/device.py` — `Pic0rick` serial driver. `_find_port()` auto-detects
  the CDC port per-OS; methods `dac(N)`, `pulse_adc_trigger(pon,poff,damp)`,
  `read()`, and `write_mux(v)` / `set_mux()` / `clear_mux()` (match firmware,
  MUX builds). 115200 baud, `Fech=60e6`.
  - `version()` sends the firmware `version` command and parses the reply into a
    dict (`version`, `changes`, `board`, `chip`, `mux`, `mux_enabled`, `build`,
    `release`, `raw`); tolerates REPL echo/prompt lines.
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
- 2026-09-13: Added `write_mux`/`set_mux`/`clear_mux` to `Pic0rick` (firmware
  parity); added versioning (`__init__.py` + `version.yaml`, v0.1.0).
- 2026-09-13: v0.1.1 — added `Pic0rick.version()` parsing the firmware `version`
  report into a dict.
- 2026-09-13: v0.1.5 — added `python/example_dsp.ipynb` (step-by-step notebook;
  executed via nbconvert against the RP2350A) and fixed the DAC command in
  `example_dsp.py` (DSP build uses `dac write`, not `write dac`).
- 2026-09-13: v0.1.4 — added `python/example_dsp.py` (end-to-end DSP example) +
  `docs/dsp_test_guide.md`; hardware-tested. (v0.1.3 = docs-only after onboard_dsp
  retirement.)
- 2026-09-13: v0.1.2 — added `pic0rick/dsp.py` (DSP-build binary frame protocol:
  FrameReader+CRC, A-law decode, parse_status; length-agnostic for read_raw 8000
  / read_fft 4096) and `Pic0rick.status()/capture()/read_fft()/read_raw()`.
  Dropped the onboard tool's hard EXPECTED_FIRMWARE gate. Verified on hardware.
