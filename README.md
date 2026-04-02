![GitHub repo size](https://img.shields.io/github/repo-size/kelu124/pic0rick?style=plastic)
![GitHub language count](https://img.shields.io/github/languages/count/kelu124/pic0rick?style=plastic)
![GitHub top language](https://img.shields.io/github/languages/top/kelu124/pic0rick?style=plastic)
![GitHub last commit](https://img.shields.io/github/last-commit/kelu124/pic0rick?color=red&style=plastic)

[![Slack replacement](https://badgen.net/badge/icon/Matrix.org?icon=matrix&label)](https://matrix.to/#/!dEbJSiragnEvzVBdUa:matrix.org?via=matrix.org)
[![made-with-Markdown](https://img.shields.io/badge/Made%20with-Markdown-1f425f.svg)](http://commonmark.org)


# the VGA branch project

1. The [firmware folder](/firmware/) contains the code for the VGA connection - use the uf2 file (compiled here for rp2350) to flash the board.
2. Connect the pulser, as well as the VGA connector (see the [vga folder](/hardware/vga/)).
3. Connect using the COM port (or tty port) that appeared after you connect the board with USB.
4. When typing a number between 0 and 9 (integers please) the board will run an acquisition and display it through the VGA display.

For non VGA, more complete code, check the "main branch" of this project.

# License

This work is based on three previous TAPR projects, [the echOmods project](https://github.com/kelu124/echomods/), the [un0rick project](https://doi.org/10.5281/zenodo.377054), and the [lit3rick project](https://doi.org/10.5281/zenodo.5792245) - their boards are open hardware and software, developped with open-source elements as much as possible.

Copyright Luc Jonveaux (<kelu124@gmail.com>) 2024

* The hardware is licensed under TAPR Open Hardware License (<www.tapr.org/OHL>)
* The software components are free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
* The documentation is licensed under a [Creative Commons Attribution-ShareAlike 3.0 Unported License](http://creativecommons.org/licenses/by-sa/3.0/).

## Disclaimer

This project is distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A PARTICULAR PURPOSE.
