// src/accel_fft_alarm_gui.cpp
#include <sink.hpp>
#include <nlohmann/json.hpp>
#include <pugg/Kernel.h>

#include <string>
#include <fstream>
#include <sstream>
#include <cstdlib>

using json = nlohmann::json;
using std::string;

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "accel_fft_alarm_gui"
#endif

class AccelFftAlarmGui : public Sink<json> {
public:
  string kind() override { return PLUGIN_NAME; }

  void set_params(void const *params) override {
    Sink::set_params(params);
    _params.merge_patch(*(json*)params);

    _python_path = _params.value(
      "python_path",
      string("/home/mads2025/Documents/Maryem/Devel/Accel_FFT_Alarm_Gui/src/venv/bin/python3"));
    _script_path = _params.value(
      "script_path",
      string("/home/mads2025/Documents/Maryem/Devel/Accel_FFT_Alarm_Gui/src/gui_line_fft.py"));
    _title      = _params.value("title", string("FFT Accélération – Monitoring"));
    _state_path = _params.value("state_path", string("/tmp/accel_fft_gui_state.json"));

    // Lance UNE fois le GUI, il va boucler et lire _state_path en continu
    std::ostringstream cmd;
    cmd << _python_path << " " << _script_path
        << " --title "  << "\"" << _title      << "\""
        << " --state "  << "\"" << _state_path << "\""
        << " &";
    std::system(cmd.str().c_str());
  }

  // À chaque message du filter accel_fft, on écrit l'état dans un fichier JSON
  return_type load_data(json const &input, string topic = "") override {
    try {
      if (!input.contains("accel_fft")) return return_type::retry;
      const json &af = input["accel_fft"];

      // Données minimales pour la GUI
      json state;
      state["title"]   = _title;
      state["alarm"]   = af.value("alarm", false);
      state["max_mag"] = af.value("max_band_mag", 0.0);
      state["bands"]   = af["bands"]; // tableau [{f_low,f_high,mean_mag}, ...]

      // fichier temporaire puis rename()
      const string tmp = _state_path + ".tmp";
      {
        std::ofstream ofs(tmp, std::ios::trunc);
        ofs << state.dump();
        ofs.flush();
      }
      std::remove(_state_path.c_str());
      std::rename(tmp.c_str(), _state_path.c_str());

      return return_type::success;

    } catch (const std::exception &e) {
      _error = e.what();
      return return_type::error;
    }
  }

  std::map<string,string> info() override {
    return {
      {"python_path", _python_path},
      {"script_path", _script_path},
      {"state_path",  _state_path},
      {"title",       _title}
    };
  }

private:
  json   _params;
  string _python_path, _script_path, _title, _state_path;
};

INSTALL_SINK_DRIVER(AccelFftAlarmGui, json)
