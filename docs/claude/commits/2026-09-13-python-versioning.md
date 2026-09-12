# 2026-09-13 — Python library versioning

Python library **v0.1.0**.

## What
- Added `python/pic0rick/version.yaml` (`version: "A.B.C"` + `changes`),
  mirroring the firmware scheme.
- Added `python/pic0rick/__init__.py` exposing `pic0rick.__version__`,
  `pic0rick.__changes__`, and `version()`. Parses the yaml with a regex (no
  PyYAML dependency) and does not import pyserial at package-import time.

## Why
User asked for an A.B.C scheme on the Python library like the firmware, with
Claude controlling only the patch number and the maintainer owning major/minor.

## Verification
`python3 -c "import pic0rick; print(pic0rick.__version__)"` → `0.1.0`.

## Rule added
Python patch (C) is bumped by Claude on every change to `python/` files,
independently of the firmware version. Recorded in `docs/claude/README.md` and
memory.
