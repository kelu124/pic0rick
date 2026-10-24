# Substack articles — working notes

Scope: **`docs/substack/`** — hands-on Substack series about using the pic0rick.
Project skill: **`.claude/skills/pic0rick-substack-article/SKILL.md`** (the
workflow — follow it). Created 2026-10-10.

- Links: project skill **`.claude/skills/un0rick-links/`** (verified
  un0rick.cc page/anchor map + GitHub URL patterns). Articles use absolute URLs
  only (raw GitHub for images, blob/main for code) + a "Go further" footer.
- `README.md` = index with summaries + run instructions; `plan.md` = editorial
  plan (arcs 1–3, "to verify before publishing" list).
- Articles 01–04 (arc 1: assemble → A-scan → gain/pulse → thickness) drafted
  against **python lib v0.1.8 / firmware v0.1.11**.
- Notebooks are **generated** from `notebooks/_build_notebooks.py` (nbformat),
  then executed with nbconvert in the repo `.venv` (system python lacks scipy).
  Executing writes `images/NN-*.png`. Edit the builder, never the `.ipynb`.
- Notebooks run **offline** (`LIVE = False`) on
  `docs/data/calibration_block_5steps_1018steel.h5` and `docs/data/dsp_captures/`.

## Data facts worth knowing (calibration HDF5, 28 shots, "Pink 5MHz" probe)
- idx 0–6: gain sweep 0..500 @ pon=poff=70 on 10 mm; 7–20: pulse sweep 25..155 ns
  @ gain 40 ("calibration" target), best 95 ns; 21–27: pon=95 on 10/15/20/25 mm.
- Big echo groups repeat every ~37 µs (10 mm shot); the back-wall train rides in
  each group. Thickness: 10 mm → 9.999, 15 → 15.09, 25 → 24.27 (consistent
  8.15 µs spacing — caliper TBD), 20 mm shot shows a 5 µs (15 mm) period → probe
  straddled the step (detector reports 22.6).
- `dsp_captures/envelope` is a different (gain 50) shot than the HDF5 traces.
- Pulser parts per schematics: **MD1213K6 + TC6320TG** (README intro says MD1210 — stale).
