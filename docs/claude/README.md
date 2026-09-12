# Claude handoff notes — pic0rick

This folder holds durable, cross-session notes for Claude Code instances working
in this repository. If you are a new instance, **read this first**, then the
topic files listed below. Keep these notes updated when you learn something a
future instance would waste time rediscovering.

- `firmware.md` — the repo-root `firmware/` folder: the original mainline
  `adc-pulse` Pico firmware (uses the MAX14866; RP2040 + RP2350 builds).
- `onboard-dsp-firmware.md` — the `experiments/onboard_dsp/` RP2350A envelope/
  A-law firmware experiment: layout, outputs, gotchas, and the work done so far.
- `python-host-tools.md` — the repo-root `python/` NDT stack (serial driver +
  echo/thickness analysis + HDF5) that drives the mainline firmware.
- `hardware-build.md` — `hardware/build.sh`: KiBot + kicad-cli production
  outputs (gerbers/CPL/BOM/STEP + 3D renders), build groups incl. the fast
  `fab-fast` path, and the venv/3D-model gotchas.

## Task tracking & commit logs

- `TODO.md` — open, actionable items (with owner tags). `DONE.md` — completed.
  Move items from one to the other as they finish.
- `commits/` — one log file per commit (`<YYYY-MM-DD>-<slug>.md`) recording what
  changed and why. **Add a log entry for every commit you make in this repo.**
- Any new remark/discovery that needs an action goes in `TODO.md`.

> These are working memories, not authoritative docs. Verify a claim against the
> code before relying on it — file/line references can drift.
