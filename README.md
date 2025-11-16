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

## 🧩 Global Description

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

## 🧱 Project Structure

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

# 🔧 1. Arduino Programs

The folder `Arduino/` contains the two independent firmwares used to acquire raw sensor data required by the MADS acquisition pipeline.

Two **Arduino Uno boards** are used simultaneously, each connected on a different serial port and streaming newline-delimited JSON (NDJSON).

---

## 🛰️ Arduino Uno #1 – Accelerations + Machine Sound

🔌 **Serial Port:** `/dev/ttyACM0`
📟 **Firmware:** `Micro2_Accelerometre_JSON.ino`

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

## 🛰️ Arduino Uno #2 – Current, Power & External Sound

🔌 **Serial Port:** `/dev/ttyACM1`
📟 **Firmware:** `Current_Micro1_JSON.ino`

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

## 📡 Serial Communication Summary

| Arduino    | Sensors                               | Port           | Baud Rate     | File                            |
| ---------- | ------------------------------------- | -------------- | ------------- | ------------------------------- |
| **Uno #1** | Accelerations (x,y,z) + machine sound | `/dev/ttyACM0` | **1,000,000** | `Micro2_Accelerometre_JSON.ino` |
| **Uno #2** | Current, power, external sound        | `/dev/ttyACM1` | **1,000,000** | `Current_Micro1_JSON.ino`       |

---


🧠 2. Buffered_sp_plugin (Source Plugin)
Description
This plugin is an extension of the original Buffered plugin by Prof.Paolo Bosetti.
It collects sensor data (current, acceleration, sound) from Arduino serial ports in NDJSON format and sends it to MADS as batched messages to reduce database overload.

Type
➡️ Source Plugin

---




