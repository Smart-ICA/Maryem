#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GUI d'alerte pour dépassement de puissance.
- Fenêtre plein écran + topmost.
- "ALARM!" en rouge + détails.
- Bip sonore CONTINU (sinusoïde) tant que la fenêtre est ouverte.
- Arrêt immédiat du bip dès fermeture de la fenêtre.

Exemple:
  python3 gui_overpower_alert.py \
      --machine "Tour CN MFJA" \
      --power 14393.9 \
      --threshold 8000 \
      --topic "Ampere" \
      --fullscreen \
      --beep --beep-backend speaker-test --beep-freq 1000
"""

import argparse
import os
import shutil
import subprocess
import sys
import signal
import tkinter as tk
from tkinter import ttk

# -----------------------------
# Bip continu (speaker-test)
# -----------------------------
def start_beep(freq_hz: int = 1000, device: str | None = None) -> subprocess.Popen | None:
    exe = shutil.which("speaker-test")
    if not exe:
        print("[ALERT GUI] speaker-test introuvable (installe 'alsa-utils')", file=sys.stderr)
        return None
    cmd = [exe, "-t", "sine", "-f", str(freq_hz)]
    if device:
        cmd += ["-D", device]
    try:
        return subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception as e:
        print(f"[ALERT GUI] Impossible de lancer speaker-test: {e}", file=sys.stderr)
        return None

def stop_beep(proc: subprocess.Popen | None):
    if not proc:
        return
    try:
        if proc.poll() is None:  # encore actif
            os.kill(proc.pid, signal.SIGKILL)
    except Exception as e:
        print(f"[ALERT GUI] Impossible de tuer speaker-test: {e}", file=sys.stderr)

# -----------------------------
# Fenêtre et UI
# -----------------------------
def enforce_fullscreen(root: tk.Tk):
    try:
        root.attributes("-topmost", True)
    except Exception:
        pass
    try:
        root.attributes("-fullscreen", True)
    except Exception:
        pass
    sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
    root.geometry(f"{sw}x{sh}+0+0")

def build_ui(root: tk.Tk, args, beep_proc):
    root.title("Alerte Puissance")
    if args.fullscreen:
        enforce_fullscreen(root)

    frame = ttk.Frame(root, padding=20)
    frame.pack(fill="both", expand=True)

    alarm_lbl = tk.Label(frame, text="ALARM!", fg="white", bg="red",
                         font=("Arial", 64, "bold"))
    alarm_lbl.pack(fill="x", pady=(0, 20))

    machine = f"Machine : {args.machine}"
    pwr     = f"Puissance mesurée : {args.power:.3f} W"
    thr     = f"Seuil configuré    : {args.threshold:.3f} W"
    topic   = f"Topic              : {args.topic}"

    details_txt = machine + "\n" + pwr + "\n" + thr + "\n" + topic
    details_lbl = tk.Label(frame, text=details_txt, fg="black", font=("Arial", 22))
    details_lbl.pack(pady=(0, 10))

    btns = ttk.Frame(frame); btns.pack(pady=10)

    def cleanup_and_exit():
        stop_beep(beep_proc)
        try:
            root.destroy()
        except Exception:
            pass

    # Bouton OK
    ttk.Button(btns, text="OK (fermer)", command=cleanup_and_exit).grid(row=0, column=0, padx=10)

    # Fermer avec Échap ou croix
    root.bind("<Escape>", lambda e: cleanup_and_exit())
    root.protocol("WM_DELETE_WINDOW", cleanup_and_exit)

    # Timeout auto éventuel
    if args.timeout and args.timeout > 0:
        root.after(args.timeout * 1000, cleanup_and_exit)

# -----------------------------
# Main
# -----------------------------
def main():
    parser = argparse.ArgumentParser(description="Fenêtre d'alerte MADS (surpuissance).")
    parser.add_argument("--machine",   type=str, required=True,  help="Nom de la machine")
    parser.add_argument("--power",     type=float, required=True, help="Puissance mesurée (W)")
    parser.add_argument("--threshold", type=float, required=True, help="Seuil de puissance (W)")
    parser.add_argument("--topic",     type=str, required=True,  help="Topic d'origine")
    parser.add_argument("--timeout",   type=int, default=0,      help="Fermeture auto en secondes (0 = jamais)")
    parser.add_argument("--fullscreen", action="store_true",     help="Fenêtre en plein écran")
    parser.add_argument("--beep",        action="store_true",     help="Activer le bip continu")
    parser.add_argument("--beep-backend", type=str, default="speaker-test",
                        choices=["speaker-test"], help="Moteur du bip (seulement speaker-test supporté ici)")
    parser.add_argument("--beep-freq",  type=int, default=1000,  help="Fréquence du bip (Hz)")
    parser.add_argument("--beep-device", type=str, default=None,
                        help="Device ALSA (ex: plughw:0,0)")

    # Compatibilité avec ton plugin C++
    parser.add_argument("--beep-interval", type=int, default=700,
                        help="Ignoré (compat)")

    args = parser.parse_args()

    # Vérifie DISPLAY
    if "DISPLAY" not in os.environ:
        print("Avertissement: pas de DISPLAY. Impossible d'ouvrir la GUI.", file=sys.stderr)
        sys.exit(0)

    # Lancer bip si demandé
    beep_proc = None
    if args.beep:
        beep_proc = start_beep(freq_hz=args.beep_freq, device=args.beep_device)

    root = tk.Tk()
    build_ui(root, args, beep_proc)
    root.mainloop()

    # Arrêt de sécurité
    stop_beep(beep_proc)

if __name__ == "__main__":
    main()

