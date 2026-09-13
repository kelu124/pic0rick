# 2026-09-13 — Python: parse the firmware `version` command

Python library **v0.1.1**.

## What
- Added `Pic0rick.version()` to `python/pic0rick/device.py`: sends `version\n`
  to the firmware and parses the multi-line reply into a dict
  (`version`, `changes`, `board`, `chip`, `mux`, `mux_enabled`, `build`,
  `release`, plus `raw`). Missing lines are simply absent from the dict.
- Added `import re`.

## Why
The firmware `version` command (fw v0.1.1) reports board/chip, mux, git hash and
release URL; the host library had no way to read it back. Lets a host script
confirm which firmware/board/mux build a connected device is running.

## Notes
- Tolerant parsing: matches on line prefixes (`changes:`/`board:`/`mux:`/
  `build:`/`release:`) and a `firmware vX.Y.Z` regex, so REPL echo (`run> `) and
  the prompt are ignored.
- `mux_enabled` is a convenience bool derived from the `mux:` line.

## Verification
Ran the parse logic against a simulated response (with `run> ` echo + prompt);
produced the expected dict. `device.py` passes `ast.parse`.
Not exercised against live hardware.
