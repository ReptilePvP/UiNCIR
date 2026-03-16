# CoreS3 Terp Meter Dashboard

A **temperature dashboard and control interface** built on the **M5Stack
CoreS3** using **PlatformIO + LVGL 9.5.0**.

This project reads temperatures from an **MLX90614 infrared sensor**
connected through a **Pa.HUB v2.1**, and provides a **joystick‑driven
LVGL interface** for monitoring and settings.

The goal is to create a **portable terp‑meter style device** with fast
live readings and a polished UI.

------------------------------------------------------------------------

# Hardware

## Main Controller

-   M5Stack **CoreS3**
-   ESP32‑S3 MCU
-   320×240 display
-   LVGL GUI framework

## I2C Bus (Port A)

    SDA = GPIO 2
    SCL = GPIO 1

## Pa.HUB v2.1

Connected to **Port A**.

Address:

    0x70

### Channel Layout

  Channel   Device
  --------- -----------------------------------
  0         8Servo Unit (reserved for future)
  1         Joystick2
  5         MLX90614 temperature sensor

------------------------------------------------------------------------

# Devices

## Joystick2

Address:

    0x63

Registers:

    0x10 → X axis
    0x11 → Y axis
    0x20 → Button

Button values:

    0 = pressed
    1 = released

The joystick is read directly over **raw I2C** instead of using a
library.

------------------------------------------------------------------------

## MLX90614 Infrared Temperature Sensor

Address:

    0x5A

Library used:

    Adafruit_MLX90614

Functions:

    mlx.begin(addr, &Wire)
    mlx.readObjectTempF()
    mlx.readAmbientTempF()

------------------------------------------------------------------------

# Software Stack

Framework:

    Arduino (PlatformIO)

Libraries:

    M5Unified
    M5GFX
    LVGL 9.5.0
    Adafruit_MLX90614
    Adafruit BusIO

------------------------------------------------------------------------

# UI Layout

The UI uses **LVGL TabView** navigation.

Tabs:

1️⃣ **Live**\
2️⃣ **Stats**\
3️⃣ **Settings**\
4️⃣ **About**

### Navigation

Joystick controls:

    Left / Right → change tab
    Up / Down → adjust settings
    Joystick press → toggle units

------------------------------------------------------------------------

# Live Tab

Displays:

-   Object temperature
-   Ambient temperature
-   Temperature zone indicator
-   Temperature progress bar

Temperature zones:

  Temp (F)    Zone
  ----------- ---------
  \<350°F     LOW
  350‑450°F   READY
  450‑550°F   HOT
  \>550°F     TOO HOT

------------------------------------------------------------------------

# Stats Tab

Shows session statistics:

-   Minimum temperature
-   Maximum temperature
-   Last reading
-   Total successful sensor reads

------------------------------------------------------------------------

# Settings Tab

Configurable options:

-   Temperature units (F / C)
-   Sensor refresh rate

Refresh options:

    35 ms
    60 ms
    100 ms
    150 ms

Default:

    60 ms

------------------------------------------------------------------------

# About Tab

Displays basic device info:

    CoreS3
    PaHub
    Joystick2
    MLX90614
    LVGL dashboard

------------------------------------------------------------------------

# Current Working State

✔ PlatformIO build successful\
✔ MLX90614 temperature readings working\
✔ PaHub channel switching working\
✔ Joystick navigation working\
✔ LVGL UI stable

Servo support temporarily removed to stabilize sensor timing.

------------------------------------------------------------------------

# Planned Features

## UI Improvements

-   Larger temperature display
-   Temperature history graph
-   Status icons
-   Highlighted settings navigation

## Hardware Features

-   Re‑add **360° servo control**
-   Optional **audio/haptic alerts**
-   Calibration tools

## Advanced Features

-   Real‑time temperature graph
-   Adjustable zone thresholds
-   Persistent settings storage

------------------------------------------------------------------------

# Project Structure

    project/
    │
    ├─ platformio.ini
    ├─ include/
    │   └─ lv_conf.h
    ├─ src/
    │   └─ main.cpp
    │
    ├─ README.md
    └─ .codex/
       ├─ project-context.md
       ├─ hardware-map.md
       ├─ lvgl-ui-map.md
       ├─ roadmap.md
       └─ i2c-debugging.md

------------------------------------------------------------------------

# Codex Integration

The `.codex` folder contains structured documentation so AI tools can:

-   understand the hardware
-   understand the UI architecture
-   debug I2C devices
-   extend the firmware safely

This allows the project to be **AI‑maintainable** and easier to extend.

------------------------------------------------------------------------

# Goal

Create a **clean, portable terp‑meter style device** with:

-   fast live temperature readings
-   joystick navigation
-   LVGL graphical dashboard
-   optional automation features
