# MADS Sensor Acquisition & Monitoring Plugins
---
This repository contains a complete set of custom MADS plugins developed for real-time acquisition, processing, and visualization of industrial sensor data (current, vibration, and sound).
It includes:

- Arduino acquisition firmware

- A buffered source plugin for serial JSON streams

- FFT-based filter plugins

- GUI and alerting plugins

- A web dashboard

- Tools for plotting data from MongoDB

All plugins are written in C++ for the MADS framework and designed to run on Linux.


## 🧩 1. Global Description
---
The goal of this project is to build an **open-source and modular system** that acquires data from physical sensors, processes it in real time, and stores it for later analysis.

### The complete data chain: 
1. **Arduino Uno Wifi** board  
2. **Sensors:**
  - DFRobot Gravity Analog AC Current sensor: *SCT-013-020*  
  - Accelerometer: *MMA7660FC, 3axis*  
  - Microphone: *Analog sound sensor (LM2904)* 
3. **MADS Source Plugin (Buffered_sp_plugin)** : real-time data collection via serial ports  
4. **Processing Plugins** : FFT and filtering
5. **MongoDB** : database for real-time data storage 
6. **Sinks** : overpower alert detection and web dashboard  
7. **Visualization Tools** : plot data using Python language
    
---

## 🧱 2. Project Structure
---
```text
├── Arduino/                       # Arduino firmwares (current, accelerometer, sound)
├── Buffered_sp_plugin/            # Source plugin for reading NDJSON sensor streams
├── Filter_FFT_Acceleration/       # Filter plugin computing FFT of vibration signals
├── Filter_FFT_Sound/              # Filter plugin computing FFT of microphone signals
├── MongoDB_Data/                  # Python tools for plotting MongoDB data
├── Overpower_alerte_plugin/       # Sink plugin sending alert notifications
├── Sink_FFT_Acceleration/         # Sink plugin visualizing vibration FFT
├── Sink_FFT_Sound/                # Sink plugin visualizing sound FFT
├── Web_Dashboard_plugin/          # Web-based dashboard for real-time monitoring
└── README.md
```

## 🔧 3. Arduino Programs
---
The folder `Arduino/` contains the two independent firmwares used to acquire raw sensor data required by the MADS acquisition pipeline.

Two **Arduino Uno boards** are used simultaneously, each connected on a different serial port and streaming newline-delimited JSON (NDJSON).

### 🛰️ Arduino Uno 1 – Accelerations + Machine Sound

🔌 **Serial Port:** `/dev/ttyACM0`

📄 **File:** `Micro2_Accelerometre_JSON.ino`

This Arduino reads:

* 3-axis accelerations of the CNC machine
* Sound level measured directly on the machine structure

It packages the measurements into the following JSON frame:

```json
{
  "millis": ...,
  "acceleration": { "x_g": ..., "y_g": ..., "z_g": ... },
  "sound_level": ...
}
```

**Fields:**

* `millis` → Arduino internal timestamp
* `acceleration.x_g / y_g / z_g` → vibrations in g
* `sound_level` → machine acoustic vibration level

---

### 🛰️ Arduino Uno 2 – Current, Power & External Sound

🔌 **Serial Port:** `/dev/ttyACM1`

📄 **File:**  `Current_Micro1_JSON.ino`

This Arduino is dedicated to:

* Measuring the spindle current using DFRobot analog current sensor SCT-013 20A 
* Computing instantaneous power using
  `P = √3 × U × I × cosφ`, with `U = 250 V`, `cosφ ≈ 0.85`
* Recording the external environmental sound level near the CNC machine

It sends JSON frames of the form:

```json
{
  "millis": ...,
  "I_A": ...,
  "P_W": ...,
  "sound_level": ...
}
```

**Fields:**

* `I_A` → RMS spindle current (A)
* `P_W` → computed electrical power (W)
* `sound_level` → external acoustic level near the CNC


###  Serial Communication Summary

