---
name: un0rick-links
description: Link map and linking policy for outward-facing pic0rick writing (Substack articles, READMEs, posts) — send readers to the un0rick.cc/pic0rick reference documentation and to the kelu124/pic0rick GitHub for images, code and notebooks. Use whenever writing or editing text that links to the pic0rick docs, website, images or code. pic0rick repo only.
---

# un0rick.cc + GitHub links for pic0rick writing

**Policy.** Readers who arrive from Substack (or anywhere outside the repo)
should land on:
- **un0rick.cc** — the reference documentation (specs, getting started,
  troubleshooting, extensions, use cases, buying a board);
- **GitHub `kelu124/pic0rick`** — images, code, notebooks, data, hardware files.

So in outward-facing text use **absolute URLs only** (no `../images/...`
relative links: they break once pasted into Substack). Link a concept to its
un0rick.cc reference the first time it appears, and close each article with a
"Go further" block (reference docs + code).

## un0rick.cc reference pages (verified 2026-10-10, anchors from the page HTML)

| Topic | URL |
|---|---|
| pic0rick main page | https://un0rick.cc/pic0rick |
| Specifications table | https://un0rick.cc/pic0rick#specifications |
| System architecture / signal chain | https://un0rick.cc/pic0rick#system-architecture |
| PIO replaces the FPGA | https://un0rick.cc/pic0rick#how-does-it-replace-the-fpga |
| PMOD extensions | https://un0rick.cc/pic0rick#pmod-extensions |
| Board comparison (un0rick, lit3rick, lit3-32) | https://un0rick.cc/pic0rick#board-comparison |
| Source files | https://un0rick.cc/pic0rick#source-files |
| Get one (buy / build) | https://un0rick.cc/pic0rick#get-one |
| Getting started (official guide) | https://un0rick.cc/getting-started |
| — what you need | https://un0rick.cc/getting-started#step-1-what-you-need |
| — flash the firmware | https://un0rick.cc/getting-started#step-2-flash-the-firmware |
| — connect the transducer | https://un0rick.cc/getting-started#step-3-connect-the-transducer |
| — install the software | https://un0rick.cc/getting-started#step-4-install-the-software |
| — first acquisition | https://un0rick.cc/getting-started#step-5-your-first-acquisition |
| — adjusting the gain | https://un0rick.cc/getting-started#adjusting-the-gain |
| — troubleshooting | https://un0rick.cc/getting-started#easy-troubleshooting |
| — physical assembly | https://un0rick.cc/getting-started#the-physical-assembly-of-the-device |
| PMODs (HV pulser, PSRAM, R2R DAC, VGA…) | https://un0rick.cc/pmods |
| MAX14866 multiplexer | https://un0rick.cc/max14866 |
| Use cases overview | https://un0rick.cc/use_cases |
| Pulse-echo use case | https://un0rick.cc/UseCase/pulse_echo |
| NDT use case | https://un0rick.cc/UseCase/NDT |
| Research | https://un0rick.cc/research |
| Licenses | https://un0rick.cc/licenses |

Outside the docs: Tindie store https://www.tindie.com/stores/kelu124/ ·
OSHWA FR000023 https://certification.oshwa.org/fr000023.html ·
Matrix chat https://matrix.to/#/!dEbJSiragnEvzVBdUa:matrix.org?via=matrix.org

## GitHub URL patterns (repo `kelu124/pic0rick`, branch `main`)

| What | Pattern |
|---|---|
| Repo | https://github.com/kelu124/pic0rick |
| File / notebook (renders) | `https://github.com/kelu124/pic0rick/blob/main/<path>` |
| Folder | `https://github.com/kelu124/pic0rick/tree/main/<path>` |
| Image to embed | `https://raw.githubusercontent.com/kelu124/pic0rick/main/<path>` |
| Firmware releases | https://github.com/kelu124/pic0rick/releases |

Common paths: `python/` (library), `python/Readme.md`, `firmware/`,
`hardware/`, `docs/data/`, `docs/images/`, `docs/substack/images/`,
`docs/substack/notebooks/`.

**Raw image URLs only work once the file is pushed to `main`** — warn the user
if the article references images that aren't on `origin/main` yet.

## Verify before finishing

```bash
# every absolute link in the touched files: un0rick.cc pages + anchors, GitHub paths
grep -ohE 'https://(un0rick\.cc|github\.com/kelu124|raw\.githubusercontent\.com/kelu124)[^) >"]*' FILES | sort -u
curl -sL -o /dev/null -w '%{http_code} %{url_effective}\n' <url>          # page exists
curl -sL https://un0rick.cc/<page> | grep -o 'id="<anchor>"'              # anchor exists
git ls-tree -r --name-only origin/main | grep -x '<path>'                 # GitHub path pushed?
```
If a page/anchor moved, update the table above (and its verification date).
