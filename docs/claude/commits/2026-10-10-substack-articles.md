# 2026-10-10 — Substack article series (arc 1) + article-writing skill

Docs only (no python/ or firmware/ changes → no version bump).

## What
- `docs/substack/`: four draft articles (01 meet the pic0rick, 02 first A-scan,
  03 gain & pulse width, 04 thickness gauge), `README.md` (index + summaries),
  `plan.md` (editorial plan, to-verify list).
- `docs/substack/notebooks/`: `_build_notebooks.py` generates 4 notebooks pinned
  to python lib v0.1.8 / fw v0.1.11; executed offline on `docs/data/`, figures in
  `docs/substack/images/`.
- Project skill `.claude/skills/pic0rick-substack-article/SKILL.md`;
  `.gitignore` changed from `.claude` to `.claude/*` + `!.claude/skills/` so
  skills are versioned while local settings stay ignored.
- `docs/claude/substack.md` note; CLAUDE.md + docs/claude/README.md indexes.

## Verification
All notebooks execute via nbconvert in `.venv`; all relative links in the
articles resolve; every figure was inspected and the numbers in the text come
from notebook output. Pulser part checked against schematics (MD1213K6).

## Follow-up (same day): un0rick.cc + GitHub linking
- New project skill `.claude/skills/un0rick-links/SKILL.md`: un0rick.cc page +
  anchor map (verified with curl against the live HTML ids) and GitHub URL
  patterns; policy = reference docs → un0rick.cc, images/code → GitHub.
- Articles 01–04: all links absolute (raw GitHub images, blob/main notebooks),
  inline un0rick.cc references, "Go further" footer. All 49 links checked:
  un0rick.cc pages/anchors 200, repo paths on origin/main except the new
  docs/substack images/notebooks (live after push).
