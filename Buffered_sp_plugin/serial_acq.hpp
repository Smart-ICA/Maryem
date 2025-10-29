/*
  ____            _       _
 / ___|  ___ _ __(_) __ _| |   __ _  ___ __ _
 \___ \ / _ \ '__| |/ _` | |  / _` |/ __/ _` |
  ___) |  __/ |  | | (_| | | | (_| | (_| (_| |_
 |____/ \___|_|  |_|\__,_|_|  \__,_|\___\__, (_)
                                           |_|
Serial port acquisitor

[MOD] Version généralisée :
      - on lit des lignes JSON (NDJSON) depuis 1..N ports série (ports[] ou port)
      - on mappe des chemins JSON ("path": "a.b.c") vers un vecteur de N canaux (N = settings["channels"])
      - on gère un horodatage stable avec 'ts_key' (ex: "millis") pour reconstruire system_clock::time_point
      - on garde la compatibilité avec l’ancien schéma (j["data"]["AI1..3"]) si aucun 'map' n’est fourni
      - on tolère aussi la variante INI "sans objets JSON": map_paths/map_to/map_ports

NOTE: L’Arduino doit envoyer UNE ligne JSON par échantillon (terminée par '\n').
*/

#pragma once

#include <serial/serial.h>
#include "acquisitor.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <optional>
#include <chrono>
#include <cmath>
#include <limits>     // [MOD] pour std::numeric_limits
#include <sstream>
#include <algorithm>

using nlohmann::json;
using namespace std;
using namespace std::chrono;

// [MOD] On spécialise l'Acquisitor sur UN VECTEUR de double (taille variable)
//       plutôt que sur std::array<double,3>. Ainsi, on peut avoir 4, 7, 8 canaux, etc.
class SerialportAcquisitor : public Acquisitor<vector<double>> {
public:
  // [MOD] Constructeur identique, on appelle setup()
  SerialportAcquisitor(json j, size_t capa = 0) : Acquisitor(j, capa) { setup(); }

  ~SerialportAcquisitor() {
    // [MOD] fermeture propre si plusieurs ports ouverts
    for (auto &sp : _serials) {
      if (sp && sp->isOpen()) sp->close();
    }
  }

  // [MOD] Prépare la connexion série, lit les paramètres INI
  void setup() override {
    // [MOD] Supporte 'ports' (liste) OU l’ancien 'port' (unique). Timeout et baud paramétrables.
    if (!_serials.empty()) {
      bool all_open = true;
      for (auto &s : _serials) all_open &= (s && s->isOpen());
      if (all_open) return;
    }

    // [MOD] nombre de canaux de sortie (ex. 4 pour accel+mic, 7 pour currents+powers+mic)
    _channels = _settings.value("channels", 3);
    if (_channels <= 0) _channels = 3; // garde-fou

    _ports.clear();
    if (_settings.contains("ports")) {
      _ports = _settings["ports"].get<vector<string>>();
    } else if (_settings.contains("port")) {
      _ports.push_back(_settings.value("port", ""));
    }
    _baud = _settings.value("baud", 115200);
    _timeout = serial::Timeout::simpleTimeout(_settings.value("timeout", 100));

    _ts_key = _settings.value("ts_key", string(""));

    // --- [MOD] map: accepter soit un tableau JSON, soit une CHAÎNE JSON ---
    _map = nlohmann::json::array();
    if (_settings.contains("map")) {
      try {
        if (_settings["map"].is_string()) {
          _map = nlohmann::json::parse(_settings["map"].get<std::string>());
        } else if (_settings["map"].is_array()) {
          _map = _settings["map"];
        }
      } catch (const std::exception &e) {
        std::cerr << "[SerialportAcquisitor] map parse error: " << e.what() << "\n";
        _map = nlohmann::json::array();
      }
    }

    // --- [MOD] Variante sans objets JSON: map_paths/map_to/map_ports (INI plus robuste) ---
    if (_map.empty() && _settings.contains("map_paths") && _settings.contains("map_to")) {
      auto paths = _settings["map_paths"].get<std::vector<std::string>>();
      auto tos   = _settings["map_to"].get<std::vector<int>>();
      std::vector<int> ports(paths.size(), 0);
      if (_settings.contains("map_ports")) {
        ports = _settings["map_ports"].get<std::vector<int>>();
      }
      if (paths.size() == tos.size() && ports.size() == paths.size()) {
        for (size_t i = 0; i < paths.size(); ++i) {
          _map.push_back({{"port", ports[i]}, {"path", paths[i]}, {"to", tos[i]}});
        }
      } else {
        std::cerr << "[SerialportAcquisitor] map_paths/map_to/map_ports length mismatch\n";
      }
    }

    // [MOD] IMPORTANT : seulement après avoir tenté de construire _map !
    _legacy_expect_data_ai = _map.empty();

    _serials.clear();
    _base_clock.assign(_ports.size(), std::nullopt);

    for (auto const &p : _ports) {
      auto s = make_unique<serial::Serial>(p, _baud, _timeout);
      if (!s->isOpen()) s->open();
      _serials.push_back(std::move(s));
    }

    // [MOD] petit log utile au démarrage
    std::cerr << "[SerialportAcquisitor] mode=" << (_legacy_expect_data_ai ? "legacy(data.AI*)" : "mapping")
              << " channels=" << _channels
              << " map_size=" << _map.size()
              << " ports=" << _ports.size() << "\n";
  }


