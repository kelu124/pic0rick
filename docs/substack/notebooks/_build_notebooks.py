"""Generate the Substack companion notebooks from one place.

Run from the repo root with the project venv, then execute them:

    .venv/bin/python docs/substack/notebooks/_build_notebooks.py
    cd docs/substack/notebooks && ../../../.venv/bin/jupyter nbconvert \
        --to notebook --execute --inplace 0*.ipynb

Executing (re)writes the article figures into ../images/.  Every notebook pins
the pic0rick Python library version it was written against (PIC0RICK_PY).
"""
import pathlib
import nbformat as nbf

HERE = pathlib.Path(__file__).resolve().parent
PIC0RICK_PY = "0.1.8"   # python/pic0rick/version.yaml at time of writing
FIRMWARE = "0.1.11"     # firmware/version.yaml at time of writing

SETUP = f'''# --- common setup: find the repo, import the pic0rick library, pin the version
import sys, pathlib
import numpy as np
import matplotlib.pyplot as plt

REPO = next(p for p in pathlib.Path.cwd().resolve().parents if (p / "python" / "pic0rick").is_dir())
sys.path.insert(0, str(REPO / "python"))
import pic0rick

WRITTEN_FOR = "{PIC0RICK_PY}"   # pic0rick python library version this notebook was written against
print("pic0rick python library:", pic0rick.__version__, "(notebook written for", WRITTEN_FOR + ")")
if pic0rick.__version__ != WRITTEN_FOR:
    print("WARNING: library version differs -- the API may have moved, check python/Readme.md")

FIG = pathlib.Path("../images"); FIG.mkdir(exist_ok=True)
DATA = REPO / "docs" / "data"
plt.rcParams.update({{"figure.dpi": 110, "savefig.dpi": 150, "savefig.bbox": "tight",
                     "axes.grid": True, "grid.alpha": 0.3}})

LIVE = False   # set True with a pic0rick (firmware -DDSP build, v{FIRMWARE}+) plugged in
'''


def nb(cells):
    n = nbf.v4.new_notebook()
    n.metadata["kernelspec"] = {"name": "python3", "display_name": "Python 3", "language": "python"}
    n.cells = [nbf.v4.new_markdown_cell(c[3:]) if c.startswith("md:") else nbf.v4.new_code_cell(c.strip("\n"))
               for c in cells]
    return n


# ---------------------------------------------------------------- 01 first contact
NB01 = [
f"""md:# 01 — First contact with the pic0rick

Companion notebook to *Meet the pic0rick* (`docs/substack/01-meet-the-pic0rick.md`).

* pic0rick python library **v{PIC0RICK_PY}**, firmware **v{FIRMWARE}** (`-DDSP` RP2350 build).
* Runs **offline** by default on a capture recorded with the real board
  (`docs/data/dsp_captures/`). Set `LIVE = True` to talk to your own board.""",
SETUP,
"""md:## 1. Open the port and ask the board who it is

`Pic0rick()` auto-detects the USB CDC port (`/dev/ttyACM*`, `COM*`, `tty.usbmodem*`).
`version()` parses the firmware's `version` report; `status()` returns the DSP build's status block.""",
"""from pic0rick import dsp

if LIVE:
    from pic0rick.device import Pic0rick
    probe = Pic0rick(verbose=False)
    info = probe.version()
    print(info["raw"])
    st = probe.status()
    print(dsp.describe_status(st))
else:
    print("offline: status block recorded from an RP2350 pic0rick, firmware 0.1.11\\n")
    print((DATA / "dsp_captures" / "raw" / "status.txt").read_text())""",
"""md:## 2. One raw frame

The DSP build answers `read_raw` with a binary frame: a 64-byte header (sequence, sample
rate, pulse settings, CRC32) followed by 8000 unsigned 10-bit samples at 60 MS/s.""",
"""import json

if LIVE:
    probe.set_gain(50)
    probe.configure_pulse(negative_ns=96, damp_ns=6000, positive_ns=96, order="neg-first")
    probe.arm_pulser()
    frame = probe.read_raw()
    probe.disarm_pulser()
    header = {k: getattr(frame.header, k) for k in ("sequence", "sample_count", "sample_rate_hz",
                                                     "negative_ns", "damp_ns", "positive_ns")}
    raw = frame.samples()
else:
    header = json.loads((DATA / "dsp_captures" / "raw" / "header.json").read_text())
    raw = np.load(DATA / "dsp_captures" / "raw" / "raw.npy")

print(json.dumps({k: header[k] for k in ("sequence", "sample_count", "sample_rate_hz",
                                          "negative_ns", "damp_ns", "positive_ns")}, indent=1))
print("samples:", raw.dtype, raw.shape, "min", raw.min(), "max", raw.max(), "mean %.1f" % raw.mean())""",
"""fs = 60e6
t_us = np.arange(raw.size) / fs * 1e6

fig, ax = plt.subplots(figsize=(10, 3.2))
ax.plot(t_us, raw, lw=0.6, color="#1f5f8b")
ax.axhline(512, color="k", lw=0.6, ls="--", label="mid-scale (512)")
ax.set(xlabel="time [µs]", ylabel="ADC code (10-bit)", xlim=(0, t_us[-1]),
       title="pic0rick — first raw frame (8000 samples @ 60 MS/s ≈ 133 µs)")
ax.legend(loc="upper right")
fig.savefig(FIG / "01-first-frame.png")""",
"""md:## 3. Sanity checks for a freshly assembled board

* **Mid-scale near 512** with no signal — the ADC is biased correctly.
* **The transmit pulse ("main bang")** at t≈0 saturates the front end: proof the pulser fired.
* **Something after it** — echoes from your transducer / target.""",
"""quiet = raw[-1500:]           # tail of the record, far from the main bang
bang = raw[:300]
print("baseline mean  : %.1f codes (ideal 512)" % quiet.mean())
print("baseline noise : %.2f codes rms" % quiet.std())
print("main bang span : %d .. %d codes" % (bang.min(), bang.max()))
print("pulser fired?  :", "yes" if bang.max() - bang.min() > 500 else "NO - check the pulser/HV boards")""",
]

