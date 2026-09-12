"""pic0rick host-side library.

Version is the single source of truth in ``version.yaml`` (A.B.C, mirroring the
firmware scheme). Parsed here without a YAML dependency so ``import pic0rick``
stays lightweight (no pyserial import until you use the device).
"""

import os
import re

_VERSION_YAML = os.path.join(os.path.dirname(__file__), "version.yaml")


def _read_version():
    version, changes = "0.0.0", ""
    try:
        with open(_VERSION_YAML, "r", encoding="utf-8") as f:
            text = f.read()
        m = re.search(r'^version:\s*"?([0-9]+\.[0-9]+\.[0-9]+)"?', text, re.M)
        if m:
            version = m.group(1)
        m = re.search(r'^changes:\s*"(.*)"\s*$', text, re.M)
        if m:
            changes = m.group(1)
    except OSError:
        pass
    return version, changes


__version__, __changes__ = _read_version()


def version():
    """Return the (version, changes) tuple for the installed library."""
    return __version__, __changes__


__all__ = ["__version__", "__changes__", "version"]
