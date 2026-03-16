# Technical Context

## Hardware Platform
- **Board**: M5Stack CoreS3
- **MCU**: ESP32-S3
- **Display**: 320x240 integrated display
- **Primary bus**: Port A I2C on `SDA=2`, `SCL=1`

## Connected Devices
- **Pa.HUB v2.1**
  - I2C address `0x70`
- **Joystick2**
  - Pa.HUB channel `1`
  - I2C address `0x63`
  - Raw registers:
    - `0x10`: X axis
    - `0x11`: Y axis
    - `0x20`: button
- **MLX90614**
  - Pa.HUB channel `5`
  - I2C address `0x5A`

## Software Stack
- **Framework**: Arduino
- **Build system**: PlatformIO
- **Graphics/UI**: LVGL `9.5.0`
- **Board support**: M5Unified, M5CoreS3
- **Sensor driver**: Adafruit MLX90614

## Current Build Configuration
The active environment is defined in `platformio.ini`:
- board: `m5stack-cores3`
- monitor speed: `115200`
- LVGL include flags enabled
- PSRAM flag enabled

## Active Timing Values
- `LV_TICK_MS = 5`
- `UI_UPDATE_MS = 80`
- `JOY_UPDATE_MS = 35`
- Temperature refresh options:
  - `35 ms`
  - `60 ms`
  - `100 ms`
  - `150 ms`

## Current Code Layout
- `src/main.cpp`: main application logic
- `include/lv_conf.h`: LVGL configuration
- `README.md`: high-level hardware and feature overview
- `memory-bank/current-firmware.md`: current architecture summary

## Known Technical Debt
- Application logic is still monolithic in `main.cpp`
- Duplicate `gauge_animation.h` files exist and are currently unused
- Legacy docs and experiments remain in the repository
- No automated tests currently cover the firmware logic
