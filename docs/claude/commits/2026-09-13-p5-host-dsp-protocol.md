# 2026-09-13 — P5: DSP-build host protocol in python

Python library **v0.1.2**. Branch `feature/unify-firmware-dsp`.

## What
- Added `python/pic0rick/dsp.py`, ported from
  `experiments/onboard_dsp/tools/pic0rick_capture.py`:
  - `FrameReader` (skips text/noise, parses the 64-byte header, validates CRC32),
    `Frame`/`FrameHeader`, `alaw_decode`, `parse_status`.
  - **Length-agnostic:** payload size is derived from the header `sample_count`
    (`expected_payload_bytes`), so `read_raw` (8000) and `read_fft` (4096) both
    parse on one firmware.
- Added `Pic0rick` methods (DSP firmware): `status()` (parsed dict),
  `capture(payload)`, `read_fft()`, `read_raw()`.

## Version handshake
Dropped the onboard tool's hard `EXPECTED_FIRMWARE == "1.6"` reject. The host now
just parses `firmware=A.B.C` from the status line (matches our version.yaml
scheme); callers can compare if they wish.

## Verified on hardware (RP2350A, DSP build)
Driven via the library:
- `status()` → firmware 0.1.8, dsp_backend f32-rfft-hilbert, samples 4096.
- `read_fft()` → envelope frame, sample_count 4096, payload 16384 B, CRC OK,
  float32[4096], peak 193.
- `read_raw()` → raw frame, sample_count 8000, uint16[8000], sane values.
Offline: parse_status + expected_payload_bytes unit-checked.

## Note
The CLI/app parts of the old tool (argparse `run`, `save_frames`,
`validate_selftest`) were not ported — only the reusable library. They can be
re-added later if a standalone capture CLI is wanted. The old tool stays until
P7 removes `experiments/onboard_dsp/`.