| Arduino    | Sensors                               | Port           | Baud Rate     | File                            |
| ---------- | ------------------------------------- | -------------- | ------------- | ------------------------------- |
| **Uno 1** | Accelerations (x,y,z) + machine sound | `/dev/ttyACM0` | **1,000,000** | `Micro2_Accelerometre_JSON.ino` |
| **Uno 2** | Current, power, external sound        | `/dev/ttyACM1` | **1,000,000** | `Current_Micro1_JSON.ino`       |

---


 ## ⚙️ 4. Compilation & Installation (Linux / MADS)
---
All C++ plugins follow the standard MADS build procedure.

### Build

```bash
cmake -Bbuild -DCMAKE_INSTALL_PREFIX="$(mads -p)"
cmake --build build -j4
```

### Install

```bash
sudo cmake --install build
```

## 🧩 5. Plugins Overview
---
This repository provides functional plugins, all developed for CNC sensor monitoring.

### 5.1 Source Plugin — `buffered_sp_plugin`

**Type :** MADS *Source Plugin*

#### Purpose

Reads NDJSON data from **multiple serial ports (two Arduinos)**, buffers it, timestamps it, and publishes it to the MADS broker.

#### Features

- Multi-port acquisition  
- Batch buffering to avoid overload  
- Timestamp alignment  
- Fully configurable channel mapping  

#### MADS Configuration in the INI settings

The plugin supports the following settings in the INI file :

##### Source Plugin for Arduino 1 — Accelerometer + Machine Microphone

```ini
[buffered_accel_mic]
pub_topic = "accel_mic"
capacity  = 1000
ports = ["/dev/ttyACM0"]
baud = 1000000
timeout = 50
ts_key = "millis"
channels = 4

map_paths = [
  "acceleration.x_g",
  "acceleration.y_g",
  "acceleration.z_g",
  "sound_level"
]

map_to    = [0, 1, 2, 3]
map_ports = [0, 0, 0, 0]

```
##### Source Plugin for Arduino 2 — Current + Environmental Microphone
```ini
[buffered_currents_full]
pub_topic = "currents_full"
capacity  = 1000
ports = ["/dev/ttyACM1"]
baud = 1000000
timeout = 50
ts_key = "millis"
channels = 3

map_paths = [
  "I_A",
  "P_W",
  "sound_level"
]

map_to    = [0, 1, 2]
map_ports = [0, 0, 0]
```

**pub_topic :** Topic where sensor measurements are published.

**capacity :** Maximum number of rows the buffer can store before sending.

**ports :** Serial ports assigned to the plugin.  

**baud :** Baud rate that must match the Arduino sketch (`1,000,000` baud).

**timeout :** Timeout used for serial reading.

**ts_key :** Timestamp key extracted from Arduino JSON messages.

**channels :** Number of numeric data channels.

**map_paths :** JSON paths extracted from the incoming NDJSON stream.

**map_to :** Maps each extracted value to an internal buffer index.

**map_ports :** Serial port associated with each mapped value.  

#### Run

The plugin can be launched with this command line :

```bash
mads source buffered_sp.plugin -n buffered_sp
```

### 5.2 Filter Plugins — `accel_fft` & `sound_fft`

**Type :** MADS *Filter Plugins*

#### Plugins included
- `accel_fft` : real-time FFT for acceleration (X/Y/Z axes)
- `sound_fft` : real-time FFT for machine sound level

#### Purpose

These two filter plugins compute the **Fast Fourier Transform (FFT)** of acceleration and sound signals in real time.

This processing converts raw signals into the frequency domain, making it possible to detect:

- dominant mechanical frequencies
- harmonics linked to spindle rotation
- abnormal peaks (possible defects)
- differences between machining operations

The two plugins share the same operating principles, with different input signals.

#### Features

- Sliding-window FFT
- Configurable sampling frequency and window size
- Automatic band extraction 
- Threshold-based peak detection
- Alarm integration via GUI sinks
- Designed for machining diagnostics


#### MADS Configuration in the INI Settings

