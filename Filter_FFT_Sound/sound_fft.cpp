// Filter MADS : FFT (DFT simple) sur le son (sound_level) -> bandes 10 Hz + alarme
#include <filter.hpp>
#include <nlohmann/json.hpp>
#include <pugg/Kernel.h>

#include <vector>
#include <cmath>
#include <string>
#include <map>
#include <algorithm>

using json   = nlohmann::json;
using std::string;
using std::vector;

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "sound_fft"
#endif

// ----------- DFT simple (mono-sided) -----------------------------------------
static void dft_real(const vector<double> &x, double fs,
                     vector<double> &freqs, vector<double> &mag) {
  const size_t N = x.size();
  const size_t K = N / 2 + 1;            // 0..fs/2
  freqs.resize(K);
  mag.assign(K, 0.0);

  for (size_t k = 0; k < K; ++k) {
    double re = 0.0, im = 0.0;
    for (size_t n = 0; n < N; ++n) {
      const double ang = -2.0 * M_PI * double(k) * double(n) / double(N);
      re += x[n] * std::cos(ang);
      im += x[n] * std::sin(ang);
    }
    double amp = std::sqrt(re * re + im * im) / double(N);
    if (k != 0 && k != (K - 1)) amp *= 2.0; // mono-sided scaling
    mag[k]   = amp;
    freqs[k] = (fs * k) / double(N);
  }
}

// ----------- Agrégation par bandes fixes de 10 Hz -----------------------------
static json bands_aggregate(const vector<double> &freqs,
                            const vector<double> &mag,
                            double fmin, double fmax) {
  const double width_hz = 10.0; // Bande FIXE à 10 Hz (demande projet)
  json out = json::array();
  if (freqs.empty()) return out;

  for (double b = fmin; b < fmax; b += width_hz) {
    const double b_hi = std::min(b + width_hz, fmax);
    double sum = 0.0; int cnt = 0;
    for (size_t i = 0; i < freqs.size(); ++i) {
      if (freqs[i] >= b && freqs[i] < b_hi) { sum += mag[i]; cnt++; }
    }
    const double mean_mag = (cnt > 0) ? (sum / double(cnt)) : 0.0;
    out.push_back({ {"f_low", b}, {"f_high", b_hi}, {"mean_mag", mean_mag} });
  }
  return out;
}

// ----------- Filter class -----------------------------------------------------
class SoundFft : public Filter<json, json> {
public:
  // Nom du plugin pour MADS
  string kind() override { return PLUGIN_NAME; }

  // Lecture & application des paramètres (depuis mads.ini)
  void set_params(void const *params) override {
    Filter::set_params(params);
    _params.merge_patch(*(json*)params);

    _fs           = _params.value("fs", 8000.0);      // Hz (son → kHz typiquement)
    _win_size     = _params.value("win_size", 256);  // taille fenêtre
    _fmin         = _params.value("f_min", 0.0);
    _fmax         = _params.value("f_max", _fs/2.0);  // ≤ Nyquist
    _threshold    = _params.value("threshold", 0.25);  // seuil d’alarme (mag bande)
    _confirm_wins = _params.value("confirm_windows", 2);

    // Buffer circulaire
    _buf.clear();
    _buf.reserve(_win_size);
    _over_count = 0;
  }

  // On reçoit un JSON du topic (Ampere)
  // On lit 'sound_level' (à la racine OU dans 'message')
  return_type load_data(json const &data, string topic = "") override {
    try {
      const json* root = &data;
      if (data.contains("message") && data["message"].is_object()) {
        root = &data["message"];
      }

      // sound_level doit être un nombre (0..1023 typique)
      if (!root->contains("sound_level") || !(*root)["sound_level"].is_number()) {
        _error = "sound_level manquant";
        return return_type::error;
      }

      // Normalisation simple 0..1 (pour comparer à un seuil constant)
      const double raw = (*root)["sound_level"].get<double>();
      const double s   = std::clamp(raw / 1023.0, 0.0, 1.0);

      // Empile dans le buffer glissant
      _buf.push_back(s);
      if (_buf.size() > _win_size) _buf.erase(_buf.begin());

      return return_type::success;

    } catch (const std::exception &e) {
      _error = e.what();
      return return_type::error;
    }
  }

  // Quand la fenêtre est pleine : DFT -> bandes -> alarme
  return_type process(json &out) override {
    out.clear();

    if (_buf.size() < _win_size) {
      out["status"] = "buffering";
      out["filled"] = _buf.size();
      out["need"]   = _win_size;
      return return_type::retry;
    }

    // 1) FFT (DFT simple)
    vector<double> freqs, mag;
    dft_real(_buf, _fs, freqs, mag);

    // 2) Agrégation 10 Hz entre f_min et f_max
    json bands = bands_aggregate(freqs, mag, _fmin, _fmax);

    // 3) Détection : bande maximale vs seuil
    double max_band = 0.0;
    for (auto &b : bands) {
      max_band = std::max(max_band, b["mean_mag"].get<double>());
    }
    const bool over = (max_band > _threshold);
    if (over) _over_count++; else _over_count = 0;
    const bool alarm = (_over_count >= _confirm_wins);

    // 4) Sortie JSON (consommée par sink GUI)
    out["sound_fft"] = {
      {"fs", _fs},
      {"win_size", _win_size},
      {"f_min", _fmin},
      {"f_max", _fmax},
      {"band_width", 10.0},
      {"threshold", _threshold},
      {"confirm_windows", _confirm_wins},
      {"max_band_mag", max_band},
      {"alarm", alarm},
      {"bands", bands}
    };
    return return_type::success;
  }

  // Infos visibles via `mads info`
  std::map<string,string> info() override {
    return {
      {"fs", std::to_string(_fs)},
      {"win_size", std::to_string(_win_size)},
      {"f_min", std::to_string(_fmin)},
      {"f_max", std::to_string(_fmax)},
      {"band_width", "10"},
      {"threshold", std::to_string(_threshold)},
      {"confirm_windows", std::to_string(_confirm_wins)}
    };
  }

private:
  json   _params;

  // Paramètres
  double _fs{8000.0};
  size_t _win_size{256};
  double _fmin{0.0}, _fmax{4000.0};
  double _threshold{0.25};
  int    _confirm_wins{2};

  // État
  vector<double> _buf;
  int _over_count{0};
};

// Enregistre ce filtre auprès de MADS
INSTALL_FILTER_DRIVER(SoundFft, json, json)
