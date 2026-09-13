# 2026-09-13 — version command build metadata + dual-version release notes

Firmware **v0.1.1**.

## What
- `version` CLI command now prints, in addition to version + changes:
  - `board` / `chip` — from `PICO_BOARD` (`pico`→RP2040, `pico2`→RP2350).
  - `mux` — enabled/disabled (already had this).
  - `build` — git short hash, `-dirty` when `firmware/` has uncommitted changes.
  - `release` — GitHub release URL derived from the git remote + `fw-v<version>`.
- Version headers:
  - `version.h.in` gains `FW_BOARD`, `FW_CHIP`, `FW_RELEASE_URL` (configure-time).
  - New `version_git.h.in` + `cmake/git_version.cmake` regenerate `FW_GIT_HASH`
    on **every build** (custom target `gen_git_version`, a dependency of
    `adc-pulse`, plus a configure-time run so the header always exists).
- CI (`.github/workflows/firmware.yml`): release body now composed from BOTH
  `firmware/version.yaml` and `python/pic0rick/version.yaml` (version + changes),
  written to `release_body.md` and passed via `body_path`. No separate python
  release stream (per maintainer decision).

## Design decisions (from the discussion)
- Git hash captured at build time (not just configure) so incremental local
  builds stay honest; dirty check scoped to `firmware/` so unrelated repo changes
  (e.g. hardware/, .vscode/) don't flag it.
- On a dirty build the `release` URL still shows the clean `fw-v<version>` link;
  the `-dirty` in the `build` line is the signal that the tree isn't that exact
  tag. (Maintainer chose this over marking the URL unpublished.)
- Python version is a snapshot inside the firmware release body; python-only
  changes still don't cut a release (intended — library is used from source).

## Verification
Built all 4 variants locally. Confirmed generated headers:
- rp2350-mux: BOARD=pico2, CHIP=RP2350, RELEASE_URL=.../fw-v0.1.1,
  GIT_HASH=b0d9435-dirty.
- rp2040-nomux: BOARD=pico, CHIP=RP2040, no `MUX=1` define.