# ---------------------------------------------------------------- 02 first A-scan
NB02 = [
f"""md:# 02 — Your first A-scan

Companion notebook to *Your first A-scan* (`docs/substack/02-your-first-a-scan.md`).
pic0rick python library **v{PIC0RICK_PY}**. Offline by default: uses a 5 MHz contact probe on a
10 mm 1018-steel block from `docs/data/calibration_block_5steps_1018steel.h5`.""",
SETUP,
"""md:## 1. Acquire (or load) one trace

`UltrasonicAcquisition.from_probe()` sets the gain, configures + arms the pulser, reads a raw
frame and normalises it to `(code − 512) / 512`. It caches into HDF5, so re-running with the same
parameters loads from disk instead of firing again.""",
"""from pic0rick.ndt_acquisition import UltrasonicAcquisition, load_h5

H5 = DATA / "calibration_block_5steps_1018steel.h5"
if LIVE:
    a = UltrasonicAcquisition.from_probe(Fech=60e6, gain=350, pon=95, poff=95, damp=6000,
                                         piezo_id="my 5MHz probe", target="steel ~10mm",
                                         h5_path="my_first_ascan.h5")
else:
    acqs = load_h5(str(H5))
    a = next(x for x in acqs if x.gain == 350 and x.pon == 95 and "10.0mm" in x.target)
a.piezo_central_freq, a.piezo_bandwidth = 5e6, 2e6
print(a.label())
print("gain", a.gain, "| pulse", a.pulse, "| samples", a.signal.size)""",
"""t_us = a.t * 1e6
fig, ax = plt.subplots(figsize=(10, 3.4))
ax.plot(t_us, a.signal, lw=0.5, color="#1f5f8b")
ax.annotate("main bang\\n(transmit pulse)", xy=(1, 0.95), xytext=(12, 0.75), arrowprops=dict(arrowstyle="->"))
ax.annotate("back-wall echo train", xy=(37.5, 0.6), xytext=(60, 0.8), arrowprops=dict(arrowstyle="->"))
ax.set(xlabel="time [µs]", ylabel="amplitude [norm.]", xlim=(0, t_us[-1]),
       title="A-scan: 5 MHz probe on 10 mm steel, gain 350")
fig.savefig(FIG / "02-ascan-raw.png")""",
"""md:## 2. What frequency are we looking at?

The spectrum of the echo window should sit around the transducer's 5 MHz.""",
"""w = (t_us > 35) & (t_us < 60)
seg = a.signal[w] - a.signal[w].mean()
spec = np.abs(np.fft.rfft(seg * np.hanning(seg.size), n=8192))
f_mhz = np.fft.rfftfreq(8192, 1 / a.Fech) / 1e6
f_peak = f_mhz[np.argmax(spec)]
print("spectral peak: %.2f MHz" % f_peak)

fig, ax = plt.subplots(figsize=(7, 3))
ax.plot(f_mhz, 20 * np.log10(spec / spec.max() + 1e-9), color="#b5462f")
ax.axvspan(4, 6, color="#1f5f8b", alpha=0.12, label="band-pass 4–6 MHz")
ax.set(xlim=(0, 15), ylim=(-50, 3), xlabel="frequency [MHz]", ylabel="dB",
       title="Spectrum of the echo window (35–60 µs)")
ax.legend()
fig.savefig(FIG / "02-spectrum.png")""",
"""md:## 3. Filter and take the envelope

A zero-phase Butterworth band-pass (`a.signal_filtered`) removes the out-of-band junk, and the
magnitude of the analytic signal (Hilbert transform) gives the envelope — the same quantity the
RP2350 DSP build computes on-board with `read_fft`.""",
"""from scipy.signal import hilbert

filt = a.signal_filtered
env = np.abs(hilbert(filt))
fig, ax = plt.subplots(figsize=(10, 3.4))
ax.plot(t_us, filt, lw=0.5, color="#9aa7b1", label="band-passed RF")
ax.plot(t_us, env, lw=1.2, color="#b5462f", label="envelope |hilbert|")
ax.set(xlim=(30, 65), xlabel="time [µs]", ylabel="amplitude [norm.]",
       title="Filtered RF and envelope — back-wall echoes every ≈3.4 µs")
ax.legend(loc="upper right")
fig.savefig(FIG / "02-envelope.png")""",
"""md:## 4. The on-board envelope (DSP build)

The RP2350 can do this itself: `read_fft` returns a 4096-sample float envelope, computed by
CMSIS-DSP on the Pico. Below is one such frame recorded on the bench.""",
"""if LIVE:
    from pic0rick.device import Pic0rick
    probe = Pic0rick(verbose=False)
    probe.arm_pulser()
    onboard = probe.read_fft().samples()
    probe.disarm_pulser()
else:
    onboard = np.load(DATA / "dsp_captures" / "envelope" / "envelope.npy")
t2 = np.arange(onboard.size) / 60e6 * 1e6
fig, ax = plt.subplots(figsize=(10, 3))
ax.plot(t2, onboard, color="#2f7d4f", lw=0.8)
ax.set(xlabel="time [µs]", ylabel="envelope [ADC codes]", xlim=(0, t2[-1]),
       title="Envelope computed on the RP2350 (read_fft, 4096 samples)")
fig.savefig(FIG / "02-onboard-envelope.png")""",
]

