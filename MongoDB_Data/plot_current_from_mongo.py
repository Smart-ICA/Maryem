#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Tracé courant et puissance depuis MongoDB en mode headless.

Hypothèses :
  Collection currents_full
  Chaque document contient message.data = [[t_sec, courant_A, puissance_W, ...], ...]
  t_sec est un temps en secondes relatif à minuit local, produit par le plugin buffered_sp

Objectif :
  Tracer deux séries temporelles séparées :
    1) Courant (A) en fonction du temps relatif en secondes
    2) Puissance (W) en fonction du temps relatif en secondes

Sorties :
  /tmp/courant.png
  /tmp/puissance.png

Aucune fenêtre graphique n'est ouverte.
"""

from datetime import datetime, timedelta
from pymongo import MongoClient
import matplotlib
matplotlib.use('Agg')  # Rendu sans interface graphique
import matplotlib.pyplot as plt


def load_batches(mongo_uri, db_name, coll_name, start_ts, end_ts):
    """Charge les documents entre start_ts et end_ts depuis MongoDB."""
    client = MongoClient(mongo_uri)
    coll = client[db_name][coll_name]
    query = {"timestamp": {"$gte": start_ts, "$lte": end_ts}}
    cursor = coll.find(query).sort("timestamp", 1)

    batches = []
    for doc in cursor:
        msg = doc.get("message", {})
        data = msg.get("data", [])
        batch = [row for row in data if isinstance(row, list) and len(row) >= 3]
        if batch:
            batches.append(batch)

    client.close()
    return batches


def concat_batches(batches):
    """Concatène les batches en trois listes : t_sec, I_A, P_W."""
    t_sec, I_A, P_W = [], [], []
    for batch in batches:
        for row in batch:
            t_sec.append(float(row[0]))
            I_A.append(float(row[1]))
            P_W.append(float(row[2]))
    return t_sec, I_A, P_W


def to_seconds_relative(t_sec):
    """Convertit t_sec en secondes relatives à partir du premier échantillon."""
    if not t_sec:
        return []
    t0 = t_sec[0]
    return [v - t0 for v in t_sec]


def plot_current_power(mongo_uri, db, coll, start_str, duration_s):
    """
    Lit MongoDB et enregistre deux graphes dans /tmp :
      /tmp/courant.png
      /tmp/puissance.png
    """
    start_ts = datetime.fromisoformat(start_str.replace("Z", "+00:00"))
    end_ts = start_ts + timedelta(seconds=duration_s)

    # Lecture MongoDB
    batches = load_batches(mongo_uri, db, coll, start_ts, end_ts)
    if not batches:
        print("Aucune donnée trouvée dans cet intervalle")
        return

    # Extraction
    t_sec, I_A, P_W = concat_batches(batches)
    t_rel_s = to_seconds_relative(t_sec)
    if not t_rel_s:
        print("Pas de temps exploitable")
        return

    # Courant
    plt.figure(figsize=(10, 4))
    plt.plot(t_rel_s, I_A, color="tab:blue")
    plt.xlim(0, 90)
    plt.xlabel("Temps (s)")
    plt.ylabel("Courant (A)")
    plt.title("Courant pendant l’usinage (0–90 s)")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("/tmp/courant.png", dpi=150)
    plt.close()

    # Puissance
    plt.figure(figsize=(10, 4))
    plt.plot(t_rel_s, P_W, color="tab:red")
    plt.xlim(0, 90)
    plt.xlabel("Temps (s)")
    plt.ylabel("Puissance (W)")
    plt.title("Puissance pendant l’usinage (0–90 s)")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("/tmp/puissance.png", dpi=150)
    plt.close()

    print("Figures enregistrées dans :")
    print("  /tmp/courant.png")
    print("  /tmp/puissance.png")


if __name__ == "__main__":
    # Paramètres MongoDB
    mongo_uri = "mongodb://localhost:27017"
    db = "mads_test"
    coll = "currents_full"

    # Début et durée d’analyse
    start = "2025-10-15T08:40:25.468+00:00"
    duration = 90  # secondes → arrêt à 90 s

    plot_current_power(mongo_uri, db, coll, start, duration)
