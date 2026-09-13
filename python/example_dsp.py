#!/usr/bin/env python3
"""Example: drive the pic0rick RP2350 DSP firmware from Python.

Targets the ``-DDSP`` firmware build (see ``firmware/`` and
``docs/dsp_test_guide.md``). Exercises the new API:

  * Pic0rick.status()            -> parsed status dict
  * Pic0rick.read_raw()          -> 8000-sample raw ADC frame
  * Pic0rick.read_fft()          -> 4096-sample Hilbert-envelope frame
  * pulser arm + a pulsed capture

Run against a connected board (auto-detects the USB-CDC port):

    python example_dsp.py                 # summary to stdout
    python example_dsp.py --save out/     # also save raw.npy / envelope.npy
    python example_dsp.py --plot out/     # also save a PNG (needs matplotlib)

Requires: pyserial, numpy (matplotlib only for --plot).
"""

import argparse
import os
import sys

import numpy as np

from pic0rick.device import Pic0rick


def summarize(name, frame):
    s = frame.samples()
    hdr = frame.header
    print(f"  {name:9s} type={hdr.payload_name:8s} samples={hdr.sample_count:5d} "
          f"bytes={hdr.payload_bytes:5d} seq={hdr.sequence}")
    if hdr.payload_type == 2:  # envelope float32
        print(f"            envelope peak={float(s.max()):.1f} "
              f"dc_mean={hdr.adc_dc_mean:.1f}")
    else:                      # raw uint16 / alaw uint8
        print(f"            min/mean/max = {int(s.min())}/{int(s.mean())}/"
              f"{int(s.max())}")
    return s


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", help="serial port (default: auto-detect)")
    ap.add_argument("--save", metavar="DIR",
                    help="save raw.npy / envelope.npy into DIR")
    ap.add_argument("--plot", metavar="DIR",
                    help="save capture.png into DIR (needs matplotlib)")
    ap.add_argument("--gain", type=int, default=300,
                    help="TGC DAC value 0..1023 (default 300)")
    args = ap.parse_args(argv)

    probe = Pic0rick(port=args.port, verbose=False)

    # 1) Identify the firmware. status() only works on a DSP build.
    try:
        st = probe.status()
    except ValueError:
        print("This board is not running a DSP (-DDSP) firmware build "
              "(no status line). Flash a rp2350-*-dsp UF2. See "
              "docs/dsp_test_guide.md.", file=sys.stderr)
        return 2
    print("Board:", st.get("board"), st.get("package"),
          "| firmware", st.get("firmware"), "| backend", st.get("dsp_backend"))

    # 2) Set the TGC gain (spi1 DAC, shared with the stdio build).
    probe.dac(args.gain)

    # 3) One-shot captures with the pulser disarmed (idle input).
    print("Captures (pulser disarmed):")
    raw = summarize("read_raw", probe.read_raw())
    env = summarize("read_fft", probe.read_fft())

    # 4) A pulsed acquisition: arm, configure, capture an envelope.
    probe.ser.write(b"pulse config 96 6000 96 neg-first\n"); probe.sread()
    probe.ser.write(b"pulser arm\n"); probe.sread()
    print("Captures (pulser armed):")
    env_pulsed = summarize("read_fft", probe.read_fft())
    probe.ser.write(b"pulser disarm\n"); probe.sread()

    # 5) Optional save / plot.
    if args.save:
        os.makedirs(args.save, exist_ok=True)
        np.save(os.path.join(args.save, "raw.npy"), raw)
        np.save(os.path.join(args.save, "envelope.npy"), env)
        print("Saved raw.npy / envelope.npy to", args.save)

    if args.plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        os.makedirs(args.plot, exist_ok=True)
        fig, ax = plt.subplots(2, 1, figsize=(9, 6))
        ax[0].plot(raw); ax[0].set_title("read_raw (8000 ADC samples)")
        ax[1].plot(env, label="idle")
        ax[1].plot(env_pulsed, label="pulser armed")
        ax[1].set_title("read_fft (4096-pt Hilbert envelope)"); ax[1].legend()
        fig.tight_layout()
        out = os.path.join(args.plot, "capture.png")
        fig.savefig(out, dpi=110)
        print("Saved", out)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
