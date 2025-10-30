// === overpower_email.cpp =====================================================
// Sink MADS : surveille power_W, envoie un e-mail (script Python Gmail OAuth2)
// et ouvre une fenêtre GUI plein écran avec bip continu tant qu’elle est ouverte.
// Ajout : historique JSONL des alertes (history_path dans mads.ini).

#include <sink.hpp>
#include <nlohmann/json.hpp>
#include <pugg/Kernel.h>

#include <chrono>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <ctime>

// ---- AJOUTS POUR L’HISTORIQUE ----
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
using namespace std::chrono;

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "overpower_email"
#endif

class OverpowerEmailPlugin : public Sink<json> {
public:
  std::string kind() override { return PLUGIN_NAME; }

  void set_params(void const *params) override {
    Sink::set_params(params);
    _params.merge_patch(*(json*)params);

    // --- Email -------------------------
    _threshold_W           = _params.value("threshold_W", 20.0);
    _min_alert_interval_s  = _params.value("min_alert_interval_s", 300); // 5 min
    _to_email              = _params.value<std::string>("to_email", "lhamyani@insa-toulouse.fr");
    _python_path           = _params.value<std::string>("python_path",
                             "/home/mads2025/Documents/Maryem/Devel/OverPower_Email/venv/bin/python3");
    _script_path           = _params.value<std::string>("script_path",
                             "/home/mads2025/Documents/Maryem/Devel/OverPower_Email/src/email_alert.py");
    _machine_name_cfg      = _params.value<std::string>("machine_name", "Machine CNC");

    // --- GUI ---------------------------
    _gui_python_path   = _params.value<std::string>("gui_python_path",
                         "/home/mads2025/Documents/Maryem/Devel/OverPower_Email/venv/bin/python3");
    _gui_script_path   = _params.value<std::string>("gui_script_path",
                         "/home/mads2025/Documents/Maryem/Devel/OverPower_Email/src/gui_overpower_alert.py");
    _gui_fullscreen    = _params.value("gui_fullscreen", true);
    _gui_beep          = _params.value("gui_beep", true);
    _gui_beep_backend  = _params.value<std::string>("gui_beep_backend", "aplay"); // tkbell|aplay|paplay
    _gui_beep_interval = _params.value("gui_beep_interval_ms", 700);
    _gui_timeout_s     = _params.value("gui_timeout_s", 0); // 0 = jamais

    // --- Historique (ajout) ------------
    _history_path    = _params.value<std::string>("history_path", "");
    _history_enabled = !_history_path.empty();

    _last_alert_tp = steady_clock::time_point::min();
    _last_notification.clear();
  }

  return_type load_data(json const &input, std::string topic = "") override {
    try {
      // 1) récupérer power_W (racine ou message.power_W)
      double power_W = 0.0;
      if (input.contains("power_W") && input["power_W"].is_number()) {
        power_W = input["power_W"].get<double>();
      } else if (input.contains("message") &&
                 input["message"].contains("power_W") &&
                 input["message"]["power_W"].is_number()) {
        power_W = input["message"]["power_W"].get<double>();
      } else {
        return return_type::success; // pas de donnée puissance -> ignorer
      }

      // 2) nom machine
      std::string machine = _machine_name_cfg;
      // si tu veux prioriser un champ remonté par la source :
      if (machine == "Machine CNC") {
        const json *root = &input;
        if (input.contains("message") && input["message"].is_object())
          root = &input["message"];
        if (root->contains("machine_name") && (*root)["machine_name"].is_string())
          machine = (*root)["machine_name"].get<std::string>();
        else if (root->contains("hostname") && (*root)["hostname"].is_string())
          machine = (*root)["hostname"].get<std::string>();
      }

      // 3) horodatage ISO (si présent dans le message)
      std::string ts_iso = extract_iso_timestamp(input);

      // 4) comparaison seuil + cooldown
      if (power_W > _threshold_W) {
        auto now = steady_clock::now();
        auto elapsed = duration_cast<seconds>(now - _last_alert_tp).count();
        if (_last_alert_tp == steady_clock::time_point::min() ||
            elapsed >= _min_alert_interval_s) {

          // ---- Email --------------------------------------------------------
          std::string subject = "ALERTE MADS – Puissance élevée";
          std::ostringstream body;
          body << "Bonjour,\n\n"
               << "Une alerte de dépassement de puissance a été détectée sur la machine : " << machine << ".\n\n"
               << "Détails :\n"
               << "- Puissance mesurée : " << power_W << " W\n"
               << "- Seuil configuré  : " << _threshold_W << " W\n"
               << "- Topic            : " << (topic.empty() ? "inconnu" : topic) << "\n";
          if (!ts_iso.empty())
            body << "- Horodatage       : " << ts_iso << "\n";
          body << "\nCordialement,\nMADS Monitoring\n";

          std::ostringstream cmd_mail;
          cmd_mail << _python_path << " "
                   << _script_path  << " "
                   << quote(subject) << " "
                   << quote(body.str()) << " "
                   << quote(_to_email);

          int rc_mail = std::system(cmd_mail.str().c_str());
          if (rc_mail != 0) {
            _error = "Python email script failed with code " + std::to_string(rc_mail);
            return return_type::error;
          }

          _last_alert_tp = now;
          _last_notification = "email envoyé à " + _to_email + (ts_iso.empty() ? "" : " (" + ts_iso + ")");
          std::cerr << "[overpower_email] " << _last_notification << std::endl;

          // ---- GUI plein écran + bip continu --------------------------------
          std::ostringstream cmd_gui;
          cmd_gui << _gui_python_path << " " << _gui_script_path
                  << " --machine "   << quote(machine)
                  << " --power "     << power_W
                  << " --threshold " << _threshold_W
                  << " --topic "     << quote(topic.empty() ? "Ampere" : topic)
                  << " --timeout "   << _gui_timeout_s;

          if (_gui_fullscreen) cmd_gui << " --fullscreen";
          if (_gui_beep) {
            cmd_gui << " --beep"
                    << " --beep-interval " << _gui_beep_interval;
            if (!_gui_beep_backend.empty())
              cmd_gui << " --beep-backend " << _gui_beep_backend;
          }

          // lancer en tâche de fond pour ne pas bloquer le sink
          cmd_gui << " &";
          std::cerr << "[overpower_email] Launch GUI: " << cmd_gui.str() << std::endl;
          std::system(cmd_gui.str().c_str());

          // ---- AJOUT : écrire l'historique JSONL ----------------------------
          append_history_jsonl(
            machine,
            power_W,
            _threshold_W,
            topic,
            ts_iso  // si vide, now_iso_local() sera utilisé
          );
        }
      }

      return return_type::success;

    } catch (const std::exception &e) {
      _error = e.what();
      return return_type::error;
    }
  }

