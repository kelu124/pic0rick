# pic0rick

Open-hardware ultrasound platform (Raspberry Pi Pico based). Hardware under
`hardware/`, firmware experiments under `experiments/`, docs under `docs/`.

## Claude memory — read this first

Durable, cross-session notes for Claude Code live in **`docs/claude/`**.
Start with `docs/claude/README.md`, then the topic file for whatever you're
working on. When you learn something a future instance would waste time
rediscovering, add or update a note there (keep it short, verify against code).

Current topic notes:
- `docs/claude/firmware.md` — the repo-root `firmware/` folder: the original
  mainline `adc-pulse` Pico firmware (MAX14866 mux, RP2040 + RP2350 builds).
- `docs/claude/onboard-dsp-firmware.md` — the RP2350A onboard-DSP firmware
  (`experiments/onboard_dsp/`): layout, outputs, build/version gotchas.
  Its output formats are fully documented in
  `experiments/onboard_dsp/understanding_figures.md`.
- `docs/claude/python-host-tools.md` — the repo-root `python/` NDT host stack
  (serial driver + echo/thickness analysis + HDF5). See `python/Readme.md`.
