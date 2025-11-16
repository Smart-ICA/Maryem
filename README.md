# MADS Sensor Acquisition & Monitoring Plugins
This repository contains a complete set of custom MADS plugins developed for real-time acquisition, processing, and visualization of industrial sensor data (current, vibration, and sound).
It includes:

- Arduino acquisition firmware

- A buffered source plugin for serial JSON streams

- FFT-based filter plugins

- GUI and alerting plugins

- A web dashboard

- Tools for plotting data from MongoDB

All plugins are written in C++ for the MADS framework and designed to run on Linux.

---

## 🧩 1. Global Description

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

---

## 🔧 3. Arduino Programs

The folder `Arduino/` contains the two independent firmwares used to acquire raw sensor data required by the MADS acquisition pipeline.

Two **Arduino Uno boards** are used simultaneously, each connected on a different serial port and streaming newline-delimited JSON (NDJSON).

---

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

* Measuring the spindle current using SCT-013 on a DFRobot Gravity V7 interface
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

---

###  Serial Communication Summary

| Arduino    | Sensors                               | Port           | Baud Rate     | File                            |
| ---------- | ------------------------------------- | -------------- | ------------- | ------------------------------- |
| **Uno 1** | Accelerations (x,y,z) + machine sound | `/dev/ttyACM0` | **1,000,000** | `Micro2_Accelerometre_JSON.ino` |
| **Uno 2** | Current, power, external sound        | `/dev/ttyACM1` | **1,000,000** | `Current_Micro1_JSON.ino`       |

---


 ## ⚙️ 4. Compilation & Installation (Linux / MADS)

All C++ plugins follow the standard MADS build procedure.

### Build

```bash
cmake -Bbuild -DCMAKE_INSTALL_PREFIX="$(mads -p)"
cmake --build build -j4
```
---

### Install

```bash
sudo cmake --install build
```
---

## 🧩 5. Plugins Overview

This repository provides functional plugins, all developed for CNC sensor monitoring.

### 5.1 Source Plugin — `buffered_sp_plugin`

**Type:** MADS *Source Plugin*

#### Purpose

Reads NDJSON data from **multiple serial ports (two Arduinos)**, buffers it, timestamps it, and publishes it to the MADS broker.

#### Features

- Multi-port acquisition  
- Batch buffering to avoid overload  
- Timestamp alignment  
- Fully configurable channel mapping  

---

#### MADS Configuration in the INI settings

The plugin supports the following settings in the INI file :

##### Source Plugin for Arduino #2 — Accelerometer + Machine Microphone

```ini
[source.buffered_sp]
ports = ["/dev/ttyACM0", "/dev/ttyACM1"]
baud = 1000000
channels = 8
ts_key = "millis"

map = [
    { port = 0, path = "I_A",              to = 0 },
    { port = 0, path = "P_W",              to = 1 },
    { port = 0, path = "sound_level",      to = 2 },

    { port = 1, path = "acceleration.x_g", to = 3 },
    { port = 1, path = "acceleration.y_g", to = 4 },
    { port = 1, path = "acceleration.z_g", to = 5 },
    { port = 1, path = "sound_level",      to = 6 }
]
```

