# pic0rick on Substack — hands-on articles

Draft articles for a Substack series on using the pic0rick: from soldering the
boards to real measurements. Each article is a Markdown file here, backed by a
Jupyter notebook in [`notebooks/`](notebooks/) that produces every figure in
[`images/`](images/). The editorial roadmap is in [`plan.md`](plan.md).

| # | Article | Summary | Notebook | Status |
|---|---|---|---|---|
| 1 | [Meet the pic0rick](01-meet-the-pic0rick.md) | The three boards (main / pulser / HV) and the signal chain, assembly photos, flashing the firmware, first contact from Python (`version()`, `status()`, `read_raw()`) and three sanity checks for a freshly built board. | [01_first_contact](notebooks/01_first_contact.ipynb) | draft |
| 2 | [Your first A-scan](02-your-first-a-scan.md) | A 5 MHz probe on 10 mm steel: anatomy of the trace (main bang, echo groups, back-wall train), spectrum (peak at 5.05 MHz), band-pass + Hilbert envelope, and the same envelope computed on the RP2350 (`read_fft`). | [02_first_ascan](notebooks/02_first_ascan.ipynb) | draft |
| 3 | [Turning the knobs](03-turning-the-knobs.md) | Gain sweep 0→500 (clipping vs. noise floor, pick gain for the echo you need) and pulse-width sweep 25→155 ns (best 95 ns ≈ half a 5 MHz period) with `calibrate()`. | [03_gain_and_pulse](notebooks/03_gain_and_pulse.ipynb) | draft |
| 4 | [Measuring steel with sound](04-measuring-steel-with-sound.md) | Thickness from echo spacing (d = cΔt/2) on a 5-step block: 10.00 mm and 15.09 mm spot on, 25 mm consistently 24.3 mm, and a 20 mm shot fooled by the probe straddling a step — why you look at the A-scan. | [04_thickness_gauge](notebooks/04_thickness_gauge.ipynb) | draft |

## Versions

All four articles and notebooks are written against **pic0rick python library
v0.1.8** and **firmware v0.1.11** (`-DDSP` RP2350 build). Every notebook prints
`pic0rick.__version__` and warns if it differs from the version it was written
for. The version is also in each article's front matter.

## Running the notebooks

They run **offline by default** on recorded data
(`docs/data/calibration_block_5steps_1018steel.h5`, `docs/data/dsp_captures/`).
Set `LIVE = True` in the setup cell to drive a connected board instead.

```bash
# from the repo root, with the project venv (numpy scipy h5py matplotlib pyserial nbconvert)
.venv/bin/python docs/substack/notebooks/_build_notebooks.py      # regenerate the .ipynb from source
cd docs/substack/notebooks
../../../.venv/bin/jupyter nbconvert --to notebook --execute --inplace 0*.ipynb   # re-run, rewrite ../images/
```

The notebooks are generated from `_build_notebooks.py` — edit that file, not
the `.ipynb`, then rebuild.

## Links: un0rick.cc + GitHub

Readers coming from Substack are sent to **[un0rick.cc/pic0rick](https://un0rick.cc/pic0rick)**
for reference documentation and to **[GitHub](https://github.com/kelu124/pic0rick)** for
images, code, notebooks and data. Articles therefore use **absolute URLs only**:
figures are embedded from `raw.githubusercontent.com/kelu124/pic0rick/main/...`,
notebooks link to `github.com/kelu124/pic0rick/blob/main/...`, and each article
ends with a **"Go further"** block. The verified link map lives in the project
skill `.claude/skills/un0rick-links/SKILL.md`.

## Publishing to Substack

Push to `main` first — the figure and notebook URLs resolve only once the files
are on GitHub. Then paste the article body (without the front matter) into the
Substack editor; images embedded from GitHub raw URLs come across, otherwise
drag the PNGs from `images/`. Code blocks paste fine.