# ---------------------------------------------------------------- 03 gain & pulse
NB03 = [
f"""md:# 03 — Tuning gain and pulse width

Companion notebook to *Turning the knobs* (`docs/substack/03-turning-the-knobs.md`).
pic0rick python library **v{PIC0RICK_PY}**. Offline by default (recorded sweeps on the 10 mm step).""",
SETUP,
"""from pic0rick.ndt_acquisition import UltrasonicAcquisition, load_h5
H5 = DATA / "calibration_block_5steps_1018steel.h5"
acqs = load_h5(str(H5))
for x in acqs:
    x.piezo_central_freq, x.piezo_bandwidth = 5e6, 4e6""",
"""md:## 1. Gain sweep (the AD8331 TGC)

The DAC value (0–1023) sets the AD8331 gain (≈7.5 → 55.5 dB across its range).
Live, the sweep is just a loop over `from_probe(gain=…)`.""",
"""if LIVE:
    sweep = [UltrasonicAcquisition.from_probe(Fech=60e6, gain=g, pon=70, poff=70, damp=6000,
                                              target="steel ~10mm", h5_path="gain_sweep.h5")
             for g in (0, 100, 200, 300, 400, 500)]
    for x in sweep: x.piezo_central_freq, x.piezo_bandwidth = 5e6, 4e6
else:
    sweep = sorted([x for x in acqs if x.pon == 70 and "10.0mm" in x.target], key=lambda x: x.gain)

fig, axes = plt.subplots(len(sweep), 1, figsize=(10, 1.4 * len(sweep)), sharex=True, sharey=True)
for ax, x in zip(axes, sweep):
    ax.plot(x.t * 1e6, x.signal, lw=0.4, color="#1f5f8b")
    ax.text(0.995, 0.8, f"gain {int(x.gain)}", transform=ax.transAxes, ha="right", fontsize=9)
    ax.set_ylim(-1.05, 1.05)
axes[0].set_title("Same target, increasing gain (raw signal)")
axes[-1].set_xlabel("time [µs]")
fig.savefig(FIG / "03-gain-sweep.png")""",
"""md:Quantify it: peak amplitude of the first echo, the amount of the record that is clipped, and
the noise floor in the quiet gap before the echoes (20–28 µs).""",
"""rows = []
for x in sweep:
    t = x.t * 1e6
    first = x.amplitude(35, 40, filtered=False)
    clip = np.mean(np.abs(x.signal) > 0.98) * 100
    noise = x.signal[(t > 20) & (t < 28)].std()
    rows.append((int(x.gain), first, clip, noise))
    print(f"gain {int(x.gain):4d}  first echo {first:5.2f}  clipped {clip:5.2f}%  noise {noise:.4f}")
g, first, clip, noise = map(np.array, zip(*rows))

fig, ax = plt.subplots(figsize=(7, 3.2))
ax.plot(g, first, "o-", label="1st echo peak (35–40 µs)", color="#1f5f8b")
ax.plot(g, noise * 20, "s--", label="noise rms ×20 (quiet gap 20–28 µs)", color="#b5462f")
ax.set(xlabel="DAC gain code", ylabel="amplitude [norm.]", title="Echo vs. noise as gain rises")
ax.legend()
fig.savefig(FIG / "03-gain-metrics.png")""",
"""md:## 2. Pulse-width sweep

The pulser drives a three-level pulse; `pon`/`poff` are the half-widths in ns. A transducer
rings best when the half-period matches it: for 5 MHz, half a period is **100 ns**.
`UltrasonicAcquisition.calibrate()` automates the sweep live; here we reuse its recorded output.""",
"""if LIVE:
    calib = UltrasonicAcquisition.calibrate("pulse_sweep.h5", Fech=60e6, start_us=35, end_us=40, gain=40,
                                            piezo_central_freq=5e6, piezo_bandwidth=4e6,
                                            target="steel ~10mm - calibration")
    print("best pon=poff:", calib["best_pon_poff"])
pulse = sorted([x for x in acqs if "calibration" in x.target], key=lambda x: x.pon)
pw = np.array([x.pon for x in pulse])
amp = np.array([x.amplitude(35, 40) for x in pulse])
best = pw[np.argmax(amp)]
print("pulse widths [ns]:", pw)
print("best pon = poff  :", best, "ns  -> equivalent centre frequency %.2f MHz" % (1e3 / (2 * best)))

fig, ax = plt.subplots(figsize=(7, 3.2))
ax.plot(pw, amp / amp.max(), "o-", color="#2f7d4f")
ax.axvline(100, color="k", ls=":", lw=1, label="half-period of 5 MHz (100 ns)")
ax.axvline(best, color="#b5462f", lw=1, label=f"best measured ({best} ns)")
ax.set(xlabel="pon = poff [ns]", ylabel="1st echo, filtered [rel.]", title="Pulse-width sweep, gain 40")
ax.legend()
fig.savefig(FIG / "03-pulse-sweep.png")""",
"""fig, ax = plt.subplots(figsize=(10, 3.2))
for x, c in zip([pulse[0], next(p for p in pulse if p.pon == best), pulse[-1]], ["#9aa7b1", "#b5462f", "#1f5f8b"]):
    ax.plot(x.t * 1e6, x.signal_filtered, lw=0.8, color=c, label=f"pon=poff={x.pon} ns")
ax.set(xlim=(34, 42), xlabel="time [µs]", ylabel="filtered [norm.]", title="First back-wall echo for three pulse widths")
ax.legend()
fig.savefig(FIG / "03-pulse-echoes.png")""",
]

