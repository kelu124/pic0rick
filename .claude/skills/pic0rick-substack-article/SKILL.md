---
name: pic0rick-substack-article
description: Write or update a Substack article about using the pic0rick (assembly, acquisition, tuning, measurements, DSP, firmware...) in docs/substack/, with a companion Jupyter notebook that generates its figures and pins the pic0rick python library version. Use when asked for a pic0rick blog/Substack/newsletter post, tutorial article, or to extend the editorial plan. pic0rick repo only.
---

# pic0rick Substack article

Articles live in `docs/substack/`. Read `docs/substack/README.md` (index +
summaries) and `docs/substack/plan.md` (editorial plan) first; pick the next
item from the plan unless the user names a topic.

## Layout (keep it)

```
docs/substack/
├── README.md                 # table: # | article | summary | notebook | status  + versions + how to run
├── plan.md                   # arcs, ideas table, "to verify before publishing"
├── NN-slug.md                # the article
├── images/NN-*.png           # figures, written ONLY by the notebooks
└── notebooks/
    ├── _build_notebooks.py   # SOURCE of all notebooks (nbformat); edit this, not .ipynb
    └── NN_name.ipynb         # generated + executed (outputs kept)
```

## Workflow

1. **Ground in the code + data.** Read `python/Readme.md`, `docs/claude/python-host-tools.md`,
   and the library (`python/pic0rick/`). Use only API that exists. Note the current
   versions: `python/pic0rick/version.yaml` (library) and `firmware/version.yaml`.
2. **Notebook first.** Add an `NBnn` cell list in `notebooks/_build_notebooks.py`
   (reuse the `SETUP` cell: finds the repo, imports `pic0rick`, prints
   `pic0rick.__version__` vs `WRITTEN_FOR`, sets `FIG`, `DATA`, `LIVE = False`).
   - Must run **offline** (`LIVE = False`) on data in `docs/data/` (HDF5 via
     `load_h5`, DSP captures in `docs/data/dsp_captures/`); put the live-hardware
     path behind `if LIVE:`.
   - Every figure: `fig.savefig(FIG / "NN-name.png")`.
   - If you bump `PIC0RICK_PY`/`FIRMWARE` at the top, older notebooks' pins change
     too — only do that when re-validating them.
3. **Build + execute** from the repo root with the project venv:
   ```bash
   .venv/bin/python docs/substack/notebooks/_build_notebooks.py
   cd docs/substack/notebooks && ../../../.venv/bin/jupyter nbconvert --to notebook --execute --inplace NN_*.ipynb
   ```
4. **Look at every figure** (Read the PNG) and the printed numbers before writing.
   Write the analysis from what the data shows; if it contradicts the expectation,
   say so (see part 4's 20 mm / 25 mm discussion) and add a line to plan.md's
   "to verify" list. Never invent numbers, BOM details or hardware facts.
5. **Write `NN-slug.md`**: YAML front matter (`title, subtitle, series,
   pic0rick_python, firmware, notebook, status`), H1 + italic subtitle, short
   code excerpts from the notebook, a takeaways list, a "next time" hook.
   **Links: follow the `un0rick-links` skill** — absolute URLs only (they get
   pasted into Substack): figures as
   `https://raw.githubusercontent.com/kelu124/pic0rick/main/docs/substack/images/NN-*.png`,
   repo photos as `.../main/docs/images/...`, notebooks/code as
   `https://github.com/kelu124/pic0rick/blob/main/...`; link concepts to their
   un0rick.cc reference page on first mention; end with the **"## Go further"**
   block (un0rick.cc reference docs, then companion notebook + python/firmware/
   hardware/releases + repo, then the "Written against …" version line).
   Tone: first person (the maintainer), hands-on, ~1200–1800 words.
6. **Update** `README.md` (row + summary) and `plan.md` (status). Verify links
   with the check in the `un0rick-links` skill; new images/notebooks only resolve
   after they are pushed to `main` — tell the user.
7. Follow repo rules: commit log in `docs/claude/commits/`, TODO/DONE. Files under
   `docs/substack/` are docs — no python/firmware version bump unless you change
   `python/` or `firmware/` files.
