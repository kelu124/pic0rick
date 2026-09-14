# 2026-09-14 — example_simple + ndt_acquisition on the DSP CLI

Python library **v0.1.8**. Branch `feature/unify-firmware-dsp`.

## What
- Added DSP-build helpers to `Pic0rick`: `set_gain(n)` (`dac write`),
  `configure_pulse(neg,damp,pos,order)`, `arm_pulser()`, `disarm_pulser()`.
- Rewrote `python/example_simple.ipynb` to the new CLI: version → `set_gain` →
  `configure_pulse` + `arm_pulser` → `read_raw()` (binary frame) → plot →
  `disarm_pulser`.
- Migrated `ndt_acquisition.from_probe()` to the same path: `set_gain` +
  `configure_pulse(negative_ns=poff, damp_ns=damp, positive_ns=pon, pos-first)` +
  `arm_pulser` + `read_raw()`; `signal = (frame.samples() - 512) / 512`
  (replaces the old stdio `dac`/`pulse_adc_trigger`/`read` hex-text path).

## Why
User: rewrite example_simple with the new CLI commands, and make
ndt_acquisition match. Both now target the DSP (`-DDSP`) build the board runs.

## Verified on hardware (RP2350A, DSP v0.1.11, mux)
- `ndt.from_probe(Fech=60e6, pon=200, poff=200, damp=2000, gain=50)` → float32
  (8000,), normalized [-1,1], echoes present (`var[2000:3000]` 0.014 vs
  `[6000:7000]` 0.0000).
- `example_simple.ipynb` executed via nbconvert (exit 0): version v0.1.11,
  `OK pulser armed`, `OK capture started type=raw samples=8000`, plot rendered.

## Note
The stdio methods (`dac`/`pulse_adc_trigger`/`read`) remain for the non-DSP
build; `from_probe` and example_simple now use the DSP build.
