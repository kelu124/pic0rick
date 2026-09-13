# pic0rick

Open-hardware ultrasound platform (Raspberry Pi Pico based). Hardware under
`hardware/`, firmware under `firmware/`, host tools under `python/`, docs under
`docs/`.

## Claude memory — read this first

Durable, cross-session notes for Claude Code live in **`docs/claude/`**.
Start with `docs/claude/README.md`, then the topic file for whatever you're
working on. When you learn something a future instance would waste time
rediscovering, add or update a note there (keep it short, verify against code).

Current topic notes:
- `docs/claude/firmware.md` — the repo-root `firmware/` folder: the unified Pico
  firmware. One tree, build variants via `-DMUX` and `-DDSP` (RP2350). `hw/`
  holds the shared acquisition/pulser/DAC; `dsp/` the RP2350 DSP feature set.
- `docs/claude/onboard-dsp-firmware.md` — **retired**: the old
  `experiments/onboard_dsp/` firmware is now the `-DDSP` build; this note points
  to where its pieces went. DSP output formats: `docs/dsp_output_formats.md`.
- `docs/claude/python-host-tools.md` — the repo-root `python/` NDT host stack
  (serial driver, DSP frame protocol `pic0rick.dsp`, analysis + HDF5). See
  `python/Readme.md`.
- `docs/claude/hardware-build.md` — `hardware/build.sh` production-output flow
  (KiBot + kicad-cli): build groups incl. the fast `fab-fast` path, the
  3D-always default, and the KiBot venv / 3D-model gotchas.
- `docs/claude/hardware-testing.md` — flashing a connected board + the
  `reboot-dfu` iterate loop; `docs/claude/unify-firmware-dsp-plan.md` — the
  firmware-unification plan/status.
