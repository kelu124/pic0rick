# Commit logs

One markdown file per commit (or logical change), named
`<YYYY-MM-DD>-<short-slug>.md`. Each entry records **what** changed and **why**,
so a future Claude instance (or human) can reconstruct intent without spelunking
the diff.

Convention (see `docs/claude/TODO.md` / `DONE.md` for the task list):
- Add a log entry for every commit Claude makes in this repo.
- Move the corresponding item from `TODO.md` to `DONE.md` when done.
