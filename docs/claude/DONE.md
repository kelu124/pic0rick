# DONE — pic0rick

Completed actions, most recent first. Items are moved here from `TODO.md` when
finished. Each entry links to its commit log in `docs/claude/commits/`.

---

## 2026-09-12

- **Firmware↔Python command parity.** Audited the 6 firmware serial commands
  against `python/pic0rick`; added the 3 missing MUX methods (`write_mux`,
  `set_mux`, `clear_mux`) to `Pic0rick`. See
  `commits/2026-09-12-mux-command-parity.md`.
- **Set up task tracking.** Created `docs/claude/TODO.md`, this file, and the
  `docs/claude/commits/` log folder; transformed `QUESTIONS.md` into TODO actions
  and removed it. See `commits/2026-09-12-setup-todo-tracking.md`.
