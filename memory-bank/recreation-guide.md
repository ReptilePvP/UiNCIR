# Project Recreation Guide

## Purpose
This guide describes how to recreate the current joystick-based CoreS3 temperature dashboard, not the older button-driven NCIR monitor design.

## Required Hardware
- M5Stack CoreS3
- Pa.HUB v2.1
- Joystick2
- MLX90614 infrared temperature sensor

## Wiring / Topology
Connect the Pa.HUB to Port A on the CoreS3.

Attach:
- Joystick2 to Pa.HUB channel `1`
- MLX90614 to Pa.HUB channel `5`

The active firmware expects:
- `SDA = GPIO 2`
- `SCL = GPIO 1`

## Required Software
- PlatformIO
- Arduino framework

## PlatformIO Configuration
Use the repository `platformio.ini` as the reference configuration. Important dependencies include:
- `M5CoreS3`
- `M5Unified`
- `M5UnitUnified`
- `M5Unit-HUB`
- `Adafruit BusIO`
- `lvgl`
- `Adafruit MLX90614`

## Core Recreation Steps
1. Create a PlatformIO project for board `m5stack-cores3`.
2. Copy the `platformio.ini` configuration.
3. Add `include/lv_conf.h`.
4. Implement the CoreS3 display bridge for LVGL.
5. Implement Pa.HUB channel selection on I2C address `0x70`.
6. Implement raw Joystick2 register reads on address `0x63`.
7. Initialize the MLX90614 on address `0x5A` after selecting channel `5`.
8. Build a five-tab LVGL UI for Live, Stats, Settings, Alerts, and Calibration.
9. Add a timed main loop for LVGL ticks, joystick polling, sensor reads, and UI refresh.

## Behavioral Notes
- Left/right joystick movement changes tabs.
- Up/down adjusts refresh interval only on Settings.
- Button press toggles Fahrenheit/Celsius only on Settings.
- Zone classification is based on Fahrenheit thresholds.

## Files To Use As Reference
- `src/main.cpp`
- `README.md`
- `memory-bank/current-firmware.md`

## Files Not To Use As Current Design Reference
- older memory-bank entries describing hardware buttons, alerts, preferences, or servo workflows
- `src/Old Work verison with temp.md`
