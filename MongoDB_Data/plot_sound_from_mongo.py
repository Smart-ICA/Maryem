#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Analyse multi-opérations du signal sonore depuis MongoDB (collection accel_mic).

Structure attendue :
  message.data = [[t_sec, ax, ay, az, sound_level], ...]
  t_sec = secondes depuis minuit local

Pour chaque opération :
  - Tracé du signal brut microphone (niveau sonore en fonction du temps)
  - Tracé du spectre FFT automatique (0 → fs/2)
  - Détection et affichage des pics principaux du spectre
"""

from datetime import datetime, timedelta
from pymongo import MongoClient
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import re

# ---------- Utilitaires ----------
def sanitize_name(s):
    return re.sub(r"[^\w]+", "_", s.lower()).strip("_")

def load_batches(mongo_uri, db, coll, start_ts, end_ts):
    client = MongoClient(mongo_uri)
    c = client[db][coll]
    cur = c.find({"timestamp": {"$gte": start_ts, "$lte": end_ts}}).sort("timestamp", 1)
    batches = []
    for doc in cur:
        data = doc.get("message", {}).get("data", [])
        rows = [r for r in data if isinstance(r, list) and len(r) >= 5]  # t, ax, ay, az, sound
        if rows:
            batches.append(rows)
    client.close()
    return batches

def concat_sound(batches):
    t_sec, sound = [], []
    for b in batches:
        for r in b:
            t_sec.append(float(r[0]))      # secondes locale
            sound.append(float(r[4]))      # sound_level
    return np.array(t_sec), np.array(sound)

def estimate_fs_from_seconds(t):
    if t.size < 2: return 0.0
    dt = np.median(np.diff(t))
    return 1.0/dt if dt > 0 else 0.0

def compute_fft(x, fs):
    n = x.size
    if n < 8 or fs <= 0: return np.array([]), np.array([])
    w = np.hanning(n)
    X = np.fft.rfft((x - np.mean(x)) * w)
    amp = np.abs(X) / (np.sum(w)/2.0)
    f = np.fft.rfftfreq(n, 1.0/fs)
    return f, amp

def find_peaks_simple(f, a, prominence=0.10, min_dist_hz=2.0, top=5):
    """Détecte les principaux pics du spectre."""
    if f.size < 3: return []
    idx = [i for i in range(1, len(a)-1) if a[i-1] < a[i] >= a[i+1]]
    if not idx: return []
    Amax = float(np.max(a))
    thr = Amax * float(prominence)
    cand = [(f[i], a[i]) for i in idx if a[i] >= thr]
    if not cand: return []
    cand.sort(key=lambda z: z[1], reverse=True)
    selected = []
    for fp, ap in cand:
        if all(abs(fp - sfp) >= min_dist_hz for sfp, _ in selected):
            selected.append((fp, ap))
        if len(selected) >= top:
            break
    return selected

def plot_raw(t_abs, y, title, xmin, xmax, path):
    plt.figure(figsize=(12, 4))
    plt.plot(t_abs, y, color="tab:blue")
    plt.xlim(xmin, xmax)
    plt.xlabel("Temps (s)")
    plt.ylabel("Niveau sonore (u.a.)")
    plt.title(title)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(path, dpi=150)
    plt.close()

def plot_fft(f, a, title, path):
    if f.size == 0: return
    plt.figure(figsize=(12, 4))
    plt.plot(f, a, color="tab:orange")
    plt.xlim(0, np.max(f))
    ymax = float(np.max(a))
    plt.ylim(0, ymax*1.05 if ymax > 0 else 1.0)
    plt.xlabel("Fréquence (Hz)")
    plt.ylabel("Amplitude (u.a.)")
    plt.title(title)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(path, dpi=150)
    plt.close()

# ---------- Main ----------
def main():
    mongo_uri = "mongodb://localhost:27017"
    db_name   = "mads_test"
    coll_name = "accel_mic"

    # Début global
    global_start_iso = "2025-10-15T08:40:26.805+00:00"
    global_start     = datetime.fromisoformat(global_start_iso.replace("Z", "+00:00"))

    operations = [
        {"name": "Dressage Ebauche",  "offset": 0.0,  "dur": 20.0},
        {"name": "Dressage Finition", "offset": 20.0, "dur": 20.0},
        {"name": "Perçage",           "offset": 40.0, "dur": 20.0},
        {"name": "Usinage Rayon",     "offset": 60.0, "dur": 30.0},  # 60→90 s
    ]

    t0_abs = None

    for i, op in enumerate(operations, start=1):
        name, offset, dur = op["name"], float(op["offset"]), float(op["dur"])
        start_ts = global_start + timedelta(seconds=offset)
        end_ts   = start_ts + timedelta(seconds=dur)

        print(f"\n=== {name} ===")
        print(f"Fenêtre docs : {start_ts.isoformat()} → {end_ts.isoformat()}  (durée {dur:.1f}s)")

        batches = load_batches(mongo_uri, db_name, coll_name, start_ts, end_ts)
        if not batches:
            print("Aucun document dans cette fenêtre.")
            continue

        t_sec, sound = concat_sound(batches)
        if t_sec.size == 0:
            print("Données vides après concaténation.")
            continue

        if t0_abs is None:
            t0_abs = float(t_sec[0]) - offset
            print(f"t0_abs fixé à {t0_abs:.6f} (premier t_sec - offset).")

        t_abs = t_sec - t0_abs
        m = (t_abs >= offset - 1e-9) & (t_abs <= offset + dur + 1e-9)
        t_abs, sound = t_abs[m], sound[m]
        if t_abs.size == 0:
            print("Tous les points sont hors-fenêtre après découpe.")
            continue

        print(f"Points : {t_abs.size} | t_abs min={t_abs.min():.6f}  max={t_abs.max():.6f}")
        short = sanitize_name(name)

        # Signal brut
        plot_raw(
            t_abs, sound,
            f"Niveau sonore – {name}",
            xmin=offset, xmax=offset+dur,
            path=f"/tmp/op{i}_{short}_sound_raw.png"
        )

        # FFT automatique
        t_rel = t_abs - t_abs[0]
        fs = estimate_fs_from_seconds(t_rel)
        f, a = compute_fft(sound, fs)
        plot_fft(f, a, f"Spectre du signal sonore – {name}", f"/tmp/op{i}_{short}_sound_fft.png")

        # Pics
        peaks = find_peaks_simple(f, a, prominence=0.10, min_dist_hz=2.0, top=5)
        f_max = fs / 2.0
        print(f"fs estimée ≈ {fs:.2f} Hz  (Nyquist {f_max:.2f} Hz) | Pics principaux :")
        if peaks:
            for fp, ap in peaks:
                print(f"  {fp:7.2f} Hz  |  {ap:.5f}")
        else:
            print("  Aucun pic significatif.")

        print(f"→ /tmp/op{i}_{short}_sound_raw.png  /tmp/op{i}_{short}_sound_fft.png")

if __name__ == "__main__":
    main()
