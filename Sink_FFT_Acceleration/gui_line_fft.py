# src/gui_line_fft.py
import argparse, json, os, time
import matplotlib
matplotlib.use("TkAgg")         
import matplotlib.pyplot as plt

def load_state(path):
    try:
        with open(path, "r") as f:
            return json.load(f)
    except:
        return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--title", default="FFT Accélération – Monitoring")
    ap.add_argument("--state", required=True, help="Chemin du fichier état JSON")
    args = ap.parse_args()

    plt.ion()
    fig, ax = plt.subplots()
    fig.canvas.manager.set_window_title(args.title)
    line, = ax.plot([], [], lw=2)
    alarm_text = ax.text(0.5, 0.9, "", color="red", fontsize=24,
                         ha="center", va="center", transform=ax.transAxes,
                         fontweight="bold")

    ax.set_xlabel("Fréquence (Hz)")
    ax.set_ylabel("Amplitude (moyenne par bande)")
    ax.grid(True)

    last_mtime = 0.0
    while True:
        try:
            if os.path.exists(args.state):
                mtime = os.path.getmtime(args.state)
                if mtime != last_mtime:
                    last_mtime = mtime
                    st = load_state(args.state)
                    if st and "bands" in st:
                        bands = st["bands"]
                        # X = centre de bande ; Y = mean_mag
                        xs = [0.5*(b["f_low"]+b["f_high"]) for b in bands]
                        ys = [b["mean_mag"] for b in bands]
                        line.set_data(xs, ys)
                        ax.relim(); ax.autoscale_view()

                        # ALARM !
                        if st.get("alarm", False):
                            alarm_text.set_text("ALARM!")
                        else:
                            alarm_text.set_text("")

                        fig.canvas.draw_idle()

            plt.pause(0.05)  # rafraîchit ~20 fps, sans bloquer

        except KeyboardInterrupt:
            break
        except Exception:
            # on ignore les erreurs de lecture passagères
            plt.pause(0.1)

    plt.ioff()
    plt.show()

if __name__ == "__main__":
    main()
