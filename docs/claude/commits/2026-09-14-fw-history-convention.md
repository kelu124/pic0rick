# 2026-09-14 — docs/fw_history convention

Docs only. Branch `feature/unify-firmware-dsp`.

## What
- Added `docs/fw_history/README.md` documenting the reference-baseline archive:
  naming (`fw<A.B.C>-<board>-<options>.uf2`), purpose (regression baselines), the
  current `fw0.1.1-RP2350-muxenabled.uf2` entry, and links to
  `python/example_simple.ipynb`.
- **Rule:** only the maintainer adds `.uf2` to `docs/fw_history/`. Claude must
  never add/copy/build/commit a `.uf2` there (may edit the README and flash the
  baselines for diagnostics). Recorded in `docs/claude/README.md` and memory
  (`fw-history-maintainer-only`).

## Note
The baseline `.uf2` and `python/example_simple.ipynb` themselves were committed
by the maintainer (commit "aing luc's tests"), not here.
