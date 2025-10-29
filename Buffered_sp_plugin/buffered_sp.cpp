/*
 ____                                   _             _       
 / ___|  ___  _   _ _ __ ___ ___   _ __ | |_   _  __ _(_)_ __  
 \___ \ / _ \| | | | '__/ __/ _ \ | '_ \| | | | |/ _` | | '_ \ 
  ___) | (_) | |_| | | | (_|  __/ | |_) | | |_| | (_| | | | | |
 |____/ \___/ \__,_|_|  \___\___| | .__/|_|\__,_|\__, |_|_| |_|
                                  |_|            |___/         
   This plugin is an extended version of the original "buffered" source plugin developed by P.Paolo Bosetti. 
   This version ("buffered_sp") is specifically designed to acquire real sensor data (current, vibration, and sound) coming from Arduino serial ports.
   The plugin reads JSON-formatted lines from one or multiple serial ports,buffers the samples over a configurable time window, and publishes them as batched messages to reduce database load.
 */


// Mandatory included headers
#include <source.hpp>
#include <nlohmann/json.hpp>
#include <pugg/Kernel.h>

// other includes as needed here
#include <chrono>
#include <sstream>              //  small helper to convert json → string
#include "serial_acq.hpp"       //  class now handles multi-port NDJSON + mapping

// Define the name of the plugin
#ifndef PLUGIN_NAME
#define PLUGIN_NAME "buffered_sp"
#endif

// Load the namespaces
using namespace std;
using json = nlohmann::json;

// Safe helper to convert a json value to string (avoids std::to_string(json))
static inline std::string json_to_string(const json &j) {
  if (j.is_number_integer())   return std::to_string(j.get<long long>());
  if (j.is_number_unsigned())  return std::to_string(j.get<unsigned long long>());
  if (j.is_number_float())     return std::to_string(j.get<double>());
  if (j.is_string())           return j.get<std::string>();
  return j.dump();
}

// Plugin class. This shall be the only part that needs to be modified,
// implementing the actual functionality
class BufferedPlugin : public Source<json> {
public:
  // Typically, no need to change this
  string kind() override { return PLUGIN_NAME; }

  // Implement the actual functionality here
  return_type get_output(json &out,
                         std::vector<unsigned char> *blob = nullptr) override {
    out.clear();
    if (!_agent_id.empty()) out["agent_id"] = _agent_id;

    // [MOD] Fill the buffer from serial port(s) (NDJSON + mapping)
    _acq->fill_buffer();

    // Output formatting:
    // out["data"] = [[t_rel, ch0, ch1, ... chN], ...]  // N = channels
    out["data"] = json::array();
    json e = json::array();
    for (auto &sample : _acq->data()) {
      e = json::array();
      e.push_back(sample.time_since(_today));
      // Push all detected channels (dynamic size)
      for (double v : sample.data) e.push_back(v);
      out["data"].push_back(e);
    }

    return return_type::success;
  }

  void set_params(void const *params) override {
    Source::set_params(params);
    // default values
    _params["capacity"]  = 100;
    _params["mean"]      = 10;    // (inherited from template — not used here)
    _params["sd"]        = 2;     // (inherited from template — not used here)
    _params["tz_offset"] = 2;
    _params["channels"]  = 3;     //  default

    // merge provided parameters (ports, baud, timeout, ts_key, channels, map…)
    _params.merge_patch(*(json *)params);

    // Time base (start of day minus tz_offset)
    _today = floor<chrono::days>(chrono::system_clock::now()) - std::chrono::hours(_params["tz_offset"]);

    // SerialportAcquisitor supports:
    //       - 'ports' (list) OR a single 'port'
    //       - 'baud', 'timeout'
    //       - 'ts_key' (e.g., "millis")
    //       - 'channels' (output vector dimension)
    //       - 'map' OR map_paths/map_to/map_ports
    _acq = make_unique<SerialportAcquisitor>(_params);
  }

  // Implement this method if you want to provide additional information
  map<string, string> info() override {
    // some useful info for display
    string ports = _params.contains("ports") ? _params["ports"].dump() 
                  : (_params.contains("port") ? _params["port"].get<string>() : string("[]"));
    return {
      {"Capacity",   json_to_string(_params["capacity"])},
      {"Channels",   json_to_string(_params["channels"])},
      {"Ports",      ports},
      {"TS key",     _params.value("ts_key", string(""))},
      {"TZ offset",  json_to_string(_params["tz_offset"])}
    };
  };

private:
  // Define the fields that are used to store internal resources
  unique_ptr<SerialportAcquisitor> _acq;
  chrono::time_point<chrono::system_clock, chrono::nanoseconds> _today;
};

/* ____  _             _             _      _
 |  _ \| |_   _  __ _(_)_ __     __| |_ __(_)_   _____ _ __
 | |_) | | | | |/ _` | | '_ \   / _` | '__| \ \ / / _ \ '__|
 |  __/| | |_| | (_| | | | | | | (_| | |  | |\ V /  __/ |
 |_|   |_|\__,_|\__, |_|_| |_|  \__,_|_|  |_| \_/ \___|_|
                |___/
Enable the class as plugin
*/
INSTALL_SOURCE_DRIVER(BufferedPlugin, json)

/*
                  _
  _ __ ___   __ _(_)_ __
 | '_ ` _ \ / _` | | '_ \
 | | | | | | (_| | | | | |
 |_| |_| |_|\__,_|_|_| |_|

 Optional test main (useful outside MADS for a quick smoke test).
     Safe to keep, but MADS does not require it to load the .plugin.
*/
#ifdef BUILD_STANDALONE
int main(int argc, char const *argv[]) {
  BufferedPlugin plugin;
  json output, params;

  // Minimal example for an accelerometer + microphone (4 channels)
  params["capacity"] = 100;
  params["ports"]    = json::array({"/dev/ttyACM0"});
  params["baud"]     = 1000000;
  params["timeout"]  = 50;
  params["tz_offset"]= 2;
  params["ts_key"]   = "millis";
  params["channels"] = 4;
  params["map"] = json::array({
    { {"port",0}, {"path","acceleration.x_g"},     {"to",0} },
    { {"port",0}, {"path","acceleration.y_g"},     {"to",1} },
	{ {"port",0}, {"path","acceleration.z_g"},     {"to",2} },
    { {"port",0}, {"path","sound_level"},          {"to",3} }
  });

  plugin.set_params(&params);
  plugin.get_output(output);
  cout << "Output: " << output.dump(2) << endl;
  return 0;
}
#endif
