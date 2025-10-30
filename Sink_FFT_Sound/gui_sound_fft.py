#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GUI FFT pour le son :
- Lit périodiquement un fichier --state JSON (écrit par le sink C++) :
    {
      "bands":[{"f_low":..,"f_high":..,"mean_mag":..}, ...],
      "max_band_mag": 0.12,
      "alarm": true/false
    }
- Trace l'amplitude (mean_mag) par bande.
- Affiche "ALARM!" en rouge quand alarm = true.
- Plein écran (--fullscreen) possible.
- Bip continu (--beep) tant que la fenêtre est ouverte.

Seuil d’affichage visuel : on se base sur alarm (déjà calculée côté filter).
"""

import argparse
import json
import os
import time
import shutil
import subprocess
import tkinter as tk
from tkinter import ttk

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

# === bip périodique (voir si aplay/paplay dispo) =============================
class Beeper:
    def __init__(self, root, interval_ms=1000, device_env="GUI_BEEP_DEVICE"):
        self.root = root
        self.interval_ms = interval_ms
        self.running = False
        self._method = None

        # tk bell d'abord
        try:
            root.bell()
            self._method = ("tk", None)
        except Exception:
            pass

        if self._method is None:
            aplay = shutil.which("aplay")
            if aplay:
                dev = os.environ.get(device_env, "")
                self._method = ("aplay", (aplay, dev if dev else None))

        if self._method is None:
            paplay = shutil.which("paplay")
            if paplay:
                self._method = ("paplay", (paplay,))

    def start(self): 
        self.running = True
        self._tick()

    def stop(self):
        self.running = False

    def _tick(self):
        if not self.running:
            return
        self._beep_once()
        self.root.after(self.interval_ms, self._tick)

    def _beep_once(self):
        if self._method is None:
            return
        kind = self._method[0]
        try:
            if kind == "tk":
                self.root.bell()
            elif kind == "aplay":
                aplay, dev = self._method[1]
                wav = "/usr/share/sounds/alsa/Front_Center.wav"
                cmd = [aplay, "-q"]
                if dev: cmd += ["-D", dev]
                cmd += [wav]
                subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            elif kind == "paplay":
                paplay = self._method[1][0]
                wav = "/usr/share/sounds/alsa/Front_Center.wav"
                subprocess.Popen([paplay, wav], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass

# === lecture du state file en polling ========================================
def load_state(path):
    try:
        with open(path, "r") as f:
            return json.load(f)
    except Exception:
        return None

def band_centers(bands):
    xs, ys = [], []
    for b in bands:
        f_low  = b.get("f_low", 0.0)
        f_high = b.get("f_high", f_low)
        c = 0.5*(f_low + f_high)
        xs.append(c)
        ys.append(b.get("mean_mag", 0.0))
    return xs, ys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--state", required=True, help="Chemin du fichier JSON d'état")
    ap.add_argument("--title", default="FFT Son – Monitoring")
    ap.add_argument("--fmin", type=float, default=0.0)
    ap.add_argument("--fmax", type=float, default=20000.0)
    ap.add_argument("--fullscreen", action="store_true")
    ap.add_argument("--beep", action="store_true")
    ap.add_argument("--beep-interval", type=int, default=1000)
    args = ap.parse_args()

    if "DISPLAY" not in os.environ:
        print("Avertissement: pas de DISPLAY. Impossible d'ouvrir une fenêtre GUI.")
        return

    root = tk.Tk()
    root.title(args.title)
    if args.fullscreen:
        root.attributes("-fullscreen", True)

    # zone haute ALARM
    alarm_lbl = tk.Label(root, text="", fg="white", bg="red",
                         font=("Arial", 48, "bold"))
    alarm_lbl.pack(fill="x")

    # figure Matplotlib
    fig = Figure(figsize=(10, 5), dpi=100)
    ax = fig.add_subplot(111)
    ax.set_xlabel("Fréquence (Hz)")
    ax.set_ylabel("Amplitude (moyenne par bande)")
    ax.set_xlim(args.fmin, args.fmax)
    ax.grid(True)

    canvas = FigureCanvasTkAgg(fig, master=root)
    canvas.get_tk_widget().pack(fill="both", expand=True)

    line, = ax.plot([], [], marker="o")  # courbe simple

    # bip en continu 
    beeper = Beeper(root, interval_ms=args.beep_interval) if args.beep else None
    if beeper: beeper.start()

    # polling de fichier (mtime)
    last_mtime = 0

    def refresh():
        nonlocal last_mtime
        try:
            mtime = os.path.getmtime(args.state)
            if mtime != last_mtime:
                last_mtime = mtime
                st = load_state(args.state)
                if st and isinstance(st, dict):
                    bands = st.get("bands", [])
                    xs, ys = band_centers(bands)
                    line.set_data(xs, ys)
                    ax.set_xlim(args.fmin, args.fmax)
                    if ys:
                        ax.set_ylim(0, max(ys)*1.15 if max(ys) > 0 else 1.0)
                    canvas.draw_idle()

                    if st.get("alarm", False):
                        alarm_lbl.config(text="ALARM!")
                        alarm_lbl.pack(fill="x")
                    else:
                        alarm_lbl.config(text="")
                        alarm_lbl.pack_forget()
        except Exception:
            pass

        root.after(200, refresh)

    def on_close():
        if beeper: beeper.stop()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    refresh()
    root.mainloop()

if __name__ == "__main__":
    main()
