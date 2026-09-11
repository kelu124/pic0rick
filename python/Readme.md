# pic0rick — Python host tools

Host-side Python for driving a pic0rick over USB serial and turning its raw
ADC traces into ultrasonic NDT (non-destructive testing) measurements: pulse
tuning, gain sweeps, back-wall echo detection, thickness estimation, and HDF5
storage.

> Targets the **mainline `adc-pulse` firmware** (repo-root `firmware/`), whose
> text REPL exposes `write dac`, `start acq`, and `read`. This is **not** the
> binary-protocol firmware in `experiments/onboard_dsp/` — that build ships its
> own host tool (`experiments/onboard_dsp/tools/pic0rick_capture.py`).

## Layout

```
python/
├── pic0rick/
│   ├── device.py            # Pic0rick — serial driver for the board
│   └── ndt_acquisition.py   # UltrasonicAcquisition — capture + analysis + HDF5
├── example_acquisition.ipynb  # live-hardware workflow (needs a board)
└── example_process.ipynb      # offline analysis of a saved HDF5 (no hardware)
```

## Python modules

### `pic0rick/device.py` — the hardware driver
`Pic0rick` opens the CDC serial port (115200 baud; `_find_port()` auto-detects
`/dev/ttyACM*`·`/dev/ttyUSB*` on Linux, `COM*` on Windows, `tty.usbmodem*` on
macOS) and wraps the firmware's three text commands:

| Method | Firmware command | Purpose |
|---|---|---|
| `dac(N)` | `write dac N` | Set the DAC (drives the analog gain / VGA). |
| `pulse_adc_trigger(pon, poff, damp)` | `start acq …` | Fire the pulser and capture a trace. |
| `read()` | `read` | Return the captured samples (comma-separated hex text). |

`Fech = 60e6` (60 MS/s) is stored on the instance. `verbose`/`logging` options
echo or append the raw serial exchange to a log file.

### `pic0rick/ndt_acquisition.py` — capture + analysis + storage
The workhorse. Importing `pic0rick.device` is **lazy** (via `get_probe()`), so
this module is fully usable for offline analysis on a machine with no hardware.

- **`UltrasonicAcquisition`** — a dataclass bundling one trace with its metadata
  (`Fech`, `pon/poff/damp`, `gain`, `target`, transducer `piezo_*`). Key members:
  - `from_probe(...)` — trigger the board (`dac` → `pulse_adc_trigger` → `read`),
    decode the hex samples to a normalized `[-1, 1]` signal (`(code-512)/512`),
    and optionally cache to HDF5 keyed on `(gain, pon, poff, damp, target,
    piezo_id)` (a cache hit skips the hardware).
  - `signal_filtered` — zero-phase Butterworth band-pass around the probe's
    centre frequency/bandwidth.
  - `detect_echoes(...)` — rectify + smooth, find the main echo in a time
    window, then track periodic back-wall echoes at `Δt = 2·thickness/c`;
    returns peak times/amplitudes, a measured `Δt`, a back-computed thickness,
    and a labelled figure.
  - `calibrate(...)` — sweep `pon = poff`, save every shot, and report the pulse
    width that maximises the windowed echo amplitude (with a 3-panel figure).
  - `amplitude(...)`, `plot(...)`, `label(...)`, `info(h5_path)`.
- **HDF5 I/O** — `save_h5`, `load_h5`, and per-acquisition `save()`; each trace
  is a group with the signal dataset (gzip) plus scalar attributes, deduplicated
  by the same key.

## Notebooks

### `example_acquisition.ipynb` — live hardware
End-to-end workflow against a connected board and a 1018-steel 5-step
calibration block with a 5 MHz probe: gain sweeps, `calibrate()` for the best
`pon/poff`, then `from_probe()` + `detect_echoes()` at several block thicknesses
(10/15/25 mm) to recover thickness from the echo train, and `info()` to
summarize the stored HDF5.

### `example_process.ipynb` — offline analysis
No hardware: `load_h5(...)` a saved capture, extract the emitted pulse, and study
it (autocorrelation / side-lobe structure) with NumPy/SciPy. Good starting point
for signal-processing work on existing data.

## Dependencies

`pyserial`, `numpy`, `scipy`, `h5py`, `matplotlib`. (No `requirements.txt` here
yet — install those into your environment.)

## Example data

The notebooks read `../docs/data/calibration_block_5steps_1018steel.h5`.

## Quick start (offline)

```python
from pic0rick.ndt_acquisition import load_h5
acqs = load_h5("../docs/data/calibration_block_5steps_1018steel.h5")
acqs[0].plot()          # raw (+ filtered if piezo_* set)
```

Live capture instead:

```python
from pic0rick.ndt_acquisition import UltrasonicAcquisition
a = UltrasonicAcquisition.from_probe(Fech=60e6, gain=350, pon=70, poff=70, damp=6000,
                                     piezo_central_freq=5e6, piezo_bandwidth=4e6)
a.detect_echoes(start_us=35, end_us=55, target_thickness=0.010, speed_of_sound=5950)
```
