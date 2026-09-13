# 2026-09-13 — P7: retire experiments/onboard_dsp

Python library **v0.1.3** (docs/reference edits only). Branch
`feature/unify-firmware-dsp`.

## What
- **Removed `experiments/` entirely** (50 tracked files: the onboard_dsp
  firmware, tools, notebook, captures, and the empty top-level Readme).
- **Kept** the durable format reference: `understanding_figures.md` →
  `docs/dsp_output_formats.md` (updated its parsing-helpers section to point at
  `pic0rick.dsp` + the device methods; added a "spec now describes the -DDSP
  build" note).
- **Dropped** `test_guide.md`, `DSP_Tests.ipynb`, `tools/pic0rick_capture.py`,
  `tools/requirements.txt`: all tightly coupled to the removed capture CLI and
  superseded by the `pic0rick.dsp` library. A fresh test guide + example notebook
  are logged in `docs/claude/TODO.md`.
- **Reference fixes:** `CLAUDE.md` (topic list + tree), `docs/claude/README.md`,
  `docs/claude/firmware.md` (unified-tree intro + build flavours section),
  `docs/claude/python-host-tools.md`, `python/Readme.md`, `python/pic0rick/dsp.py`
  docstring. Rewrote `docs/claude/onboard-dsp-firmware.md` as a "retired / where
  it went" pointer.

## Verification
`check-links --no-external`: 5 flagged local refs are all false positives (regex
strings in `.py`, and vendored CMSIS-DSP under `firmware/.deps/`). No genuine
broken references from the removal.

## Result
"One firmware, not two": the DSP firmware now lives only in the mainline
`firmware/` tree (`-DDSP`), and there is a single host stack in `python/`.
