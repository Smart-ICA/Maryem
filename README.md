# Sensor Data Acquisition and Analysis Plugins for MADS
A modular open-source system for real-time sensor data acquisition and analysis integrated into the MADS (Multi-Agent Distributed System) framework.
This project was developed as part of the Renault Ampere – ICA Toulouse collaboration, focusing on the monitoring of CNC machining operations through current, vibration, and sound measurements.

---

## 🧩 Overview

This repository provides:
- **Arduino programs** for sensor data acquisition (current, vibration, and sound).  
- A **MADS source plugin** (`Buffered_sp_plugin`) for real-time data collection from serial ports.  
- **Filtering and FFT plugins** for frequency-domain analysis of acceleration and sound.  
- **MongoDB scripts** for data visualization and statistical study.  
- A **Web dashboard plugin** for monitoring and display of machine data in real time.  
- An **Overpower alert plugin** for detecting excessive energy consumption during machining.

---

## 🔧 Hardware Setup

- **Arduino Uno Wifi** board  
- **Sensors:**
  - DFRobot Gravity Analog AC Current sensor: *SCT-013-020*  
  - Accelerometer: *MMA7660FC, 3axis*  
  - Microphone: *Analog sound sensor (LM2904)*  
- **CNC machine** (industrial environment)
- **MADS system** running on **Linux**
- **MongoDB** database for real-time data storage

---

## 💡 Features

- Acquisition of multi-sensor data (current, vibration, sound) via serial JSON streams  
- Data buffering and batch transmission using the **Buffered Source Plugin**  
- Frequency analysis using **FFT filters**  
- Detection of abnormal machine behavior (via Overpower Alert Plugin)  
- Real-time visualization with Python and MongoDB tools  
- Web dashboard for monitoring and visualization in real time.

---

## 🚀 Quick Start

### 1. Upload Arduino Programs

1. Open one of the `.ino` files in `/Arduino/`:
   - `Current_Micro1_JSON.ino` → sends current data  
   - `Micro2_Accelerometre_JSON.ino` → sends acceleration and sound data  
2. Upload the code to the Arduino board using the Arduino IDE.

---

### 2. Build and Load the MADS Plugins

1. Compile the plugin source files located in each `/plugin_name/` folder (e.g., `Buffered_sp_plugin`, `Filter_FFT_Acceleration`, etc.).  
2. Install the plugin into the MADS source directory:
   ```bash
   sudo cmake --install .
