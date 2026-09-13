# Claude handoff notes — pic0rick

This folder holds durable, cross-session notes for Claude Code instances working
in this repository. If you are a new instance, **read this first**, then the
topic files listed below. Keep these notes updated when you learn something a
future instance would waste time rediscovering.

- `firmware.md` — the repo-root `firmware/` folder: the original mainline
  `adc-pulse` Pico firmware (uses the MAX14866; RP2040 + RP2350 builds).
- `onboard-dsp-firmware.md` — **retired**: the old `experiments/onboard_dsp/`
  firmware is now the mainline `-DDSP` build (`firmware/dsp` + `firmware/hw`);
  this note is now a pointer to where everything went.
- `dsp_output_formats.md` (in `docs/`, not here) — the DSP binary frame / status
  format reference (moved from onboard_dsp's `understanding_figures.md`).
- `python-host-tools.md` — the repo-root `python/` NDT stack (serial driver +
  echo/thickness analysis + HDF5) that drives the mainline firmware.
- `hardware-build.md` — `hardware/build.sh`: KiBot + kicad-cli production
  outputs (gerbers/CPL/BOM/STEP + 3D renders), build groups incl. the fast
  `fab-fast` path, and the venv/3D-model gotchas.
- `hardware-testing.md` — flashing a connected board (UF2 → BOOTSEL mass storage)
  and talking to the REPL over `/dev/ttyACM0`; the `reboot-dfu` iterate loop.

## Task tracking & commit logs

- `TODO.md` — open, actionable items (with owner tags). `DONE.md` — completed.
  Move items from one to the other as they finish.
- `commits/` — one log file per commit (`<YYYY-MM-DD>-<slug>.md`) recording what
  changed and why. **Add a log entry for every commit you make in this repo.**
- Any new remark/discovery that needs an action goes in `TODO.md`.

## Versioning rule

Firmware and the Python library each carry `A.B.C` in their own `version.yaml`
(`firmware/version.yaml`, `python/pic0rick/version.yaml`).
- **Claude bumps only the patch (C)** — firmware patch on changes to `firmware/`
  files, python patch on changes to `python/` files (independent). A/B 0-99,
  C 0-999. **The maintainer owns major/minor — never change them.**
- Always update that file's one-line, double-quoted `changes:` summary; the CI
  release body and the firmware `version` command read it.

> These are working memories, not authoritative docs. Verify a claim against the
> code before relying on it — file/line references can drift.