The plugins support the following settings in the `mads.ini` file :

##### Acceleration FFT filter Plugin — `accel_fft`

```ini
[accel_fft]
sub_topic = ["Ampere"]
axis = "x"              
fs        = 500          
win_size  = 256         
f_min     = 10
f_max     = 250
threshold = 0.5
confirm_windows = 2    
```

##### Sound FFT filter Plugin — `sound_fft`

```ini
[sound_fft]
sub_topic = ["arduino"]
fs        = 500
win_size  = 256
f_min     = 0
f_max     = 250
threshold = 0.25
confirm_windows = 2
```

**sub_topic :** Topic containing the sensor data of sound and accelerations : Topic listens by the plugin (Ampere and arduino).

**axis :** Axis of acceleration to process (x, y, z).

**fs :** Expected sampling frequency output of the Arduino.

**win_size :** Number of samples per FFT computation.

**f_min / f_max :** Frequency band kept for analysis.

**threshold :** Minimum amplitude that considers a frequency peak significant.

**confirm_windows :** Number of consecutive FFT windows exceeding the threshold before reporting.

#### Run

The plugins can be launched with this command lines :

```bash
mads filter accel_fft.plugin -n accel_fft
mads filter sound_fft.plugin -n sound_fft
```


### 5.3 Sink Plugins — `accel_fft_alarm_gui` & `sound_fft_alarm_gui`

**Type :** MADS *Sink Plugins*

#### Plugins included
- `accel_fft_alarm_gui` : real-time display of FFT for acceleration (X/Y/Z axes)
- `sound_fft_alarm_gui` : real-time display of FFT for machine sound level


#### Purpose

These GUI sink plugins visualize the results of the FFT filters and generate graphical alarms when a peak is detected.

They provide:

- real-time FFT display
- threshold-based alarm highlighting
- optional fullscreen mode

#### Features

- Live FFT visualization
- Alarm indicators
- Simple GUI for operators
- Fully configurable Python backend


#### MADS Configuration in the INI Settings

The plugins support the following settings in the `mads.ini` file :

##### Acceleration FFT Alarm GUI — accel_fft_alarm_gui

```ini
[accel_fft_alarm_gui]
sub_topic   = ["accel_fft"]
python_path = "/path/to/venv/bin/python3"
script_path = "/path/to/gui_sound_fft.py"
title       = "FFT Accélération – Monitoring"   
```


##### Sound FFT Alarm GUI — sound_fft_alarm_gui

```ini
[sound_fft_alarm_gui]
sub_topic        = ["sound_fft"]
python_path      = "/path/to/venv/bin/python3"
script_path      = "/path/to/gui_sound_fft.py"
state_path       = "/tmp/sound_fft_gui_state.json"
title            = "FFT Son – Monitoring"
fullscreen       = true
f_min            = 0
f_max            = 4000
```

**sub_topic :** Topic produced by the associated FFT filter plugin.

**python_path :** Path to the Python virtual environment used to launch the GUI.

**script_path :** Script that displays the FFT graph.

**title :** Title of the GUI window.

**fullscreen :** Whether to display the GUI in fullscreen.

**f_min / f_max :** GUI display band selection.


#### Run

The plugins can be launched with this command lines :

```bash
mads sink accel_fft_alarm_gui.plugin -n accel_fft_alarm_gui
mads sink sound_fft_alarm_gui.plugin -n sound_fft_alarm_gui
```

### 5.4 Sink Plugin — `web_dashboard`

**Type :** MADS *Sink Plugin*

#### Plugin included
- `web_dashboard`


#### Purpose

The web_dashboard plugin provides a real-time web-based interface to visualize sensor data coming from the two Arduinos.
It creates a lightweight HTTP server that continuously refreshes numerical values, allowing an operator to monitor:

- spindle current
- estimated power
- machine vibration (x, y, z)
- machine sound level

This dashboard is accessible from any browser connected to the same network.

#### Features

