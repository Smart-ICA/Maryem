#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Accélérations par opérations (temps + FFT auto + pics) depuis MongoDB.

Base    : mads_test
Coll.   : accel_mic
message.data = [[t_sec, ax, ay, az, sound_level], ...]
t_sec = secondes depuis minuit LOCAL (plugin buffered_sp)

Fenêtres (par rapport à 2025-10-15T08:40:26.805+00:00) :
  1) Dressage Ebauche        : 0 → 20 s
  2) Dressage finition       : 20 → 40 s
  3) Perçage                 : 40 → 60 s
  4) Usinage Rayon           : 60 → 90 s

Sorties /tmp :
  op{i}_..._accel_[x|y|z]_t.png   (séries temporelles)
  op{i}_..._fft_[x|y|z].png       (FFT auto jusqu’à fs/2, amplitude ≥ 0)
"""

from datetime import datetime, timedelta
from pymongo import MongoClient
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import re

# Utils 
def sanitize_name(s: str) -> str:
    return re.sub(r"[^\w]+", "_", s.lower()).strip("_")

def load_batches(mongo_uri, db_name, coll_name, start_ts, end_ts):
    client = MongoClient(mongo_uri)
    coll = client[db_name][coll_name]
    cur = coll.find({"timestamp": {"$gte": start_ts, "$lte": end_ts}}).sort("timestamp", 1)
    batches = []
    for doc in cur:
        data = doc.get("message", {}).get("data", [])
        rows = [r for r in data if isinstance(r, list) and len(r) >= 4]  # t, ax, ay, az
        if rows:
            batches.append(rows)
    client.close()
    return batches

def concat_axes(batches):
    t_sec, ax, ay, az = [], [], [], []
    for b in batches:
        for r in b:
            t_sec.append(float(r[0]))
            ax.append(float(r[1]))
            ay.append(float(r[2]))
            az.append(float(r[3]))
    return np.array(t_sec), np.array(ax), np.array(ay), np.array(az)

# FFT 
def estimate_fs_from_seconds(t_s: np.ndarray) -> float:
    if t_s.size < 2:
        return 0.0
    dt_med = np.median(np.diff(t_s))
    return 1.0 / dt_med if (np.isfinite(dt_med) and dt_med > 0) else 0.0

def compute_fft(signal: np.ndarray, fs: float):
    n = signal.size
    if n < 8 or fs <= 0:
        return np.array([]), np.array([])
    x = signal - np.mean(signal)
    w = np.hanning(n)
    X = np.fft.rfft(x * w)
    amp = np.abs(X) / (np.sum(w) / 2.0)   # normalisation Hann
    f = np.fft.rfftfreq(n, d=1.0 / fs)
    return f, amp

def peak_freq(f: np.ndarray, A: np.ndarray):
    if f.size == 0 or A.size == 0:
        return None, None
    k = int(np.argmax(A))
    return float(f[k]), float(A[k])

def find_peaks_simple(f: np.ndarray,
                      A: np.ndarray,
                      prominence: float = 0.10,
                      min_dist_hz: float = 2.0,
                      top: int = 8):
    if f.size < 3 or A.size < 3:
        return []
    idx = [i for i in range(1, len(A) - 1) if A[i - 1] < A[i] >= A[i + 1]]
    if not idx:
        return []
    Amax = float(np.max(A))
    if Amax <= 0:
        return []
    thr = Amax * float(prominence)
    cand = [(float(f[i]), float(A[i])) for i in idx if A[i] >= thr]
    if not cand:
        return []
    cand.sort(key=lambda z: z[1], reverse=True)
    selected = []
    for fp, ap in cand:
        if all(abs(fp - sfp) >= min_dist_hz for sfp, _ in selected):
            selected.append((fp, ap))
        if len(selected) >= top:
            break
    return selected

#Plots 
def plot_timeseries(t_abs, s, title, ylabel, path, xmin, xmax):
    fig, ax = plt.subplots(figsize=(12, 4))
    ax.plot(t_abs, s)
    ax.set_xlim(xmin, xmax)
    ax.set_xlabel("Temps (s)")
    ax.set_ylabel(ylabel)    # Accélération (g)
    ax.set_title(title)
    ax.grid(True)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)

def plot_fft_auto(f, A, title, path):
    if f.size == 0:
        return
    fig, ax = plt.subplots(figsize=(15, 6))
    ax.plot(f, A, linewidth=1.0, color='tab:blue')
    ax.set_xlim(0.0, float(np.max(f)))           # jusqu’à Nyquist
    ymax = float(np.max(A))
    ax.set_ylim(0.0, (ymax * 1.05) if ymax > 0 else 1.0)  # amplitude ≥ 0
    ax.margins(y=0.0)
    ax.set_xlabel("Fréquence (Hz)")
    ax.set_ylabel("Amplitude (g)")
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)

# Main 
def main():
    mongo_uri = "mongodb://localhost:27017"
    db_name   = "mads_test"
    coll_name = "accel_mic"

    # Début global (repère pour définir les fenêtres en horodatage)
    global_start_iso = "2025-10-15T08:40:26.805+00:00"
    global_start     = datetime.fromisoformat(global_start_iso.replace("Z", "+00:00"))

    operations = [
        {"name": "Dressage Ebauche (Vc=300m/min, f=0.35mm/tour)", "offset_s": 0.0,  "duration_s": 20.0},  # 0–20
        {"name": "Dressage finition (Vc=350m/min, f=0.1mm/tour)", "offset_s": 20.0, "duration_s": 20.0},  # 20–40
        {"name": "Perçage (Vc=310m/min, f=0.7mm/tour)",           "offset_s": 40.0, "duration_s": 20.0},  # 40–60
        {"name": "Usinage Rayon (Vc=350m/min, f=0.1mm/tour)",     "offset_s": 60.0, "duration_s": 30.0},  # 60–90
    ]

    # Origine temporelle dynamique basée sur les données (évite les soucis de TZ / tz_offset)
    t0_abs = None

    for i, op in enumerate(operations, start=1):
        name   = op["name"]
        offset = float(op["offset_s"])
        dur    = float(op["duration_s"])
        start_ts = global_start + timedelta(seconds=offset)
        end_ts   = start_ts + timedelta(seconds=dur)

        print(f"\n=== Opération {i} — {name} ===")
        print(f"Fenêtre docs : {start_ts.isoformat()} → {end_ts.isoformat()} (durée {dur:.1f}s)")

        # 1) Lecture Mongo
        batches = load_batches(mongo_uri, db_name, coll_name, start_ts, end_ts)
        if not batches:
            print("Aucune donnée trouvée pour cette fenêtre.")
            continue

        # 2) Concaténation
        t_sec, ax_sig, ay_sig, az_sig = concat_axes(batches)
        print(f"  t_sec brut : min={t_sec.min():.6f}  max={t_sec.max():.6f}  (N={t_sec.size})")

        # 3) Calibration de l'origine absolue depuis les données (clé !)
        if t0_abs is None:
            # On force le 1er point de la première fenêtre à tomber à 'offset'
            t0_abs = float(t_sec[0]) - offset
            print(f"  t0_abs fixé à {t0_abs:.6f} = premier t_sec - offset ({offset:.3f})")

        # 4) Temps absolu demandé et découpe stricte [offset, offset+dur]
        t_abs = t_sec - t0_abs
        print(f"  t_abs AVANT découpe : min={t_abs.min():.6f}  max={t_abs.max():.6f}")
        eps = 1e-9
        m = (t_abs >= offset - eps) & (t_abs <= offset + dur + eps)
        if not np.any(m):
            print(" Aucun point dans la fenêtre après découpe. "
                  "Vérifie l'alignement t0_abs et les offsets.")
            # On continue quand même à tracer sans découpe pour diagnostiquer
            m = slice(None)

        t_abs  = t_abs[m]
        ax_sig = ax_sig[m]
        ay_sig = ay_sig[m]
        az_sig = az_sig[m]
        print(f"  t_abs APRÈS découpe : min={t_abs.min():.6f}  max={t_abs.max():.6f}  (N={t_abs.size})")

        # 5) Séries temporelles (unités claires)
        short = sanitize_name(name)
        xmin, xmax = offset, offset + dur
        plot_timeseries(t_abs, ax_sig, f"Accélération X – {name}", "Accélération (g)",
                        f"/tmp/op{i}_{short}_accel_x_t.png", xmin, xmax)
        plot_timeseries(t_abs, ay_sig, f"Accélération Y – {name}", "Accélération (g)",
                        f"/tmp/op{i}_{short}_accel_y_t.png", xmin, xmax)
        plot_timeseries(t_abs, az_sig, f"Accélération Z – {name}", "Accélération (g)",
                        f"/tmp/op{i}_{short}_accel_z_t.png", xmin, xmax)

        # 6) FFT auto par axe (fs déduite du segment courant)
        if t_abs.size < 2:
            print("  Segment trop court pour estimer fs / FFT.")
            continue

        t_rel = t_abs - t_abs[0]
        fs = estimate_fs_from_seconds(t_rel)
        fX, AX = compute_fft(ax_sig, fs)
        fY, AY = compute_fft(ay_sig, fs)
        fZ, AZ = compute_fft(az_sig, fs)

        fx, Ax = peak_freq(fX, AX)
        fy, Ay = peak_freq(fY, AY)
        fz, Az = peak_freq(fZ, AZ)
        print(f"  fs estimée ≈ {fs:.2f} Hz (Nyquist ≈ {fs/2.0:.2f} Hz)")
        if fx is not None: print(f"    Pic principal X : {fx:.2f} Hz  | amplitude {Ax:.4f} (g)")
        if fy is not None: print(f"    Pic principal Y : {fy:.2f} Hz  | amplitude {Ay:.4f} (g)")
        if fz is not None: print(f"    Pic principal Z : {fz:.2f} Hz  | amplitude {Az:.4f} (g)")

        # 7) Liste des pics significatifs (par axe)
        peaksX = find_peaks_simple(fX, AX, prominence=0.10, min_dist_hz=2.0, top=8)
        peaksY = find_peaks_simple(fY, AY, prominence=0.10, min_dist_hz=2.0, top=8)
        peaksZ = find_peaks_simple(fZ, AZ, prominence=0.10, min_dist_hz=2.0, top=8)

        def _print_peaks(lbl, peaks):
            if peaks:
                print(f"    Pics {lbl} (Hz | amplitude g) :")
                for fp, ap in peaks:
                    print(f"      {fp:8.3f}  |  {ap:.5f}")
            else:
                print(f"    Aucun pic {lbl} au-dessus du seuil.")

        _print_peaks("X", peaksX)
        _print_peaks("Y", peaksY)
        _print_peaks("Z", peaksZ)

        # 8) Tracés FFT (axe X auto 0..Nyquist, axe Y démarre à 0)
        plot_fft_auto(fX, AX, f"FFT Accélération X – {name}", f"/tmp/op{i}_{short}_fft_x.png")
        plot_fft_auto(fY, AY, f"FFT Accélération Y – {name}", f"/tmp/op{i}_{short}_fft_y.png")
        plot_fft_auto(fZ, AZ, f"FFT Accélération Z – {name}", f"/tmp/op{i}_{short}_fft_z.png")

        print("  Fichiers enregistrés :")
        print(f"    /tmp/op{i}_{short}_accel_x_t.png")
        print(f"    /tmp/op{i}_{short}_accel_y_t.png")
        print(f"    /tmp/op{i}_{short}_accel_z_t.png")
        print(f"    /tmp/op{i}_{short}_fft_x.png")
        print(f"    /tmp/op{i}_{short}_fft_y.png")
        print(f"    /tmp/op{i}_{short}_fft_z.png")

if __name__ == "__main__":
    main()
