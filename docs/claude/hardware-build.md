# Hardware production outputs — `hardware/build.sh`

Generates JLCPCB-friendly production artefacts from the KiCad boards under
`hardware/<board>/` using **KiBot** + **kicad-cli**. Output lands in
`hardware/<board>/build_<vendor>/` (vendor = config basename, so the default
`jlcpcb.kibot.yaml` → `build_jlcpcb/`), split into `gerbers/`, `production/`,
`assembly/`, and `3D/` subfolders.

## Usage

```bash
./build.sh [board_dir|all] [group]
```

- `board_dir`: `adc` (default), `mux`, `psram`, `vga`, `pulser_panel`, or `all`
  (every board in the `BOARDS` registry). Accepts `hardware/mux` or an abs path too.
- `group` (default `all`) — what to build. **3D deliverables (STEP + top &
  bottom PNG renders) are produced for every group EXCEPT `fab-fast`:**
  - `all` — everything
  - `fab` — gerbers + drill + zip (+ 3D); no schematic needed
  - `fab-fast` — **gerbers/drill/zip ONLY, no STEP, no PNG renders** (fast path)
  - `production` — fab + CPL + BOM CSV (+ 3D); needs a schematic
  - `docs` — schematic PDF + interactive HTML BOM (+ 3D); needs a schematic
  - `models` — STEP + top/bottom PNG renders only

Schematic-less boards (e.g. `pulser_panel`) auto-fall-back to fabrication + 3D.

## How the two engines split the work

- **KiBot** (`jlcpcb.kibot.yaml`) produces gerbers/drill/zip, CPL, BOM, schematic
  PDF, iBOM, and the **STEP** (output `step`, type `export_3d`). Groups
  `fab`/`production`/`docs`/`models` map to KiBot groups of the same name.
- **kicad-cli** does the **top/bottom PNG renders** directly (`render_pngs` in
  build.sh). KiBot's `render_3d` is bypassed because it refuses the kicad-cli
  backend on KiCad < 10.0.5 and its headless-GUI fallback fails on KiCad 10.

## The 3D-always default + fast path (current behaviour)

Originally 3D only ran for `all`/`models`, so `build.sh mux production` produced
no `3D/` folder — that's why the mux was missing its top/bottom views. Now:

- build.sh folds the `step` KiBot target into `fab`/`production`/`docs`, and
  `render_pngs` runs unconditionally — **gated by `WITH_3D` (default 1)**.
- `fab-fast` sets `WITH_3D=0` and targets `fab` only → gerbers + zip in ~2 s,
  vs ~35 s when the two PNG renders run (~17 s each at 1600×1200, quality high).

To skip 3D on any build, use `fab-fast`. To force only-3D, use `models`.

## Environment / gotchas

- KiBot runs from a dedicated **Python 3.10 venv** at `hardware/.venv-kibot`
  (`--system-site-packages` so it can import KiCad's `pcbnew`). Auto-created on
  first run from `requirements-kibot.txt` (installed `--no-compile`; KiBot's
  macros break when byte-compiled). Not the repo `.venv`.
- 3D model libs: build.sh points `KICAD_3DMODELS_DIR` / `KICAD6_3DMODEL_DIR` at
  `/usr/share/kicad/3dmodels` (KiCad 10 no longer defines them).
- STEP export returns non-zero because a few component models are unavailable on
  this machine (SamacSys ICs under Windows-only paths, the VRML-only Pico model).
  build.sh uses KiBot `-D` (don't-stop) and still writes a valid STEP of the
  board + resolvable parts; the non-zero rc is reported as a warning only.
- **Side-effect-free**: build.sh snapshots the `.kicad_*` sources before running
  and restores them on exit (KiCad 10's kicad-cli silently migrates/rewrites
  older-format files just by opening them), and cleans KiBot's `kibot_*` strays.
- `build_<vendor>/` folders are build artefacts; `.claude/` is now gitignored
  and untracked (unrelated repo hygiene).