  std::map<std::string, std::string> info() override {
    return {
      // Email
      {"threshold_W", std::to_string(_threshold_W)},
      {"min_alert_interval_s", std::to_string(_min_alert_interval_s)},
      {"to_email", _to_email},
      {"python_path", _python_path},
      {"script_path", _script_path},
      {"machine_name", _machine_name_cfg},
      {"last_notification", _last_notification},

      // GUI
      {"gui_python_path", _gui_python_path},
      {"gui_script_path", _gui_script_path},
      {"gui_fullscreen", _gui_fullscreen ? "true" : "false"},
      {"gui_beep", _gui_beep ? "true" : "false"},
      {"gui_beep_backend", _gui_beep_backend},
      {"gui_beep_interval_ms", std::to_string(_gui_beep_interval)},
      {"gui_timeout_s", std::to_string(_gui_timeout_s)},

      // Historique (ajout)
      {"history_path", _history_path},
      {"history_enabled", _history_enabled ? "true" : "false"}
    };
  }

private:
  static std::string quote(const std::string &s) {
    std::ostringstream q;
    q << "\"";
    for (char c : s) { if (c == '\"') q << "\\\""; else q << c; }
    q << "\"";
    return q.str();
  }

  static std::string extract_iso_timestamp(const json &in) {
    const json *root = &in;
    if (in.contains("message") && in["message"].is_object())
      root = &in["message"];

    if (root->contains("timestamp")) {
      const auto &ts = (*root)["timestamp"];
      if (ts.is_string()) return ts.get<std::string>();
      if (ts.is_object() && ts.contains("$date")) {
        if (ts["$date"].is_string()) return ts["$date"].get<std::string>();
        if (ts["$date"].is_object() && ts["$date"].contains("$numberLong")) {
          try {
            long long ms = std::stoll(ts["$date"]["$numberLong"].get<std::string>());
            std::time_t t = ms / 1000;
            std::tm tm{};
            gmtime_r(&t, &tm);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
            return std::string(buf);
          } catch (...) {}
        }
      }
    }
    return {};
  }

  // ---- AJOUTS : utilitaires d'historique ----
  static std::string now_iso_local() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t   = system_clock::to_time_t(now);
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm{};
    localtime_r(&t, &tm);

    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    char tzbuf[8];
    std::strftime(tzbuf, sizeof(tzbuf), "%z", &tm);

    std::ostringstream oss;
    oss << buf << "." << std::setw(3) << std::setfill('0') << ms.count() << tzbuf;
    return oss.str();
  }

  void append_history_jsonl(const std::string &machine,
                            double power_W,
                            double threshold_W,
                            const std::string &topic,
                            const std::string &ts_iso_msg) {
    if (!_history_enabled) return;

    try {
      const std::string ts = ts_iso_msg.empty() ? now_iso_local() : ts_iso_msg;

      std::filesystem::path p(_history_path);
      if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
      }

      std::ofstream ofs(_history_path, std::ios::app);
      if (!ofs) {
        std::cerr << "[overpower_email] WARN: cannot open history_path=" << _history_path << std::endl;
        return;
      }

      ofs << "{\"event\":\"overpower\","
          << "\"machine\":"    << quote(machine) << ","
          << "\"power_W\":"    << power_W << ","
          << "\"threshold_W\":"<< threshold_W << ","
          << "\"timestamp\":"  << quote(ts) << ","
          << "\"topic\":"      << quote(topic.empty() ? std::string("Ampere") : topic)
          << "}\n";
      ofs.flush();
    } catch (const std::exception &e) {
      std::cerr << "[overpower_email] WARN: history append failed: " << e.what() << std::endl;
    }
  }

  // --- Params email ---
  double _threshold_W{};
  int    _min_alert_interval_s{};
  std::string _to_email;
  std::string _python_path;
  std::string _script_path;
  std::string _machine_name_cfg;

  // --- Params GUI ---
  std::string _gui_python_path;
  std::string _gui_script_path;
  bool        _gui_fullscreen{true};
  bool        _gui_beep{true};
  std::string _gui_beep_backend{"aplay"};
  int         _gui_beep_interval{700};
  int         _gui_timeout_s{0};

  // --- Historique (ajout) ---
  std::string _history_path;
  bool        _history_enabled{false};

  // --- État ---
  std::chrono::steady_clock::time_point _last_alert_tp;
  std::string _last_notification;
  json _params;
};

INSTALL_SINK_DRIVER(OverpowerEmailPlugin, json)
