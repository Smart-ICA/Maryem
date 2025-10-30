// src/accel_fft.cpp
// Filter MADS : FFT (DFT simple) sur accélération tri-axes -> agrégation en bandes + alarme

#include <filter.hpp>
#include <nlohmann/json.hpp>
#include <pugg/Kernel.h>

#include <vector>
#include <cmath>
#include <string>
#include <map>
#include <algorithm>

using std::size_t;
using std::string;
using std::vector;
using json = nlohmann::json;

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "accel_fft"
#endif

// DFT réelle (naïve)
static void dft_real(const vector<double> &x, double fs,
                     vector<double> &freqs, vector<double> &mag) {
  const size_t N = x.size();
  const size_t K = N / 2 + 1; // [0 .. fs/2]
  freqs.resize(K);
  mag.assign(K, 0.0);

  for (size_t k = 0; k < K; ++k) {
    double re = 0.0, im = 0.0;
    for (size_t n = 0; n < N; ++n) {
      const double angle = -2.0 * M_PI * double(k) * double(n) / double(N);
      re += x[n] * std::cos(angle);
      im += x[n] * std::sin(angle);
    }
    double amp = std::sqrt(re * re + im * im) / double(N);
    if (k != 0 && k != (K - 1)) amp *= 2.0; // mono-sided
    mag[k] = amp;
    freqs[k] = (fs * k) / double(N);
  }
}

//Agrégation en bandes fixes
static json bands_aggregate(const vector<double> &freqs,
                            const vector<double> &mag,
                            double fmin, double fmax, double width_hz) {
  json out = json::array();
  if (freqs.empty()) return out;

  for (double b = fmin; b < fmax; b += width_hz) {
    const double b_hi = std::min(b + width_hz, fmax);
    double accum = 0.0;
    int count = 0;
    for (size_t i = 0; i < freqs.size(); ++i) {
      if (freqs[i] >= b && freqs[i] < b_hi) {
        accum += mag[i];
        count++;
      }
    }
    const double val = (count > 0) ? (accum / double(count)) : 0.0;
    out.push_back({{"f_low", b}, {"f_high", b_hi}, {"mean_mag", val}});
  }
  return out;
}

class AccelFft : public Filter<json, json> {
public:
  void set_params(void const *params) override {
    Filter::set_params(params);
    _params.merge_patch(*(json*)params);

    _axis         = _params.value("axis", string("x")); // "x"|"y"|"z"
    _fs           = _params.value("fs", 2000.0);        // Hz
    _win_size     = _params.value("win_size", 256);// échantillons
    _fmin         = _params.value("f_min", 10.0);
    _fmax         = _params.value("f_max", _fs/2.0);
    _band_w       = 10.0;                               // bandes 10 Hz
    _thresh       = _params.value("threshold", 0.5);   // seuil d’alarme
    _confirm_wins = _params.value("confirm_windows", 2);// nb fenêtres > seuil

    _buf.clear();
    _buf.reserve(_win_size);
    _over_count = 0;
  }

  string kind() override { return PLUGIN_NAME; }

  // Réception des messages Ampere plugin -> on empile l’échantillon d’axe choisi
  return_type load_data(json const &data, string topic = "") override {
    try {
      if (!data.contains("message") || !data["message"].is_object()) {
        _error = "message JSON absent";
        return return_type::error;
      }
      const auto &msg = data["message"];
      if (!msg.contains("acceleration") || !msg["acceleration"].is_object()) {
        _error = "message JSON incomplet (acceleration manquante)";
        return return_type::error;
      }
      const auto &ac = msg["acceleration"];
      if (!(ac.contains("x_g") && ac.contains("y_g") && ac.contains("z_g"))) {
        _error = "message JSON incomplet (x_g,y_g,z_g manquants)";
        return return_type::error;
      }
      const double ax = ac["x_g"].get<double>();
      const double ay = ac["y_g"].get<double>();
      const double az = ac["z_g"].get<double>();

      double a_sel = (_axis == "x") ? ax : (_axis == "y" ? ay : az);

      _buf.push_back(a_sel);
      if (_buf.size() > _win_size) _buf.erase(_buf.begin()); // fenêtre glissante
      return return_type::success;

    } catch (const std::exception &e) {
      _error = e.what();
      return return_type::error;
    }
  }

  // Quand la fenêtre est pleine : DFT -> bandes -> max -> alarme
  return_type process(json &out) override {
    out.clear();
    if (_buf.size() < _win_size) {
      out["status"] = "buffering";
      out["filled"] = _buf.size();
      out["need"]   = _win_size;
      return return_type::retry;
    }

    vector<double> freqs, mag;
    dft_real(_buf, _fs, freqs, mag);

    json bands = bands_aggregate(freqs, mag, _fmin, _fmax, _band_w);

    double max_band = 0.0;
    for (auto &b : bands) {
      max_band = std::max(max_band, b["mean_mag"].get<double>());
    }
    const bool over = (max_band > _thresh);
    if (over) _over_count++; else _over_count = 0;
    const bool alarm = (_over_count >= _confirm_wins);

    out["accel_fft"] = {
      {"axis",       _axis},
      {"fs",         _fs},
      {"win_size",   _win_size},
      {"f_min",      _fmin},
      {"f_max",      _fmax},
      {"band_width", _band_w},
      {"threshold",  _thresh},
      {"confirm_windows", _confirm_wins},
      {"max_band_mag", max_band},
      {"alarm", alarm},
      {"bands", bands}
    };
    return return_type::success;
  }

  std::map<string,string> info() override {
    return {
      {"axis", _axis},
      {"fs", std::to_string(_fs)},
      {"win_size", std::to_string(_win_size)},
      {"f_min", std::to_string(_fmin)},
      {"f_max", std::to_string(_fmax)},
      {"band_width", std::to_string(_band_w)},
      {"threshold", std::to_string(_thresh)},
      {"confirm_windows", std::to_string(_confirm_wins)}
    };
  }

private:
  string _axis{"x"};
  double _fs{2000.0);
  size_t _win_size{256};
  double _fmin{10.0},max{1000.0};
  double _band_w{10.0};
  double _thresh{0.5);
  int    _confirm_wins{2};

  vector<double> _buf;
  int _over_count{0};
};

INSTALL_FILTER_DRIVER(AccelFft, json, json)
