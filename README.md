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


---

## 🔌 1. Arduino Programs

The folder `Arduino/` contains the firmware used to acquire raw sensor data.

---

### 🧩 Available Firmwares

| File | Description |
|------|--------------|
| [`Current_Micro1_JSON.ino`](Arduino/Current_Micro1_JSON.ino) | Reads current of the machine and sound level of the external environment and outputs NDJSON |
| [`Micro2_Accelerometre_JSON.ino`](Arduino/Micro2_Accelerometre_JSON.ino) | Reads accelerations (x, y, z) and sound level of the machine and outputs NDJSON |

---

### 📤 Output Format

Arduino sends **newline-delimited JSON (NDJSON)**:

```json
{
  "millis": 158426,
  "acceleration": { "x_g": 0.27, "y_g": -0.98, "z_g": 0.03 },
  "sound_level": 69,
  "I_A": 1.42,
  "P_W": 533.68,
}

---

🧠 2. Buffered_sp_plugin (Source Plugin)
Description
This plugin is an extension of the original Buffered plugin by Prof.Paolo Bosetti.
It collects sensor data (current, acceleration, sound) from Arduino serial ports in NDJSON format and sends it to MADS as batched messages to reduce database overload.

Type
➡️ Source Plugin

---