- Real-time live monitoring
- Auto-refresh using a configurable refresh rate
- Custom HTML/CSS/JS interface
- Simple deployment on any Linux machine (Raspberry Pi, server, etc...)
- Fully configurable from the mads.ini file
- Displays data from both Arduino sources in a clean web UI

#### MADS Configuration in the INI Settings

The plugin support the following settings in the `mads.ini` file :

```ini
[web_dashboard]
sub_topic = ["Mytopic"]
http_host  = "0.0.0.0"
http_port  = 8088
title      = "Monitoring Capteurs – Ampere"
refresh_ms = 500
static_dir = "/path/to directory containing static files (CSS, JS)"
```

**sub_topic :** Topic to listen to (sensor messages coming from the current sensor + accelerometer + microphone Arduinos).

**http_host :** IP interface on which the web server runs.
"0.0.0.0" = accessible from any device on the network.

**http_port :** Port of the dashboard (e.g., open browser at http://<machine-ip>:8088).

**title :** Title displayed at the top of the web dashboard.

**refresh_ms :** Refresh interval in milliseconds.

**static_dir :** Folder containing CSS and optional JS assets.


#### Run

Launch the dashboard from a terminal:
```bash
mads sink web_dashboard.plugin -n web_dashboard
```
Then open your browser at:
```bash
http://localhost:8088
```
Or from another device on the same network:
```bash
http://<your-machine-ip>:8088
```

### 5.5 Sink Plugin — `overpower_email`

**Type :** MADS *Sink Plugin*

#### Plugin included
- `overpower_email`

#### Purpose

The  `overpower_email` plugin monitors the estimated spindle power coming from the Arduino current-sensor stream and automatically sends an email alert whenever the power exceeds a predefined threshold.

In addition to email notifications, the plugin also launches a GUI alarm window displaying the alert and playing a warning sound.

This plugin is intended to warn operators of abnormal cutting power, possible tool issues, overload conditions, or safety-related events.

#### Features

- Real-time power monitoring from the Ampere topic
- Automatic email alerts when exceeding a threshold
- Minimum interval between alerts (to avoid spamming)
- GUI alert window for on-machine supervision
- Optional audio alarm (Linux speaker-test)
- Historical logging of all alerts in JSONL
- Fully configurable via mads.ini

#### Google API Authentication (Gmail OAuth2)

The `overpower_email` plugin sends alert emails using the Gmail API, which requires an OAuth2 authentication step.
The setup is done once, and the token is then reused automatically.

##### Summary of the Authentication Process 
- You must create a Google Cloud project and enable the Gmail API.
- Generate OAuth2 credentials (“OAuth Client ID”) and download the file `credentials.json`.
- Place `credentials.json` in the same folder as `email_alert`.py.
- On the first execution of the plugin, a browser window will open asking you to authorize the application.
- After successful authentication, a file `token.json` is created and stored locally.
This token is reused for all subsequent email sending operations, so you don’t need to authenticate again.

##### Python Environment
We recommend using a dedicated virtual environment:

```bash
python3 -m venv venv
source venv/bin/activate
pip install numpy # also install other necessary Python libs
```
Configure this Python interpreter in `mads.ini` using:

```bash
python_path    = "/path/to/venv/bin/python3"
```
This completes the Gmail API setup for secure automated alert emails.

#### MADS Configuration in the INI Settings

The plugin support the following settings in the `mads.ini` file :

```ini
[overpower_email]
sub_topic = ["Ampere"]
threshold_W = 20
min_alert_interval_s = 300
to_email = "recipient@gmail.com"
machine_name = "Tour CN MFJA"
python_path      = "/path/to/venv/bin/python3"
script_path      = "/path/to/email.alert.py"
gui_python_path = "/path/to/venv/bin/python3"
gui_script_path = "/path/to/gui_overpower_alert.py"
gui_fullscreen = true
gui_beep = true
gui_beep_backend = "speaker-test"
gui_beep_interval_ms = 700
gui_timeout_s = 0   
history_path = "/path/to/alerts_history.jsonl"
```

**sub_topic :** Topic where the plugin listens for incoming power values (from the Arduino “current + microphone” stream).

**threshold_W :** Power threshold (in Watts) above which an alert is triggered.

**min_alert_interval_s :** Minimum number of seconds between two email alerts (anti-spam protection).

**to_email :** Recipient address for alert emails.

**machine_name :** Displayed in emails and GUI as the monitored machine.

**python_path :** Python interpreter for the email alert script (virtual environment recommended).

**script_path :** Path to the Python script used to send email notifications.

**gui_python_path :** Python interpreter used for the GUI alert window.

**gui_script_path :** Path to the GUI script that displays alerts in real time.

**gui_fullscreen :** If true, the alert GUI opens full screen.

**gui_beep :** Enables acoustic alarm.

**gui_beep_backend :** Command used for generating sound (Linux-compatible).

**gui_beep_interval_ms :** Interval between audio beeps.

**gui_timeout_s :** Auto-close timeout (0 = never closes).

**history_path :** File where all alerts are stored in JSON Lines format.

#### Run

The plugins can be launched with this command lines :
```bash
mads sink overpower_email.plugin -n overpower_email
```
The GUI window starts automatically when power exceeds the threshold.

### 5.6 Python Tools for Offline Data Visualization (MongoDB_Data)
The folder MongoDB_Data/ contains Python scripts used to load, process, and plot sensor data stored in MongoDB during machining operations.
These tools are essential for analyzing vibration, current consumption, and sound signals collected during the three machining operations (rough dressing, finishing, drilling).

#### 📂 Available Scripts

| File                      | Description                                                                                     |
|--------------------------|-------------------------------------------------------------------------------------------------|
| `plot_accelfft_from_mongo.py` | Loads accelerometer data from MongoDB, computes FFT, and plots vibration spectra (X/Y/Z axes). |
| `plot_current_from_mongo.py`  | Extracts current and/or estimated power over time from the database and plots the spindle electrical behavior. |
| `plot_sound_from_mongo.py`    | Loads microphone data and plots sound level or FFT spectrum for acoustic analysis during machining. |


#### 🔧 Requirements
All scripts require:

```bash
pip install pymongo matplotlib numpy scipy
```

#### 📁 Data Source

The scripts read data from:

- **The MongoDB collection** populated by the MADS plugins (from `buffered_sp` plugin),
- Data recorded during machining of **an aluminum cylindrical part** using three different operations:
  - Rough dressing (*dressage ébauche*)
  - Finishing (*dressage finition*)
  - Drilling (*perçage*)
  - Optional fourth operation: radius machining (*usinage rayon*)

Each document contains numerical batches of samples acquired in real time from Arduino.


#### ▶️ How to Run Each Script

##### 1. Plot accelerometer FFT

```bash
python3 plot_accelfft_from_mongo.py
````

**Produces:**

* FFT spectrum for **X, Y, Z axes**
* Identification of dominant vibration frequencies
* Useful for chatter detection and machine condition monitoring

##### 2. Plot current and power

```bash
python3 plot_current_from_mongo.py
```

**Produces:**

* Spindle current curve
* Power curve computed from the formula:

`P = √3 × U × I × cosφ`

* Comparison against thresholds or machine logs

##### 3. Plot sound spectrum

```bash
python3 plot_sound_from_mongo.py
```

**Produces:**

* Acoustic signal spectrum
* Identification of dominant noise frequencies
* Helps correlate sound anomalies with vibration events

#### 📊 Output

All graphs are saved automatically in `/tmp/` or in the script’s configured output folder.

They can be used directly for diagnostics or machine learning preprocessing.

```
## 🤝 Project collaboration

This work is part of a final year project (PFE) carried out within the ICA laboratory.

It is part of a collaborative project with the company  Renault Ampere.

## 🏭 Contribution

Author: Maryem LHAMYANI (INSA Toulouse)

Contributors: Anna-Carla Araujo (INSA Toulouse - ICA) & Paolo BOSETTI (University of Trento)