# ---------------------------------------------------------------- 04 thickness
NB04 = [
f"""md:# 04 — A €50 thickness gauge

Companion notebook to *Measuring steel with sound* (`docs/substack/04-measuring-steel-with-sound.md`).
pic0rick python library **v{PIC0RICK_PY}**. Offline by default: 5-step 1018-steel calibration block,
5 MHz contact probe, c = 5950 m/s.""",
SETUP,
"""from pic0rick.ndt_acquisition import UltrasonicAcquisition, load_h5
H5 = DATA / "calibration_block_5steps_1018steel.h5"
C_STEEL = 5950.0   # longitudinal speed of sound in 1018 steel [m/s]
acqs = load_h5(str(H5))

def shot(thickness_mm, gain):
    if LIVE:
        a = UltrasonicAcquisition.from_probe(Fech=60e6, gain=gain, pon=95, poff=95, damp=6000,
                                             target=f"steel ~{thickness_mm}mm", h5_path="thickness.h5")
    else:
        a = next(x for x in acqs if x.pon == 95 and x.gain == gain and f"~{thickness_mm:.1f}mm" in x.target)
    a.piezo_central_freq, a.piezo_bandwidth = 5e6, 2e6
    return a""",
"""md:## 1. The principle

A pulse bounces between the probe face and the back wall. Successive back-wall echoes are spaced by
the round-trip time **Δt = 2·d / c**, so **d = c·Δt / 2**. Measuring the *spacing* between echoes
(rather than the time of the first one) cancels the probe delay line and couplant thickness.""",
"""for d in (10, 15, 20, 25):
    print(f"{d} mm steel -> expected Δt = {2 * d * 1e-3 / C_STEEL * 1e6:.2f} µs")""",
"""md:## 2. 10 mm step""",
"""a10 = shot(10.0, 350)
r10 = a10.detect_echoes(start_us=35, end_us=60, target_thickness=0.010, speed_of_sound=C_STEEL,
                        smooth=15, smooth_kernel=7)
r10["figure"].savefig(FIG / "04-echoes-10mm.png")
print("echo times [µs]:", np.round(r10["peak_times_us"], 2))
print("measured Δt %.3f µs -> thickness %.3f mm" % (r10["measured_dt_us"], r10["calculated_thickness"] * 1e3))""",
"""md:## 3. 15 mm and 25 mm steps""",
"""a15 = shot(15.0, 250)
r15 = a15.detect_echoes(start_us=25, end_us=60, target_thickness=0.015, speed_of_sound=C_STEEL,
                        smooth=15, smooth_kernel=7)
r15["figure"].savefig(FIG / "04-echoes-15mm.png")
a25 = shot(25.0, 250)
r25 = a25.detect_echoes(start_us=15, end_us=60, target_thickness=0.025, speed_of_sound=C_STEEL,
                        smooth=15, smooth_kernel=7)
r25["figure"].savefig(FIG / "04-echoes-25mm.png")""",
"""md:## 4. The cautionary 20 mm shot

On the 20 mm step the detector, seeded with Δt = 6.72 µs, latches onto a train spaced ≈5 µs — the
15 mm period. The probe face was most likely straddling the 15/20 mm step edge. The plot shows it.""",
"""a20 = shot(20.0, 250)
r20 = a20.detect_echoes(start_us=25, end_us=60, target_thickness=0.020, speed_of_sound=C_STEEL,
                        smooth=15, smooth_kernel=7)
r20["figure"].savefig(FIG / "04-echoes-20mm.png")
print("echo times [µs]:", np.round(r20["peak_times_us"], 2))""",
"""md:## 5. Summary""",
"""res = [(10, r10), (15, r15), (20, r20), (25, r25)]
print(f"{'nominal':>8} {'Δt meas':>8} {'d meas':>8} {'error':>7}")
for d, r in res:
    dm = r["calculated_thickness"] * 1e3
    print(f"{d:6d} mm {r['measured_dt_us']:6.2f}µs {dm:6.2f}mm {dm - d:+6.2f}mm")

fig, ax = plt.subplots(figsize=(5, 4))
nom = np.array([d for d, _ in res]); meas = np.array([r["calculated_thickness"] * 1e3 for _, r in res])
ax.plot([5, 30], [5, 30], "k:", lw=1, label="ideal")
ax.plot(nom, meas, "o", ms=8, color="#1f5f8b", label="pic0rick")
ax.annotate("probe on the\\n15/20 mm edge", xy=(20, meas[2]), xytext=(21, 12), arrowprops=dict(arrowstyle="->"))
ax.set(xlabel="nominal step [mm]", ylabel="measured [mm]", title="Thickness from echo spacing", xlim=(5, 30), ylim=(5, 30))
ax.legend()
fig.savefig(FIG / "04-summary.png")""",
]

for name, cells in [("01_first_contact", NB01), ("02_first_ascan", NB02),
                    ("03_gain_and_pulse", NB03), ("04_thickness_gauge", NB04)]:
    nbf.write(nb(cells), HERE / f"{name}.ipynb")
    print("wrote", name)
