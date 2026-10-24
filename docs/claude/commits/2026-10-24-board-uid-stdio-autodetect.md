# 2026-10-24 — Board unique ID + stdio/DSP auto-detect (fw 0.1.12, py 0.1.9)

## What
- firmware 0.1.12: `version` reports `uid:` (Pico unique board ID, links
  `pico_unique_id`); stdio build prints a connect banner
  `pic0rick firmware vX uid: ...` once when the host opens the port.
  Rebuilt `firmware/rp2040.uf2` / `rp2350.uf2`.
- python 0.1.9: `Pic0rick` parses the UID (banner or `version`), auto-detects
  stdio vs DSP firmware (`status` probe), and `set_gain` / `configure_pulse` /
  `arm_pulser` / `read_raw` pick the right CLI — `example_simple.ipynb` runs on
  the default stdio UF2 again.
- Refreshed `docs/data/dsp_captures/*` and notebook outputs from the board.
- `docs/claude/hardware-testing.md`: RP2350B board runs the `pico2` build.
- KiCad `.kicad_prl` local-settings churn included (user asked to commit all).

## Follow-up (same day)
- Merged `substack-articles` into main; pruned merged branches
  (`substack-articles`, `feature/unify-firmware-dsp`, `development`, remote
  `questions`). Kept `VGA` and `wifi` (unmerged work).
- Updated `docs/claude/python-host-tools.md` and `firmware.md` for the UID and
  stdio/DSP auto-detect.
