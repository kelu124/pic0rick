#!/usr/bin/env bash
#
# build.sh — generate JLCPCB-friendly production outputs from a KiCad board
# with KiBot. Artefacts go to <board>/build_<vendor>/, in per-type subfolders
# (<vendor> is the config basename, so jlcpcb.kibot.yaml -> build_jlcpcb).
#
# Usage:
#   ./build.sh [board_dir|all] [group]
#
#   board_dir   Directory holding the .kicad_pcb/.kicad_sch (default: adc).
#               "all" builds every board in the BOARDS registry below.
#   group       What to build (default: all). The 3D deliverables (STEP + top &
#               bottom PNG renders) are produced for every group EXCEPT fab-fast:
#                 all         everything below
#                 fab         gerbers, drill, zipped gerbers (+ 3D)   (no schematic needed)
#                 fab-fast    gerbers/drill/zip ONLY — no STEP, no PNG renders (fast)
#                 production  fab + CPL + BOM (CSV) (+ 3D)   [needs a schematic]
#                 docs        schematic PDF, interactive HTML BOM (+ 3D)  [needs a schematic]
#                 models      STEP + top & bottom 3D renders only
#
# Boards without a schematic (e.g. pulser_panel, a panel) automatically fall back
# to fabrication + 3D only; their assembly CPL/BOM come from the single board / KiKit.
#
# Examples:
#   ./build.sh                 # adc, everything
#   ./build.sh all             # every board, everything it supports
#   ./build.sh mux production  # mux, JLCPCB upload files (+ 3D)
#   ./build.sh mux fab-fast    # mux, gerbers only — fast, no 3D renders
#   ./build.sh pulser_panel    # panel: gerber zip + STEP + renders
#
# The KiCad python module (pcbnew) is compiled for the system Python 3.10,
# so KiBot runs from a dedicated Python 3.10 venv with --system-site-packages.
# This script auto-creates hardware/.venv-kibot on first run.
set -euo pipefail

HARDWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Vendor config. Override to target another fab: CONFIG=hardware/pcbway.kibot.yaml ./build.sh
CONFIG="${CONFIG:-${HARDWARE_DIR}/jlcpcb.kibot.yaml}"
VENV="${HARDWARE_DIR}/.venv-kibot"

BOARD_DIR="${1:-adc}"
GROUP="${2:-all}"

# Registry of boards in this repo, for the "all" target. Each has a .kicad_pcb
# under hardware/<name>/; pulser_panel is a schematic-less panel (fab + 3D only).
BOARDS=(adc mux psram vga pulser_panel panel_adc_pulser_hv)

# "./build.sh all [group]" regenerates every registered board's outputs so the
# whole repo stays consistent. Recurses into this same script once per board.
if [[ "${BOARD_DIR}" == "all" ]]; then
    for b in "${BOARDS[@]}"; do
        echo "==================== ${b} ===================="
        CONFIG="${CONFIG:-}" "${BASH_SOURCE[0]}" "${b}" "${GROUP}" \
            || echo ">> warning: build for '${b}' reported issues" >&2
        echo
    done
    exit 0
fi

# Resolve board directory (accept "adc" or "hardware/adc" or absolute path).
if [[ -d "${HARDWARE_DIR}/${BOARD_DIR}" ]]; then
    BOARD_DIR="${HARDWARE_DIR}/${BOARD_DIR}"
elif [[ ! -d "${BOARD_DIR}" ]]; then
    echo "error: board directory not found: ${BOARD_DIR}" >&2
    exit 1
fi
BOARD_DIR="$(cd "${BOARD_DIR}" && pwd)"

# Find the board files (ignore KiBot's own kibot_* temp copies).
PCB="$(find "${BOARD_DIR}" -maxdepth 1 -name '*.kicad_pcb' ! -name 'kibot_*' | head -n1)"
SCH="$(find "${BOARD_DIR}" -maxdepth 1 -name '*.kicad_sch' ! -name 'kibot_*' | head -n1)"
if [[ -z "${PCB}" ]]; then
    echo "error: no .kicad_pcb found in ${BOARD_DIR}" >&2
    exit 1
fi

