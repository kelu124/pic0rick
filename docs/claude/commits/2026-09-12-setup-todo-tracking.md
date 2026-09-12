# 2026-09-12 — Set up TODO/DONE tracking, retire QUESTIONS.md

## What
- Added `docs/claude/TODO.md` (open actions) and `docs/claude/DONE.md`
  (completed actions).
- Added `docs/claude/commits/` with a `README.md` describing the per-commit
  log convention (this folder).
- Transformed the 14 items in the repo-root `QUESTIONS.md` into actionable,
  tagged TODO entries ([claude]/[maintainer]/[mixed]) and **deleted**
  `QUESTIONS.md`.
- Noted the convention in `docs/claude/README.md`.

## Why
The user asked for a durable, cross-session task list and an audit trail of
commits. The old `QUESTIONS.md` was a flat list of open questions; folding it
into `TODO.md` makes each item an owned, checkable action.

## Notes / discoveries
- `QUESTIONS.md` Q1 referenced a repo-root `TODO.md` describing a DMA-timeout
  `sleep_us(1)` fix. **No such file exists** in the repo — `docs/claude/TODO.md`
  now carries the only surviving record of that proposed fix.
