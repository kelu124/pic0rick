#!/usr/bin/env python3
"""Find broken *local* links in the pic0rick repo.

Scans Markdown (.md), Python (.py) and Jupyter notebook (.ipynb) files for
references to local files/images and reports any whose target does not exist
on disk. External URLs (http/https), mailto:, anchors, etc. are ignored on
purpose -- this checker is about links that break when files move or get
renamed inside the repo.

Usage:
    python3 docs/others/check_local_links.py [ROOT]

ROOT defaults to the repository root (two levels up from this script).
Exit code is 1 if any broken link is found, else 0 -- so it can be used in CI
or a pre-commit hook.

No third-party dependencies (standard library only).
"""
import os
import re
import sys
import json
from urllib.parse import unquote

# Directories never worth scanning: VCS, virtualenvs, caches, vendored builds.
EXCLUDE_DIRS = {".git", ".venv", "venv", "node_modules", "__pycache__", ".tox"}
# Path fragments that mark vendored / generated trees (e.g. CMSIS build deps).
EXCLUDE_SUBSTR = ("/_deps/", "/.venv-", "/site-packages/", "/build2350/", "/build2040/")

# Markdown/inline links and images, i.e. bracketed-text then paren-target,
# and the image form with a leading bang.
MD_LINK = re.compile(r'!?\[[^\]]*\]\(\s*<?([^)\s>]+)>?[^)]*\)')
# HTML-style references: an src or href attribute with a quoted value.
HTML_REF = re.compile(r'(?:src|href)\s*=\s*["\']([^"\']+)["\']', re.IGNORECASE)


def is_excluded(path):
    if set(path.split(os.sep)) & EXCLUDE_DIRS:
        return True
    p = path.replace(os.sep, "/")
    return any(s in p for s in EXCLUDE_SUBSTR)


def collect_files(root):
    files = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames
                       if d not in EXCLUDE_DIRS and not d.startswith(".venv-")]
        for name in filenames:
            if name.endswith((".md", ".py", ".ipynb")):
                full = os.path.join(dirpath, name)
                if not is_excluded(full):
                    files.append(full)
    return files


def get_text(path):
    """Return the searchable text of a file (notebook cells are concatenated)."""
    if path.endswith(".ipynb"):
        try:
            nb = json.load(open(path, encoding="utf-8", errors="replace"))
        except Exception:
            return ""
        return "\n".join("".join(cell.get("source", []))
                         for cell in nb.get("cells", []))
    try:
        return open(path, encoding="utf-8", errors="replace").read()
    except Exception:
        return ""


def is_external(link):
    return link.startswith((
        "http://", "https://", "ftp://", "mailto:", "tel:",
        "#", "data:", "javascript:",
    ))


def looks_like_code(target):
    """Skip regex / format / placeholder strings that resemble a path."""
    if any(c in target for c in ("{", "}", "$", "*", "<", ">", "|",
                                 "[", "]", "^", "\\", "`")):
        return True
    if "%s" in target or "%d" in target:
        return True
    if not any(c.isalnum() for c in target):  # e.g. "...", "--"
        return True
    return False


def resolve(root, source_file, link):
    """Return absolute candidate path for a local link, or None to skip it."""
    target = link.split("#", 1)[0].split("?", 1)[0].strip()
    if not target:
        return None  # pure anchor / query
    target = unquote(target)
    if looks_like_code(target):
        return None
    if target.startswith("/"):
        # Treat leading slash as repo-root-relative.
        return os.path.join(root, target.lstrip("/"))
    return os.path.normpath(os.path.join(os.path.dirname(source_file), target))


def main():
    root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else \
        os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

    files = collect_files(root)
    ok = 0
    broken = []  # (source_rel, raw_link, missing_rel)

    for path in files:
        text = get_text(path)
        links = set()
        for rx in (MD_LINK, HTML_REF):
            links.update(m.group(1) for m in rx.finditer(text))
        rel_src = os.path.relpath(path, root)
        for link in links:
            if is_external(link):
                continue
            cand = resolve(root, path, link)
            if cand is None:
                continue
            if os.path.exists(cand):
                ok += 1
            else:
                broken.append((rel_src, link, os.path.relpath(cand, root)))

    print(f"Scanned {len(files)} file(s) under {root}")
    print(f"Local links: {ok} ok, {len(broken)} broken")

    if broken:
        print("\n--- BROKEN LOCAL LINKS ---")
        for rel_src, link, missing in sorted(broken):
            print(f"  {rel_src}")
            print(f"    link:    {link}")
            print(f"    missing: {missing}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