# Output goes to <board>/build_<vendor>/, where <vendor> is the config's basename
# (jlcpcb.kibot.yaml -> build_jlcpcb). Point CONFIG at another vendor's *.kibot.yaml
# to get a parallel folder (e.g. build_pcbway) without clobbering this one.
VENDOR="$(basename "${CONFIG}" .kibot.yaml)"
OUT_DIR="${BOARD_DIR}/build_${VENDOR}"

# Keep the build side-effect-free on the source tree:
#  1. KiBot copies the board to kibot_<rand>.kicad_* to apply filters/transforms
#     and doesn't always clean those up.
#  2. KiCad 10's kicad-cli silently MIGRATES an older-format .kicad_pro/.kicad_sch
#     to the new format (and re-serialises it) when it merely opens the file —
#     which would rewrite the user's committed sources with a huge diff.
# So: remove kibot_* strays, snapshot the real design files, and on exit restore
# the snapshot (preserving any uncommitted user edits) + drop kibot_* strays.
rm -f "${BOARD_DIR}"/kibot_*.kicad_* 2>/dev/null || true

SRC_BACKUP="$(mktemp -d)"
shopt -s nullglob
for f in "${BOARD_DIR}"/*.kicad_pcb "${BOARD_DIR}"/*.kicad_sch \
         "${BOARD_DIR}"/*.kicad_pro "${BOARD_DIR}"/*.kicad_prl; do
    [[ "$(basename "$f")" == kibot_* ]] && continue
    cp -p "$f" "${SRC_BACKUP}/"
done
shopt -u nullglob

restore_sources_and_clean() {
    local b
    for b in "${SRC_BACKUP}"/*; do
        [[ -e "$b" ]] || continue
        cp -p "$b" "${BOARD_DIR}/$(basename "$b")"
    done
    rm -rf "${SRC_BACKUP}"
    rm -f "${BOARD_DIR}"/kibot_*.kicad_* 2>/dev/null || true
}
trap restore_sources_and_clean EXIT

# --- Ensure a working KiBot venv (Python 3.10 + KiCad's pcbnew/wx) ---------
# See requirements-kibot.txt for why this venv is separate from the repo .venv.
if [[ ! -x "${VENV}/bin/kibot" ]]; then
    echo ">> Creating KiBot venv at ${VENV} (Python 3.10, system site-packages)..."
    /usr/bin/python3 -m venv --system-site-packages "${VENV}"
    "${VENV}/bin/pip" install --quiet --upgrade pip
    # KiBot's macro system breaks when byte-compiled, so install with --no-compile.
    "${VENV}/bin/pip" install --quiet --no-compile -r "${HARDWARE_DIR}/requirements-kibot.txt"
fi
KIBOT="${VENV}/bin/kibot"

# Verify pcbnew is importable (guards against a broken interpreter).
if ! "${VENV}/bin/python" -c 'import pcbnew' 2>/dev/null; then
    echo "error: KiBot venv cannot import pcbnew — is KiCad installed system-wide?" >&2
    exit 1
fi

# KiBot shells out to KiAuto helpers (kicad2step_do, etc.). Put this venv's bin
# first so those resolve here (and can import pcbnew) instead of any other
# activated venv on PATH.
export PATH="${VENV}/bin:${PATH}"

# 3D model library. The board references models via ${KICAD6_3DMODEL_DIR} /
# ${KICAD_3DMODELS_DIR}; KiCad 10 no longer defines those, so point them at the
# system library (from the kicad-packages3d apt package) unless overridden.
# Used by both KiBot's STEP export and the kicad-cli PNG renders below.
: "${KICAD_3DMODELS_DIR:=/usr/share/kicad/3dmodels}"
export KICAD_3DMODELS_DIR
export KICAD6_3DMODEL_DIR="${KICAD_3DMODELS_DIR}"

# --- 3D PNG renders (kicad-cli) --------------------------------------------
# KiBot's render_3d refuses the kicad-cli backend on KiCad < 10.0.5 and falls
# back to a headless-GUI path that fails on KiCad 10, so we render the top and
# bottom views with kicad-cli directly. Writes build/3D/<board>-3D-{top,bottom}.png.
render_pngs() {
    local board_name out3d
    board_name="$(basename "${PCB}" .kicad_pcb)"
    out3d="${OUT_DIR}/3D"
    mkdir -p "${out3d}"

    local side
    for side in top bottom; do
        echo ">> Rendering 3D ${side} view (kicad-cli)..."
        kicad-cli pcb render \
            --side "${side}" \
            --quality high \
            --width 1600 --height 1200 \
            --floor --perspective \
            -D "KICAD6_3DMODEL_DIR=${KICAD_3DMODELS_DIR}" \
            -o "${out3d}/${board_name}-3D-${side}.png" \
            "${PCB}" 2> >(grep -v -e 'property.h.*assert' -e '^Rendering:' >&2 || true) \
            || echo "warning: ${side} render failed" >&2
    done
}

# --- Run --------------------------------------------------------------------
# -D/--dont-stop: keep going if one output fails. The STEP export returns a
# non-zero code because a few component 3D models are unavailable on this machine
# (SamacSys ICs stored under Windows-only paths, and the VRML-only Pico model);
# kicad-cli still writes a valid STEP of the board + all resolvable parts.
KIBOT_ARGS=(-D -c "${CONFIG}" -b "${PCB}" -d "${OUT_DIR}")
[[ -n "${SCH}" ]] && KIBOT_ARGS+=(-e "${SCH}")

# Pick which outputs/groups KiBot runs. An empty target list means "all outputs".
# Boards without a schematic (e.g. panels) can only do fabrication (+ STEP);
# their assembly CPL/BOM come from the single board / KiKit, not from here.
KIBOT_TARGETS=()
RUN_KIBOT=1
# 3D deliverables (STEP + top/bottom PNG renders) are produced for EVERY group by
# default, so the "step" KiBot target is folded into fab/production/docs below and
# render_pngs runs unless WITH_3D is cleared. The "fab-fast" group is the fast
# path: gerbers only, no STEP and no ~35 s of PNG rendering.
WITH_3D=1
if [[ -z "${SCH}" ]]; then
    echo ">> No schematic in $(basename "${BOARD_DIR}") — fabrication + 3D only (CPL/BOM/schematic skipped)." >&2
    case "${GROUP}" in
        all)                 KIBOT_TARGETS=(fab step);;
        production|fab|docs) KIBOT_TARGETS=(fab step);;
        fab-fast)            KIBOT_TARGETS=(fab); WITH_3D=0;;   # gerbers only, no 3D
        models)              KIBOT_TARGETS=(step);;
        *) echo "error: unknown group '${GROUP}' (use fab|fab-fast|production|docs|models|all)" >&2; exit 1;;
    esac
else
    case "${GROUP}" in
        all) ;;                                        # empty targets => every output
        models) KIBOT_TARGETS=(step);;
        fab|production|docs) KIBOT_TARGETS=("${GROUP}" step);;   # always fold in the 3D STEP
        fab-fast) KIBOT_TARGETS=(fab); WITH_3D=0;;               # gerbers only, no 3D
        *) echo "error: unknown group '${GROUP}' (use fab|fab-fast|production|docs|models|all)" >&2; exit 1;;
    esac
fi

echo ">> Board : $(basename "${PCB}")"
echo ">> Group : ${GROUP}"
echo ">> Output: ${OUT_DIR}"
echo

# KiCad 10 prints harmless property assertions to stderr; keep them out of view.
rc=0
if [[ ${RUN_KIBOT} -eq 1 ]]; then
    "${KIBOT}" "${KIBOT_ARGS[@]}" "${KIBOT_TARGETS[@]}" \
        2> >(grep -v 'property.h.*assert' >&2 || true) || rc=$?
fi

echo
if [[ ${rc} -ne 0 ]]; then
    echo ">> Completed with warnings (KiBot exit ${rc}); some 3D models may be absent from the STEP." >&2
fi

# Top/bottom 3D PNG renders are produced for every group except the fast path.
if [[ ${WITH_3D} -eq 1 ]]; then
    echo
    render_pngs
fi

echo
echo ">> Done. Artefacts in ${OUT_DIR}"
find "${OUT_DIR}" -maxdepth 2 -type f ! -name 'kibot_*' | sort | sed "s#${OUT_DIR}/#   #"
