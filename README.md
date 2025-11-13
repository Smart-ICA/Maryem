# Sensor Data Acquisition and Analysis Plugins for MADS
A modular open-source system for real-time sensor data acquisition and analysis integrated into the MADS (Multi-Agent Distributed System) framework.
This project was developed as part of the Renault Ampere – ICA Toulouse collaboration, focusing on the monitoring of CNC machining operations through current, vibration, and sound measurements.

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

├── Arduino/
│ ├── Current_Micro1_JSON.ino
│ ├── Micro2_Accelerometre_JSON.ino
│
├── Buffered_sp_plugin/
├── Filter_FFT_Acceleration/
├── Filter_FFT_Sound/
├── MongoDB_Data/
│ ├── plot_current_from_mongo.py
│ ├── plot_accelfft_from_mongo.py
│ └── plot_sound_from_mongo.py
│
├── Overpower_alerte_plugin/
├── Sink_FFT_Acceleration/
├── Sink_FFT_Sound/
├── Web_Dashboard_plugin/
│
└── README.md

---

## 🔌 1. Arduino Programs

### Description
The Arduino programs are responsible for acquiring raw data from sensors and sending it in **JSON format** through the serial port to the MADS system.

### Files
- `Current_Micro1_JSON.ino` → measures **current of the machine and sound level of the external environment**
- `Micro2_Accelerometre_JSON.ino` → measures **vibrations (x, y, z) and sound level of the machine**

### Example JSON Output
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