  // [MOD] Acquisition d’un seul échantillon : on lit AU PLUS une ligne JSON sur l’un des ports
  void acquire() override {
    if (is_full()) throw AcquisitorException();

    for (size_t i = 0; i < _serials.size(); ++i) {
      auto &ser = _serials[i];
      if (!ser || !ser->isOpen()) continue;

      // [MOD] readline() avec timeout (défini dans _timeout). Peut renvoyer vide.
      string raw = ser->readline();
      if (raw.empty()) continue;

      // [MOD] certaines libs ajoutent des bytes parasites → on isole strictement { ... }
      string line;
      if (!sanitize_json_line(raw, line)) continue;

      json j;
      try {
        j = json::parse(line);
      } catch (exception &e) {
        cerr << "[SerialportAcquisitor] Cannot parse JSON on port "
             << (_ports.size()>i ? _ports[i] : string("?"))
             << ": " << e.what() << "\n";
        continue;
      }

      // [MOD] On prépare un sample générique: time + vector<double> de taille _channels
      Acquisitor::sample s;
      s.data.assign(_channels, std::numeric_limits<double>::quiet_NaN());  // initialise tous les canaux à NaN

      // [MOD] Horodatage : si 'ts_key' est présent (ex "millis"), on reconstruit un time_point stable
      if (!_ts_key.empty() && j.contains(_ts_key)) {
        try {
          long long ms = j[_ts_key].get<long long>();
          auto now_tp = system_clock::now();
          if (!_base_clock[i].has_value()) {
            // 1re mesure sur ce port : base = now - millis
            _base_clock[i] = now_tp - milliseconds(ms);
          }
          s.time = *_base_clock[i] + milliseconds(ms);
        } catch (...) {
          s.time = system_clock::now();
        }
      } else {
        s.time = system_clock::now();
      }

      // [MOD] Remplissage des canaux
      if (_legacy_expect_data_ai) {
        // *** MODE HISTORIQUE (démo) — SÉCURISÉ ***
        const json* d = (j.contains("data") && j["data"].is_object()) ? &j["data"] : nullptr;
        if (!d) {
          // pas de 'data' → ce n’est pas du legacy → on ignore proprement cette ligne
          continue;
        }
        if (_channels >= 1) s.data[0] = d->value("AI1", std::numeric_limits<double>::quiet_NaN());
        if (_channels >= 2) s.data[1] = d->value("AI2", std::numeric_limits<double>::quiet_NaN());
        if (_channels >= 3) s.data[2] = d->value("AI3", std::numeric_limits<double>::quiet_NaN());
      } else {
        // *** MODE MAPPING ***
        // Chaque entrée: {"port":int, "path":"acceleration.x_g", "to":int}
        for (auto const &m : _map) {
          try {
            int   p    = m.value("port", 0);
            int   to   = m.value("to",   0);
            auto  path = m.at("path").get<string>();
            if ((int)i != p) continue;           // pas le bon port
            if (to < 0 || to >= _channels) continue;
            double val;
            if (json_get_by_path(j, path, val)) {
              s.data[(size_t)to] = val;
            }
          } catch (...) { /* ignore entrée invalide de map */ }
        }
      }

      // [MOD] On pousse UN sample et on laisse fill_buffer() rappeler acquire()
      _data.push_back(std::move(s));
      return;
    }

    // [MOD] si aucun port n’a renvoyé de ligne ce tour-ci → pas de push (fill_buffer() réessaiera)
  }

private:
  // [MOD] Coupe proprement une ligne pour ne conserver que la sous-chaîne "{...}"
  static bool sanitize_json_line(const string &in, string &out) {
    auto b = in.find('{');
    if (b == string::npos) return false;
    auto e = in.rfind('}');
    if (e == string::npos || e < b) return false;
    out = in.substr(b, e - b + 1);
    while (!out.empty() && (out.back()=='\r' || out.back()=='\n')) out.pop_back();
    return !out.empty();
  }

  // [MOD] Accès à un chemin "a.b.c" sans dépendre de at_path (compat versions)
  static bool json_get_by_path(json const &j, string const &path, double &out) {
    try {
      json const* cur = &j;
      size_t start = 0;
      while (true) {
        size_t pos = path.find('.', start);
        string key = path.substr(start, pos == string::npos ? string::npos : (pos - start));
        if (!cur->contains(key)) return false;
        cur = &(*cur)[key];
        if (pos == string::npos) break;
        start = pos + 1;
      }
      if (cur->is_number_float() || cur->is_number_integer() || cur->is_number_unsigned()) {
        out = cur->get<double>();
        return true;
      }
      return false;
    } catch (...) { return false; }
  }

private:
  int _channels{3};                                    // [MOD] nb de canaux de sortie
  vector<string> _ports;
  size_t _baud{};
  serial::Timeout _timeout;
  vector<unique_ptr<serial::Serial>> _serials;

  string _ts_key;                                      // ex: "millis"
  json   _map;                                         // mapping JSON→canaux
  bool   _legacy_expect_data_ai{false};

  vector<optional<system_clock::time_point>> _base_clock; // base temporelle par port
};

