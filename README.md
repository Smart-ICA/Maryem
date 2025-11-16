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
