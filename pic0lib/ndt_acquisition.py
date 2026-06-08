"""
Ultrasonic NDT pulse-echo acquisition.

Bundles a captured signal with all of its acquisition parameters
(Fech, pulse settings, gain, target description), provides a labelled
(15, 5) plot, and saves/loads batches to HDF5.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import datetime
from typing import Iterable, List, Optional

import h5py
import matplotlib.pyplot as plt
import numpy as np
from scipy.signal import butter, sosfiltfilt
from scipy.ndimage import uniform_filter1d


# --------------------------------------------------------------------------- #
#  Device handle                                                              #
# --------------------------------------------------------------------------- #

_PROBE = None  # module-level cache so we don't reopen the device every shot


def get_probe(force_new: bool = False):
    """
    Return a shared `Pic0rick` instance, creating it on first call.

    Importing `pic0lib` is deferred until this is called so the rest of
    the module is usable on machines without the hardware library
    (e.g. for offline analysis or loading HDF5 files).
    """
    global _PROBE
    if _PROBE is None or force_new:
        from pic0lib.device import Pic0rick  # lazy import
        _PROBE = Pic0rick()
    return _PROBE


# --------------------------------------------------------------------------- #
#  Data model                                                                 #
# --------------------------------------------------------------------------- #

@dataclass
class UltrasonicAcquisition:
    """A single ultrasonic pulse-echo trace with its acquisition metadata."""

    signal: np.ndarray            # normalized echo, range ~[-1, 1]
    Fech:   float                 # sampling frequency [Hz]
    pon:    int                   # pulse-on duration
    poff:   int                   # pulse-off duration
    damp:   int                   # damping
    gain:   float                 # raw DAC value (0-1023) passed to MCP4812 via `write dac`
    target: str = ""              # NDT target description (block, defect, ...)
    timestamp: str = field(
        default_factory=lambda: datetime.now().isoformat(timespec="seconds")
    )
    h5_path: Optional[str] = None  # if set, .save() / from_probe auto-persist here

    # transducer characterization (used by signal_filtered)
    piezo_id:           str = ""                 # transducer identifier
    piezo_central_freq: Optional[float] = None   # [Hz]
    piezo_bandwidth:    Optional[float] = None   # [Hz]

    # ------------------------------------------------------ derived properties

    @property
    def t(self) -> np.ndarray:
        """Time vector [s]."""
        return np.arange(len(self.signal)) / self.Fech

    @property
    def pulse(self) -> dict:
        """Pulse settings as a dict (handy for **kwargs)."""
        return {"pon": self.pon, "poff": self.poff, "damp": self.damp}

    @property
    def key(self) -> tuple:
        """Identity tuple used to deduplicate acquisitions on disk."""
        return (
            round(float(self.gain), 6),
            int(self.pon),
            int(self.poff),
            int(self.damp),
            str(self.target),
            str(self.piezo_id),
        )

    @property
    def signal_filtered(self) -> np.ndarray:
        """
        Band-pass filtered signal around the piezo central frequency.
        Uses a 4th-order Butterworth applied forward-backward (zero phase).
        Requires `piezo_central_freq` and `piezo_bandwidth` to be set.
        """
        if self.piezo_central_freq is None or self.piezo_bandwidth is None:
            raise ValueError(
                "signal_filtered needs piezo_central_freq and piezo_bandwidth."
            )
        nyq = self.Fech / 2.0
        low  = max((self.piezo_central_freq - self.piezo_bandwidth / 2) / nyq, 1e-6)
        high = min((self.piezo_central_freq + self.piezo_bandwidth / 2) / nyq, 1 - 1e-6)
        sos = butter(4, [low, high], btype="bandpass", output="sos")
        return sosfiltfilt(sos, self.signal).astype(np.float32)

    def amplitude(self, start_us: float, end_us: float,
                  filtered: bool = True) -> float:
        """
        Peak amplitude (max |signal|) within a time window expressed in µs.
        Set `filtered=False` to operate on the raw signal.
        """
        sig = self.signal_filtered if filtered else self.signal
        t_us = self.t * 1e6
        mask = (t_us >= start_us) & (t_us <= end_us)
        window = sig[mask]
        return float(np.max(np.abs(window))) if window.size else 0.0

    # --------------------------------------------------------- echo detection

    def detect_echoes(
        self,
        *,
        start_us: float,
        end_us: float,
        target_thickness: float,
        speed_of_sound: float,
        smooth: int = 10,
        smooth_kernel: int = 5,
        tolerance: float = 0.30,
        min_amplitude_ratio: float = 0.10,
        dip_threshold: float = 0.5,
        plot: bool = True,
    ) -> dict:
        """
        Detect periodic back-wall echoes in the filtered signal.

        Algorithm:
          1. Rectify the filtered signal (|.|).
          2. Smooth it `smooth` times with a `smooth_kernel`-wide moving
             average -> "work signal".
          3. Find the largest peak of the work signal inside [start_us, end_us].
          4. Predict subsequent echoes at multiples of dt = 2*thickness/c and,
             for each one, search a window of ±`tolerance`*dt around the
             expected position. Accept the local maximum if it exceeds
             `min_amplitude_ratio` times the main-peak amplitude.

        Parameters
        ----------
        start_us, end_us       : analysis window for locating the main peak [µs]
        target_thickness       : expected target thickness [m]
        speed_of_sound         : longitudinal speed of sound in the target [m/s]
        smooth                 : number of moving-average passes applied to |s|
        smooth_kernel          : window size of each moving-average pass [samples]
        tolerance              : ± fraction of dt used as the search window
        min_amplitude_ratio    : minimum echo amplitude relative to the main
                                  peak; rejects noise at predicted positions
        dip_threshold          : a peak is discarded as a "dip" if its
                                  amplitude is below `dip_threshold` times the
                                  smaller of its two neighbours (single pass,
                                  applied after the initial detection).
        plot                   : whether to draw the filtered + work plot

        Returns
        -------
        dict with keys: filtered_signal, work_signal, peak_indices,
        peak_times_us, peak_amplitudes, main_peak_time_us, expected_dt_us, figure.
        """
        filtered = self.signal_filtered
        work = np.abs(filtered)
        for _ in range(int(smooth)):
            work = uniform_filter1d(work, size=int(smooth_kernel), mode="nearest")

        t_us = self.t * 1e6

        # Expected round-trip time between consecutive back-wall echoes
        dt_us = 2.0 * target_thickness / speed_of_sound * 1e6

        # --- 1) main peak inside the analysis window ----------------------
        main_mask = (t_us >= start_us) & (t_us <= end_us)
        if not np.any(main_mask):
            raise ValueError(
                f"Window [{start_us}, {end_us}] µs is outside the signal "
                f"(0..{t_us[-1]:.2f} µs)."
            )
        main_win = np.where(main_mask)[0]
        main_idx = int(main_win[np.argmax(work[main_win])])
        main_amp = float(work[main_idx])
        main_time_us = float(t_us[main_idx])

        # --- 2) subsequent peaks at multiples of dt -----------------------
        # The analysis window [start_us, end_us] bounds the whole search:
        # iteration stops once the predicted echo position passes end_us,
        # and each per-echo search window is clipped to that range too.
        peak_indices = [main_idx]
        peak_amps = [main_amp]
        peak_n = [0]                          # echo number (0 = main peak)
        thresh = min_amplitude_ratio * main_amp
        tol_us = tolerance * dt_us
        n = 1
        while True:
            center = main_time_us + n * dt_us
            if center > end_us:
                break
            lo = max(center - tol_us, start_us)
            hi = min(center + tol_us, end_us)
            win = np.where((t_us >= lo) & (t_us <= hi))[0]
            if win.size == 0:
                n += 1
                continue
            cand_idx = int(win[np.argmax(work[win])])
            cand_amp = float(work[cand_idx])
            if cand_amp >= thresh:
                peak_indices.append(cand_idx)
                peak_amps.append(cand_amp)
                peak_n.append(n)
            n += 1

        peak_times_us = [float(t_us[i]) for i in peak_indices]

        # --- 3) dip filter: drop peaks well below both neighbours ---------
        discarded_times_us: List[float] = []
        if len(peak_amps) >= 3:
            keep = [True] * len(peak_amps)
            for i in range(1, len(peak_amps) - 1):
                neighbour_min = min(peak_amps[i - 1], peak_amps[i + 1])
                if peak_amps[i] < dip_threshold * neighbour_min:
                    keep[i] = False
            discarded_times_us = [peak_times_us[i]
                                  for i, k in enumerate(keep) if not k]
            peak_indices  = [peak_indices[i]  for i, k in enumerate(keep) if k]
            peak_amps     = [peak_amps[i]     for i, k in enumerate(keep) if k]
            peak_n        = [peak_n[i]        for i, k in enumerate(keep) if k]
            peak_times_us = [peak_times_us[i] for i, k in enumerate(keep) if k]

        # --- 4) thickness from first & last surviving peaks ---------------
        # Use the echo-number gap, not the list-index gap, so a discarded
        # middle peak still contributes its interval to the count.
        if len(peak_times_us) >= 2:
            first_us = peak_times_us[0]
            last_us  = peak_times_us[-1]
            n_intervals = peak_n[-1] - peak_n[0]
            measured_dt_us = (last_us - first_us) / n_intervals
            calculated_thickness = measured_dt_us * 1e-6 * speed_of_sound / 2.0
        else:
            measured_dt_us = None
            calculated_thickness = None
            n_intervals = 0

        # --- 5) plot ------------------------------------------------------
        fig = None
        if plot:
            fig, ax = plt.subplots(figsize=(15, 5))
            ax.plot(t_us, self.signal, lw=0.8, color="tab:blue", alpha=0.45,
                    label="raw")
            ax.plot(t_us, work, lw=1.1, color="tab:orange",
                    label="|filtered| smoothed")
            ax.axvspan(start_us, end_us, color="gray", alpha=0.10,
                       label=f"window [{start_us}, {end_us}] µs")
            for k, tp in enumerate(peak_times_us):
                ax.axvline(tp, color="tab:red", lw=1.0,
                           ls="-" if k == 0 else "--", alpha=0.85)
            for tp in discarded_times_us:
                ax.axvline(tp, color="tab:gray", lw=1.0, ls=":", alpha=0.6)

            # dummy legend entries for the peak markers
            ax.plot([], [], color="tab:red", lw=1.0, ls="--",
                    label=f"{len(peak_times_us)} peak(s) kept")
            if discarded_times_us:
                ax.plot([], [], color="tab:gray", lw=1.0, ls=":",
                        label=f"{len(discarded_times_us)} dip(s) discarded")

            title_lines = [
                self.label(),
                f"expected Δt = {dt_us:.2f} µs  "
                f"(thickness={target_thickness*1e3:.2f} mm, "
                f"c={speed_of_sound:.0f} m/s)",
            ]
            if calculated_thickness is not None:
                title_lines.append(
                    f"measured Δt = {measured_dt_us:.2f} µs "
                    f"(first → last over {n_intervals} interval(s))  →  "
                    f"calculated thickness = {calculated_thickness*1e3:.3f} mm"
                )
            ax.set_title("\n".join(title_lines))
            ax.set_xlabel("Time [µs]")
            ax.set_ylabel("Amplitude [norm.]")
            ax.grid(True, alpha=0.3)
            ax.margins(x=0)
            ax.legend(loc="upper right", fontsize=9)

        return {
            "filtered_signal":      filtered,
            "work_signal":          work,
            "peak_indices":         peak_indices,
            "peak_times_us":        peak_times_us,
            "peak_amplitudes":      peak_amps,
            "discarded_times_us":   discarded_times_us,
            "main_peak_time_us":    main_time_us,
            "expected_dt_us":       dt_us,
            "measured_dt_us":       measured_dt_us,
            "calculated_thickness": calculated_thickness,
            "figure":               fig,
        }

    # -------------------------------------------------------------- persistence

    def save(self, h5_path: Optional[str] = None) -> None:
        """
        Append this acquisition to an HDF5 file, replacing any existing
        entry that shares the same (gain, pon, poff, damp, target) key.

        If `h5_path` is given, it is also remembered on the instance.
        """
        path = h5_path or self.h5_path
        if path is None:
            raise ValueError(
                "No h5_path set on this acquisition; pass h5_path= explicitly."
            )
        _write_one(path, self)
        self.h5_path = path

    # ------------------------------------------------------ acquisition helper

    @classmethod
    def from_probe(
        cls,
        Fech: float,
        *,
        probe=None,
        pon: int = 70,
        poff: int = 70,
        damp: int = 6000,
        gain: float = 20,
        target: str = "",
        piezo_id: str = "",
        piezo_central_freq: Optional[float] = None,
        piezo_bandwidth: Optional[float] = None,
        h5_path: Optional[str] = None,
        overwrite: bool = False,
    ) -> "UltrasonicAcquisition":
        """
        Trigger an acquisition and wrap the resulting trace.

        `probe`     : existing Pic0rick instance, or None to use the shared one.
        `gain`      : raw 10-bit DAC value (0–1023) sent to the MCP4812 SPI DAC,
                      which sets the VGAIN pin on the AD8331 TGC (7.5–55.5 dB).
                      This is NOT a dB value; typical range is 0–500.
        `pon`/`poff`/`damp` : pulse timing in nanoseconds.
        `h5_path`   : if given, the acquisition is persisted to that file
                      using (gain, pon, poff, damp, target) as the dedup key.
        `overwrite` : if False (default) and an entry with the same key
                      already exists in `h5_path`, the cached version is
                      loaded and returned — the hardware is NOT triggered.
                      Set True to force re-acquisition and replace the cache.

        `piezo_id` / `piezo_central_freq` / `piezo_bandwidth` are stored as
        metadata. On a cache hit, if any of them are passed and differ from
        what's on disk, the cached entry is refreshed.
        """
        # Cache hit -> skip the hardware entirely
        if h5_path is not None and not overwrite:
            key = (round(float(gain), 6), int(pon), int(poff),
                   int(damp), str(target), str(piezo_id))
            cached = _load_by_key(h5_path, key)
            if cached is not None:
                dirty = False
                if (piezo_central_freq is not None
                        and cached.piezo_central_freq != piezo_central_freq):
                    cached.piezo_central_freq = piezo_central_freq
                    dirty = True
                if (piezo_bandwidth is not None
                        and cached.piezo_bandwidth != piezo_bandwidth):
                    cached.piezo_bandwidth = piezo_bandwidth
                    dirty = True
                if dirty:
                    cached.save()
                return cached

        if probe is None:
            probe = get_probe()
        probe.dac(gain)
        probe.pulse_adc_trigger(pon=pon, poff=poff, damp=damp)
        C = probe.read()
        raw = [x.replace("b'", "") for x in str(C[2]).split(",") if len(x)]
        signal = np.array(
            [(int(x, 16) - 512) / 512.0 for x in raw[:-1]], dtype=np.float32
        )
        a = cls(
            signal=signal, Fech=Fech,
            pon=pon, poff=poff, damp=damp,
            gain=gain, target=target,
            piezo_id=piezo_id,
            piezo_central_freq=piezo_central_freq,
            piezo_bandwidth=piezo_bandwidth,
            h5_path=h5_path,
        )
        if h5_path is not None:
            a.save()
        return a

    # ------------------------------------------------------------------- info

    @classmethod
    def info(cls, h5_path: str, verbose: bool = True) -> dict:
        """
        Summarize the acquisitions stored in an HDF5 file.

        Returns a JSON-compatible dict with: h5_path, n_signals, gains
        (unique, sorted), and targets (unique, sorted). When `verbose`
        is True (default), the summary is also pretty-printed.
        """
        try:
            h5 = h5py.File(h5_path, "r")
        except (FileNotFoundError, OSError):
            summary = {"h5_path": h5_path, "n_signals": 0,
                       "gains": [], "targets": []}
            if verbose:
                print(f"HDF5 file not found: {h5_path}")
            return summary

        with h5:
            names = list(h5)
            gains = sorted({float(_attr(h5[n], "gain")) for n in names})
            targets = sorted({str(_attr(h5[n], "target")) for n in names})

        summary = {
            "h5_path": h5_path,
            "n_signals": len(names),
            "gains": gains,
            "targets": targets,
        }
        if verbose:
            print(json.dumps(summary, indent=2, ensure_ascii=False))
        return summary

    # ------------------------------------------------------------ calibration

    @classmethod
    def calibrate(
        cls,
        h5_path: str,
        *,
        Fech: float,
        start_us: float,
        end_us: float,
        gain: float,
        piezo_central_freq: float,
        piezo_bandwidth: float,
        piezo_id: str = "",
        target: str = "calibration",
        damp: int = 6000,
        pon_poff_values: Iterable[int] = range(25, 156, 10),
        probe=None,
        overwrite: bool = False,
    ) -> dict:
        """
        Sweep pon = poff across `pon_poff_values` (default 25..155 step 10),
        save every acquisition to `h5_path`, and report the value that
        maximises the peak amplitude of the filtered signal inside the
        [start_us, end_us] time window.

        Produces a figure with three panels:
            - top-left    : trace at the minimum-amplitude pon/poff
            - bottom-left : trace at the maximum-amplitude pon/poff
            - right       : amplitude vs pon/poff curve

        Returns a dict with the swept values, the per-value amplitudes, and
        the optimum.
        """
        values = list(pon_poff_values)
        amps = np.empty(len(values))
        acqs: List[UltrasonicAcquisition] = []

        for i, p in enumerate(values):
            a = cls.from_probe(
                Fech=Fech, probe=probe, gain=gain,
                pon=p, poff=p, damp=damp, target=target,
                piezo_id=piezo_id,
                piezo_central_freq=piezo_central_freq,
                piezo_bandwidth=piezo_bandwidth,
                h5_path=h5_path, overwrite=overwrite,
            )
            amps[i] = a.amplitude(start_us, end_us)
            acqs.append(a)

        i_max = int(np.argmax(amps))
        i_min = int(np.argmin(amps))

        # --- figure: 2x2 grid, right column spans both rows ----------------
        fig = plt.figure(figsize=(15, 6))
        gs = fig.add_gridspec(2, 2, width_ratios=[2, 1], hspace=0.35, wspace=0.25)
        ax_min = fig.add_subplot(gs[0, 0])
        ax_max = fig.add_subplot(gs[1, 0], sharex=ax_min)
        ax_amp = fig.add_subplot(gs[:, 1])

        for ax, idx, tag in ((ax_min, i_min, "MIN"), (ax_max, i_max, "MAX")):
            a = acqs[idx]
            t_us = a.t * 1e6
            ax.plot(t_us, a.signal_filtered, lw=0.8)
            ax.axvspan(start_us, end_us, color="gray", alpha=0.15,
                       label=f"window [{start_us}, {end_us}] µs")
            ax.set_title(f"{tag}  —  pon = poff = {values[idx]}  "
                         f"(amplitude = {amps[idx]:.4f})")
            ax.set_ylabel("Amplitude [norm.]")
            ax.grid(True, alpha=0.3)
            ax.margins(x=0)
        ax_max.set_xlabel("Time [µs]")

        ax_amp.plot(values, amps, "o-", lw=1.2)
        ax_amp.plot(values[i_max], amps[i_max], "o",
                    color="tab:red", markersize=10,
                    label=f"best: pon=poff={values[i_max]}")
        ax_amp.plot(values[i_min], amps[i_min], "o",
                    color="tab:gray", markersize=8,
                    label=f"worst: pon=poff={values[i_min]}")
        ax_amp.set_xlabel("pon = poff")
        ax_amp.set_ylabel("Peak amplitude (filtered) [norm.]")
        ax_amp.set_title("Amplitude vs pulse width")
        ax_amp.grid(True, alpha=0.3)
        ax_amp.legend(loc="best", fontsize=9)

        fig.suptitle(
            f"Calibration — target='{target}', gain={gain} (DAC), "
            f"piezo {piezo_central_freq/1e6:.2f} MHz ± "
            f"{piezo_bandwidth/2/1e6:.2f} MHz"
        )

        return {
            "best_pon_poff":   values[i_max],
            "best_amplitude":  float(amps[i_max]),
            "worst_pon_poff":  values[i_min],
            "worst_amplitude": float(amps[i_min]),
            "pon_poff_values": values,
            "amplitudes":      amps.tolist(),
            "figure":          fig,
        }

    # ---------------------------------------------------------------- display

    def label(self) -> str:
        """One-line summary used as plot title / legend entry."""
        target = self.target or "untitled"
        parts = [target, f"gain={self.gain} (DAC)",
                 f"pon={self.pon}, poff={self.poff}, damp={self.damp}",
                 f"Fech={self.Fech / 1e6:.2f} MHz"]
        if self.piezo_id:
            parts.insert(1, f"piezo={self.piezo_id}")
        return "  |  ".join(parts)

    def plot(
        self,
        ax: Optional[plt.Axes] = None,
        unit: str = "us",
        figsize=(15, 5),
        **plot_kwargs,
    ) -> plt.Axes:
        """
        Plot the echo on a single axis.

        The raw signal is shown; if `piezo_central_freq` and `piezo_bandwidth`
        are set, the band-pass-filtered signal is overlaid on top with a
        legend distinguishing the two. Returns the axis.
        """
        has_filter = (self.piezo_central_freq is not None
                      and self.piezo_bandwidth is not None)
        scale = {"s": 1.0, "ms": 1e3, "us": 1e6}[unit]
        t = self.t * scale

        if ax is None:
            _, ax = plt.subplots(figsize=figsize)

        lw = plot_kwargs.pop("lw", 0.8)
        ax.plot(t, self.signal, lw=lw, color="tab:blue", alpha=0.45,
                label="raw", **plot_kwargs)
        if has_filter:
            ax.plot(t, self.signal_filtered, lw=lw, color="tab:orange",
                    label="filtered")
            ax.legend(loc="upper right", fontsize=9)

        ax.set_xlabel(f"Time [{unit}]")
        ax.set_ylabel("Amplitude [norm.]")
        ax.set_title(self.label())
        ax.grid(True, alpha=0.3)
        ax.margins(x=0)
        return ax


# --------------------------------------------------------------------------- #
#  HDF5 I/O                                                                   #
# --------------------------------------------------------------------------- #

_SCALAR_ATTRS = (
    "Fech", "pon", "poff", "damp", "gain", "target", "timestamp",
    "piezo_id", "piezo_central_freq", "piezo_bandwidth",
)


def _attr(g, name):
    """Read an HDF5 attr, decoding bytes if needed."""
    v = g.attrs[name]
    return v.decode() if isinstance(v, bytes) else v


def _attr_or(g, name, default=None):
    """Like _attr, but returns `default` if the attribute is missing."""
    if name not in g.attrs:
        return default
    return _attr(g, name)


def _opt_float(v) -> Optional[float]:
    """Coerce a stored attr value to Optional[float] (NaN -> None)."""
    if v is None:
        return None
    f = float(v)
    return None if np.isnan(f) else f


def _find_group_by_key(h5, key) -> Optional[str]:
    """Return the name of the group matching `key`, or None."""
    for name in h5:
        g = h5[name]
        try:
            existing = (
                round(float(_attr(g, "gain")), 6),
                int(_attr(g, "pon")),
                int(_attr(g, "poff")),
                int(_attr(g, "damp")),
                str(_attr(g, "target")),
                str(_attr_or(g, "piezo_id", "")),
            )
        except KeyError:
            continue
        if existing == key:
            return name
    return None


def _write_one(path: str, a: "UltrasonicAcquisition") -> None:
    """Append `a` to `path`, replacing any entry with the same key."""
    with h5py.File(path, "a") as h5:
        match = _find_group_by_key(h5, a.key)
        if match is not None:
            del h5[match]
        i = 0
        while f"acq_{i:04d}" in h5:
            i += 1
        g = h5.create_group(f"acq_{i:04d}")
        g.create_dataset("signal", data=np.asarray(a.signal),
                         compression="gzip")
        for k in _SCALAR_ATTRS:
            v = getattr(a, k)
            if v is None:
                continue  # optional fields (e.g. piezo_*) omitted when unset
            g.attrs[k] = v


def _load_by_key(path: str, key: tuple) -> Optional["UltrasonicAcquisition"]:
    """Return the acquisition stored under `key`, or None if not found."""
    try:
        h5 = h5py.File(path, "r")
    except (FileNotFoundError, OSError):
        return None
    with h5:
        name = _find_group_by_key(h5, key)
        if name is None:
            return None
        g = h5[name]
        return UltrasonicAcquisition(
            signal=g["signal"][:],
            Fech=float(_attr(g, "Fech")),
            pon=int(_attr(g, "pon")),
            poff=int(_attr(g, "poff")),
            damp=int(_attr(g, "damp")),
            gain=float(_attr(g, "gain")),
            target=str(_attr(g, "target")),
            timestamp=str(_attr(g, "timestamp")),
            h5_path=path,
            piezo_id=str(_attr_or(g, "piezo_id", "")),
            piezo_central_freq=_opt_float(_attr_or(g, "piezo_central_freq")),
            piezo_bandwidth=_opt_float(_attr_or(g, "piezo_bandwidth")),
        )


def save_h5(
    path: str,
    acquisitions: Iterable["UltrasonicAcquisition"],
    overwrite_file: bool = False,
) -> None:
    """
    Save a batch of acquisitions.

    By default the file is opened in append mode and each acquisition
    replaces any prior entry with a matching key. Pass `overwrite_file=True`
    to wipe the file first.
    """
    if overwrite_file:
        with h5py.File(path, "w"):
            pass
    for a in acquisitions:
        _write_one(path, a)


def load_h5(path: str) -> List["UltrasonicAcquisition"]:
    """Load all acquisitions from an HDF5 file; loaded objects remember `path`."""
    out: List[UltrasonicAcquisition] = []
    with h5py.File(path, "r") as h5:
        for name in sorted(h5):
            g = h5[name]
            out.append(UltrasonicAcquisition(
                signal=g["signal"][:],
                Fech=float(_attr(g, "Fech")),
                pon=int(_attr(g, "pon")),
                poff=int(_attr(g, "poff")),
                damp=int(_attr(g, "damp")),
                gain=float(_attr(g, "gain")),
                target=str(_attr(g, "target")),
                timestamp=str(_attr(g, "timestamp")),
                h5_path=path,
                piezo_id=str(_attr_or(g, "piezo_id", "")),
                piezo_central_freq=_opt_float(_attr_or(g, "piezo_central_freq")),
                piezo_bandwidth=_opt_float(_attr_or(g, "piezo_bandwidth")),
            ))
    return out