// sound_fft_alarm_gui.cpp
// Sink MADS : écoute le topic "sound_fft", écrit l'état dans un fichier JSON,
// lance une GUI Python persistante qui lit ce fichier et trace la FFT (bandes) + ALARM.

#include <sink.hpp>
#include <nlohmann/json.hpp>
#include <pugg/Kernel.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using json = nlohmann::json;
using std::string;

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "sound_fft_alarm_gui"
#endif

class SoundFftAlarmGui : public Sink<json> {
public:
  string kind() override { return PLUGIN_NAME; }

  void set_params(void const *params) override {
    Sink::set_params(params);
    _params.merge_patch(*(json*)params);

    // chemins par défaut adaptés à ton arborescence
    _python_path   = _params.value("python_path",
      string("/home/mads2025/Documents/Maryem/Devel/Sound_FFT_Alarm_Gui/src/venv/bin/python3"));
    _script_path   = _params.value("script_path",
      string("/home/mads2025/Documents/Maryem/Devel/Sound_FFT_Alarm_Gui/src/gui_sound_fft.py"));
    _state_path    = _params.value("state_path",
      string("/tmp/sound_fft_gui_state.json"));

    _title         = _params.value("title", string("FFT Son – Monitoring"));
    _fullscreen    = _params.value("fullscreen", true);
    _beep          = _params.value("beep", true);
    _beep_interval = _params.value("beep_interval_ms", 1000);
    _fmin          = _params.value("f_min", 0.0);
    _fmax          = _params.value("f_max", 4000.0);

    // Prépare la commande de lancement (une seule fois)
    std::ostringstream cmd;
    cmd << _python_path << " " << _script_path
        << " --state "      << quote(_state_path)
        << " --title "      << quote(_title)
        << " --fmin "       << _fmin
        << " --fmax "       << _fmax
        << " --beep-interval " << _beep_interval;
    if (_fullscreen) cmd << " --fullscreen";
    if (_beep)       cmd << " --beep";
    cmd << " &"; // lancer en arrière-plan pour ne pas bloquer l'agent
    _launch_cmd = cmd.str();

    // Lancement à vide (la GUI s’ouvre et attend les MAJ de state.json)
    std::cerr << "[sound_fft_alarm_gui] Launch GUI: " << _launch_cmd << std::endl;
    std::system(_launch_cmd.c_str());
  }

  return_type load_data(json const &input, string topic = "") override {
    try {
      if (!input.contains("sound_fft") || !input["sound_fft"].is_object())
        return return_type::retry;

      const auto &sf = input["sound_fft"];
      if (!sf.contains("bands") || !sf["bands"].is_array())
        return return_type::retry;

      bool alarm = sf.value("alarm", false);
      double max_mag = sf.value("max_band_mag", 0.0);

      // On écrit l’état pour la GUI Python (écriture atomique)
      json state;
      state["bands"]       = sf["bands"];
      state["alarm"]       = alarm;
      state["max_band_mag"]= max_mag;

      // pour debug/affichage facultatif
      state["topic"]       = topic;

      // write tmp + rename (atomique)
      const string tmp = _state_path + ".tmp";
      {
        std::ofstream ofs(tmp, std::ios::trunc);
        ofs << state.dump();
      }
      std::remove(_state_path.c_str());   // ignore si absent
      std::rename(tmp.c_str(), _state_path.c_str());

      // log console utile pour mads feedback
      std::cerr << "[sound_fft_alarm_gui] bands=" << sf["bands"].size()
                << " max=" << max_mag
                << " alarm=" << (alarm ? "true" : "false") << std::endl;

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
  static string quote(const string &s) {
    std::ostringstream q; q << "\"";
    for (char c : s) q << (c=='\"' ? "\\\"" : string(1,c));
    q << "\""; return q.str();
  }

  json   _params;
  string _python_path, _script_path, _state_path, _title, _launch_cmd;
  bool   _fullscreen{true}, _beep{true};
  int    _beep_interval{1000};
  double _fmin{0.0}, _fmax{4000.0};
};

INSTALL_SINK_DRIVER(SoundFftAlarmGui, json)
