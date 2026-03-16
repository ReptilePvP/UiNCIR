---
Project: NCIR Monitor Device
Date: October 2025
Last Updated: 2025-10-13
---

# NCIR Monitor Memory Bank

## Status Note
This memory-bank folder is mostly historical. The active firmware in this repository is the joystick + Pa.HUB dashboard implemented in `src/main.cpp`, not the older button-driven NCIR monitor described in most files below.

Use `memory-bank/current-firmware.md` as the current source of truth before relying on the older notes.

This memory bank documents the NCIR Monitor project - an ESP32-based temperature monitoring device with MLX90614 sensor, LCD display, and hardware button interface.

## Core Requirements
- Multi-screen UI with main menu, temperature display, temperature gauge, and settings
- Hardware button navigation (no touch input)
- NCIR temperature sensor integration
- Persistent settings storage
- Audio alerts and visual indicators

## Current Project State
- Basic multi-screen UI implemented with LVGL
- MLX90614 sensor integration working
- Hardware interrupt-based button handling
- Settings persistence with Preferences library
- Temperature display and gauge screens functional
- Basic settings tabs (Units, Audio, Alerts, About)
- Audio beeps and LED alerts implemented

## Pending Implementation
- Redesign button layout and navigation logic
- Add Settings menu navigation with hardware buttons only
- Add visual highlighting/selection indicators for Settings
- Create Exit tab with Save/Cancel options
- Update main menu behavior
- Disable touch interaction throughout device
