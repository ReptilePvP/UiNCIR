# Project Brief: CoreS3 Terp Meter Dashboard

## Executive Summary
This project is a handheld temperature dashboard built on the M5Stack CoreS3 using PlatformIO, Arduino, and LVGL 9.5.0. It reads an MLX90614 infrared temperature sensor through a Pa.HUB and uses a Joystick2 as the primary input device.

The current firmware focuses on stable live temperature display, basic statistics, and a simple settings interface for units and refresh rate.

## Current Project Goal
Create a clean, responsive, joystick-driven terp meter style UI that can:
- show live object and ambient temperatures
- classify readings into practical heat zones
- provide quick navigation between screens
- stay stable on the CoreS3 with the Pa.HUB hardware chain

## Current Hardware Requirements
- **M5Stack CoreS3**
- **Pa.HUB v2.1** on Port A
- **Joystick2** on Pa.HUB channel `1`
- **MLX90614** on Pa.HUB channel `5`

## Current Software Requirements
- **PlatformIO** with Arduino framework
- **LVGL 9.5.0**
- **M5Unified**
- **Adafruit_MLX90614**

## Current UI Requirements
- Four-tab dashboard:
  - Live
  - Stats
  - Settings
  - About
- Hardware-only joystick navigation
- No touchscreen dependency
- Fast enough sensor and UI refresh to feel live on-device

## Functional Scope In Current Firmware
### Implemented
- MLX90614 initialization through Pa.HUB
- Raw I2C joystick polling through Pa.HUB
- Live object and ambient temperature display
- Temperature zone labeling and color-coding
- Stats tracking for min, max, last value, and read count
- Settings for unit toggle and sensor refresh interval
- LVGL tab-based navigation

### Not Implemented In Active Build
- Persistent settings storage
- Servo control
- Audio alerts
- LED alerts
- Battery monitoring
- Temperature graphing
- Calibration UI

## Success Criteria
- Device boots reliably into the dashboard
- Sensor reads remain stable through the Pa.HUB
- Joystick navigation feels predictable
- Live tab updates quickly enough to be useful
- Settings changes immediately affect display behavior

## Source Of Truth
The current project behavior is defined primarily by:
- `src/main.cpp`
- `README.md`
- `platformio.ini`
- `memory-bank/current-firmware.md`
