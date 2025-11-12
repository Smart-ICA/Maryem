# Sensor Data Acquisition and Analysis Plugins for MADS
A modular open-source system for real-time sensor data acquisition and analysis integrated into the MADS (Multi-Agent Distributed System) framework.
This project was developed as part of the Renault Ampere – ICA Toulouse collaboration, focusing on the monitoring of CNC machining operations through current, vibration, and sound measurements.

🧩 Overview

This repository provides:

Arduino programs for sensor data acquisition (current, vibration, and sound).

A MADS source plugin (Buffered_sp_plugin) for real-time data collection from serial ports.

Filtering and FFT plugins for frequency-domain analysis of acceleration and sound.

MongoDB scripts for data visualization and statistical study.

A Web dashboard plugin for monitoring and display of machine state and alerts in real time.

An Overpower alert plugin for detecting excessive energy consumption during machining.
