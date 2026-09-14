# onboard_dsp firmware — RETIRED / merged into the mainline firmware

**As of 2026-09-13 (branch `feature/unify-firmware-dsp`), `experiments/onboard_dsp/`
no longer exists.** Its RP2350A DSP firmware was folded into the mainline
`firmware/` tree as an opt-in build, so there is one firmware source, not two.

## Where everything went

- **DSP firmware sources** → `firmware/dsp/` (`dsp.c`, `pipeline.c`,
  `protocol.c`, `usb_transport.c`, `usb_descriptors.c`, `tusb_config.h`) +
  `firmware/main_dsp.c` (the raw-TinyUSB command loop). Built with **`-DDSP=ON`**
  (RP2350/`pico2` only). See `firmware.md`.
- **Shared drivers** (used by both the stdio and DSP builds) → `firmware/hw/`
  (`acquisition.c/.pio`, `pulser.pio`, `dac.c` spi1, `u4rk.h`).
- **Host capture tool** → its reusable library is now `python/pic0rick/dsp.py`
  (frame reader + CRC + `parse_status` + `alaw_decode`), driven via
  `Pic0rick.status()/capture()/read_fft()/read_raw()`. See `python-host-tools.md`.
- **Output-format reference** (`understanding_figures.md`) →
  `docs/dsp_output_formats.md`.

## What changed vs the old onboard_dsp

- Two acquisition lengths: **`read_raw` = 8000** samples (raw), **`read_fft` =
  4096** (Hilbert envelope). The frame `sample_count` is now per-mode.
- Firmware version follows the repo `version.yaml` A.B.C scheme (no more the
  hard-coded `1.6` / `EXPECTED_FIRMWARE` handshake).
- Commands `version` and `reboot-dfu` added; `-DMUX` adds `write/set/clear mux`.

## Not carried over (see `docs/claude/TODO.md`)

- The old `test_guide.md` and `DSP_Tests.ipynb` (tightly coupled to the removed
  `tools/pic0rick_capture.py` CLI) were dropped. A fresh docs test-guide and a
  `python/` example notebook against `read_raw`/`read_fft` + `pic0rick.dsp` are
  TODO.
