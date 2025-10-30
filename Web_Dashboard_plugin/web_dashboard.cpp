// === web_dashboard.cpp =======================================================
// Sink MADS : page web temps réel (fond blanc, gros chiffres)
// Routes :
//   GET /          -> page HTML
//   GET /style.css -> CSS (fichier externe si présent, sinon CSS clair par défaut)
//   GET /api/last  -> dernier échantillon JSON
//
// Paramètres mads.ini [web_dashboard]
//   sub_topic = ["Ampere"]
//   http_host  = "0.0.0.0"
//   http_port  = 8088
//   title      = "Monitoring Capteurs – Ampere"
//   refresh_ms = 500
//   static_dir = "/home/.../Web_Dashboard/static"   # dossier où l'on met style.css
// ============================================================================

#include <sink.hpp>
#include <nlohmann/json.hpp>
#include <pugg/Kernel.h>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using json = nlohmann::json;
using namespace std::chrono;

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "web_dashboard"
#endif

struct LatestSample {
  std::string ts_iso;   // horodatage affiché
  double current_A   = NAN;
  double power_W     = NAN;
  double acc_x_g     = NAN;
  double acc_y_g     = NAN;
  double acc_z_g     = NAN;
  double sound_level = NAN;
};

// ——— CSS clair (fallback si on ne trouve pas style.css sur disque) ———
static const char* kDefaultLightCSS = R"(/* Light theme for lab display */
*{box-sizing:border-box}
body{margin:0;background:#fff;color:#000;font-family:Arial,Helvetica,Ubuntu,sans-serif}
header{padding:16px 20px;border-bottom:2px solid #000;display:flex;align-items:center;gap:12px}
header h1{margin:0;font-size:40px;font-weight:800}
header .ts{margin-left:auto;font-size:22px;color:#333}
main{max-width:1200px;margin:24px auto;padding:0 16px;display:grid;gap:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:20px}
.card{background:#fff;border:2px solid #000;border-radius:14px;padding:20px}
.label{font-size:18px;color:#333;font-weight:700;margin-bottom:6px}
.kpi{font-size:56px;font-weight:900;letter-spacing:.5px}
pre{margin:8px 0 0 0;font-size:16px;background:#f7f7f7;border:1px solid #ddd;border-radius:12px;padding:12px}
footer{text-align:center;color:#444;font-size:14px;margin:10px 0}
.mono{font-family: ui-monospace,Menlo,Consolas,monospace}
)";

// ——— page HTML (on lie le CSS externe via /style.css) ———
static std::string make_html(const std::string &title, int refresh_ms) {
  std::ostringstream h;
  h <<
R"(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>)" << title << R"(</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header>
  <h1>)" << title << R"(</h1>
  <div class="ts" id="ts">—</div>
</header>

<main>
  <section class="grid">
    <div class="card">
      <div class="label">Courant (A)</div>
      <div class="kpi mono" id="current">—</div>
    </div>
    <div class="card">
      <div class="label">Puissance (W)</div>
      <div class="kpi mono" id="power">—</div>
    </div>
    <div class="card">
      <div class="label">Son (niveau ADC)</div>
      <div class="kpi mono" id="sound">—</div>
    </div>
  </section>

  <section class="grid">
    <div class="card">
      <div class="label">Accélération X (g)</div>
      <div class="kpi mono" id="ax">—</div>
    </div>
    <div class="card">
      <div class="label">Accélération Y (g)</div>
      <div class="kpi mono" id="ay">—</div>
    </div>
    <div class="card">
      <div class="label">Accélération Z (g)</div>
      <div class="kpi mono" id="az">—</div>
    </div>
  </section>

  <div class="card">
    <div class="label">Dernier JSON reçu</div>
    <pre class="mono" id="raw">—</pre>
  </div>
</main>

<footer>Données mises à jour automatiquement toutes les )" << refresh_ms << R"( ms</footer>

<script>
const REFRESH_MS = )" << refresh_ms << R"(;

function fmt(x, digits=3){ if(x===null||x===undefined||Number.isNaN(x)) return "—"; return Number(x).toFixed(digits); }

async function tick(){
  try{
    const r = await fetch('/api/last', {cache:'no-store'});
    if(!r.ok) throw new Error('HTTP '+r.status);
    const j = await r.json();

    document.getElementById('ts').textContent = j.ts_iso ?? '—';
    document.getElementById('current').textContent = fmt(j.current_A, 3);
    document.getElementById('power').textContent   = fmt(j.power_W, 1);
    document.getElementById('sound').textContent   = fmt(j.sound_level, 0);
    document.getElementById('ax').textContent      = fmt(j.acc_x_g, 3);
    document.getElementById('ay').textContent      = fmt(j.acc_y_g, 3);
    document.getElementById('az').textContent      = fmt(j.acc_z_g, 3);
    document.getElementById('raw').textContent     = JSON.stringify(j, null, 2);
  }catch(e){}
  finally{ setTimeout(tick, REFRESH_MS); }
}
tick();
</script>
</body></html>
)";
  return h.str();
}

// ——— util : lire un fichier texte si présent ———
static bool slurp_text_file(const std::string& path, std::string& out) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if(!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

class WebDashboardSink : public Sink<json> {
public:
  std::string kind() override { return PLUGIN_NAME; }

  void set_params(void const *params) override {
    Sink::set_params(params);
    _params.merge_patch(*(json*)params);

    _host       = _params.value<std::string>("http_host", "0.0.0.0");
    _port       = _params.value<int>("http_port", 8088);
    _title      = _params.value<std::string>("title", "Monitoring Capteurs – Ampere");
    _refresh_ms = _params.value<int>("refresh_ms", 500);
    _static_dir = _params.value<std::string>("static_dir", "");

    if(!_server_started.exchange(true)) start_http_server();
  }

  return_type load_data(json const &input, std::string /*topic*/) override {
    try {
      const json* root = &input;
      if (input.contains("message") && input["message"].is_object())
        root = &input["message"];

      LatestSample s{};
      if (root->contains("timestamp") && (*root)["timestamp"].is_string()) {
        s.ts_iso = (*root)["timestamp"].get<std::string>();
      } else {
        // ici on force l'heure locale
        auto now = system_clock::now();
        std::time_t t = system_clock::to_time_t(now);
        std::tm tm{};
        #ifdef _WIN32
          localtime_s(&tm, &t);
        #else
          localtime_r(&t, &tm);
        #endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        s.ts_iso = buf;
      }

      auto num = [&](const char* k, double& dst){
        auto it = root->find(k);
        if (it!=root->end() && it->is_number()) dst = it->get<double>();
      };
      num("current_A",   s.current_A);
      num("power_W",     s.power_W);
      num("sound_level", s.sound_level);

      if (root->contains("acceleration") && (*root)["acceleration"].is_object()) {
        const json& acc = (*root)["acceleration"];
        if (acc.contains("x_g") && acc["x_g"].is_number()) s.acc_x_g = acc["x_g"].get<double>();
        if (acc.contains("y_g") && acc["y_g"].is_number()) s.acc_y_g = acc["y_g"].get<double>();
        if (acc.contains("z_g") && acc["z_g"].is_number()) s.acc_z_g = acc["z_g"].get<double>();
      }

      { std::lock_guard<std::mutex> lk(_mx); _last = s; _has_last = true; }
      return return_type::success;

    } catch (const std::exception& e) {
      _error = e.what();
      return return_type::error;
    }
  }

  ~WebDashboardSink() override {
    if (_server_started.load()) {
      _svr.stop();
      if (_http_thread.joinable()) _http_thread.join();
    }
  }

  std::map<std::string, std::string> info() override {
    return {
      {"http_host", _host},
      {"http_port", std::to_string(_port)},
      {"title", _title},
      {"refresh_ms", std::to_string(_refresh_ms)},
      {"static_dir", _static_dir}
    };
  }

private:
  void start_http_server() {
    // Page HTML
    _svr.Get("/", [this](const httplib::Request&, httplib::Response& res) {
      res.set_content(make_html(_title, _refresh_ms), "text/html; charset=utf-8");
    });

    // CSS : on sert <static_dir>/style.css si présent, sinon le CSS clair par défaut
    _svr.Get("/style.css", [this](const httplib::Request&, httplib::Response& res) {
      std::string css;
      if (!_static_dir.empty()) {
        std::string path = _static_dir + "/style.css";
        if (slurp_text_file(path, css)) {
          res.set_content(css, "text/css; charset=utf-8");
          return;
        }
      }
      res.set_content(kDefaultLightCSS, "text/css; charset=utf-8");
    });

    // API JSON
    _svr.Get("/api/last", [this](const httplib::Request&, httplib::Response& res) {
      json j;
      { std::lock_guard<std::mutex> lk(_mx);
        if (_has_last) {
          j["ts_iso"]      = _last.ts_iso;   // maintenant en LOCAL
          j["current_A"]   = _last.current_A;
          j["power_W"]     = _last.power_W;
          j["acc_x_g"]     = _last.acc_x_g;
          j["acc_y_g"]     = _last.acc_y_g;
          j["acc_z_g"]     = _last.acc_z_g;
          j["sound_level"] = _last.sound_level;
        } else {
          j["status"] = "no_data_yet";
        }
      }
      res.set_content(j.dump(), "application/json; charset=utf-8");
    });

    // Thread HTTP
    _http_thread = std::thread([this](){
      std::cerr << "[web_dashboard] listening on " << _host << ":" << _port << std::endl;
      if(!_svr.listen(_host.c_str(), _port)){
        std::cerr << "[web_dashboard] ERROR: cannot listen on " << _host << ":" << _port << std::endl;
      }
    });
  }

private:
  // config
  std::string _host{"0.0.0.0"};
  int         _port{8088};
  std::string _title{"Monitoring Capteurs – Ampere"};
  int         _refresh_ms{500};
  std::string _static_dir{};

  // http
  httplib::Server _svr;
  std::thread     _http_thread;
  std::atomic<bool> _server_started{false};

  // data
  LatestSample _last;
  bool         _has_last{false};
  std::mutex   _mx;

  json _params;
};

INSTALL_SINK_DRIVER(WebDashboardSink, json)

