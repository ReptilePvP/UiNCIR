# NCIR Monitor for M5Stack CoreS3

## Overview

This project is a handheld NCIR monitor built for the M5Stack CoreS3 using an MLX90614 non-contact infrared temperature sensor, LVGL-based on-screen UI, hardware button navigation, and a Pa.HUB 2.1v expansion path for external I2C devices.

The application is designed around hardware-first operation:

- real-time NCIR temperature measurement
- a simple main menu and dedicated view screens
- persistent settings stored in ESP32 preferences
- audible and visual alerting
- expansion through Pa.HUB for routed I2C peripherals
- a joystick-driven continuous-servo control mode

The current codebase lives primarily in [src/main.cpp](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/src/main.cpp) and builds with PlatformIO for the `m5stack-cores3` board.

## Current Feature Set

- Main menu with hardware-button navigation
- Numeric temperature display screen
- Analog-style temperature gauge screen
- Settings screen with persistent preferences
- MLX90614 emissivity write/apply flow
- Pa.HUB 2.1v I2C routing through PortA
- Continuous-servo control screen using a joystick on the Pa.HUB
- Battery level monitoring
- Audio feedback and temperature alerting
- Debug logging support over serial

## Hardware Architecture

### Core device

- M5Stack CoreS3
- MLX90614 NCIR Unit
- Pa.HUB 2.1v connected to PortA

### Current routed peripherals

The software currently assumes this Pa.HUB channel layout:

| Pa.HUB channel | Device | Purpose |
|---|---|---|
| 0 | MLX90614 NCIR Unit | Temperature measurement |
| 1 | Joystick2 Unit | X-axis input for servo control |
| 2 | PbHub | Continuous-servo output |

### Servo path

The servo integration now targets a continuous-rotation servo rather than a positional 0-180 degree servo.

The control model is:

- `90` = stop
- `< 90` = CCW
- `> 90` = CW

The current code assumes:

- Pa.HUB channel `2` contains a PbHub
- PbHub port `0`, servo index `0` is connected to the continuous servo

If your wiring differs, update the constants near the top of [src/main.cpp](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/src/main.cpp):

- `NCIR_HUB_CHANNEL`
- `JOYSTICK_HUB_CHANNEL`
- `SERVO_HUB_CHANNEL`
- `PBHUB_PORT_CHANNEL`
- `PBHUB_SERVO_INDEX`

## Software Stack

- Arduino framework
- PlatformIO
- M5CoreS3
- M5Unified
- M5GFX
- LVGL
- Adafruit MLX90614 library
- M5UnitUnified
- M5Unit-HUB
- M5Unit-PbHub
- M5Unit-Joystick2

Library dependencies are defined in [platformio.ini](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/platformio.ini).

## I2C Routing Design

Originally, the MLX90614 was connected directly to PortA. The current architecture routes PortA I2C through the Pa.HUB:

```text
CoreS3 PortA -> Pa.HUB 2.1v -> channel-selected I2C peripheral
```

This means the application must select the correct Pa.HUB channel before communicating with any routed device.

### Important behavior

- `Wire` is reconfigured onto the CoreS3 PortA SDA/SCL pins during startup
- `UnitUnified` manages the Pa.HUB itself
- the code explicitly selects the NCIR channel before MLX90614 init
- the code re-selects the NCIR channel before each temperature read
- the code selects the joystick and servo channels before those operations
- `Units.update()` runs in `loop()` to keep hub-managed communication active

## Screen Flow

The app uses a simple state-driven screen system.

### Screens

- `SCREEN_MAIN_MENU`
- `SCREEN_TEMP_DISPLAY`
- `SCREEN_TEMP_GAUGE`
- `SCREEN_SERVO`
- `SCREEN_SETTINGS`

### Main menu entries

- Thermometer
- Gauge
- Servo
- Settings

### Input model

The app is intended for hardware-button use rather than touch interaction.

- Button 1: move backward/up through menu choices
- Button 2: move forward/down through menu choices
- Key button: enter or confirm

The servo screen additionally reads the joystick X-axis and maps it to a continuous-servo command range.

## Continuous Servo Mode

The servo mode is a live control screen that reads the joystick and writes a speed command to the servo.

### Control logic

- joystick X is read from the Joystick2 unit
- a small dead-zone around center reduces jitter
- normalized joystick input is mapped to a command from `0` to `180`
- that command is sent through the PbHub servo API
- the UI updates only when the value changes meaningfully

### Display behavior

The servo screen shows:

- motion state text such as `STOPPED`, `CCW 18`, or `CW 24`
- a horizontal bar representing the current command
- status text showing the Pa.HUB and PbHub routing path

### Control convention

The implementation now matches the pattern from your other project:

```cpp
90 = stop
<90 = CCW
>90 = CW
```

## Temperature Measurement Path

The temperature logic still uses the Adafruit MLX90614 driver, but it is now hub-aware.

### Temperature update flow

1. Select Pa.HUB channel for the NCIR unit
2. Read object temperature
3. Re-select Pa.HUB channel if needed
4. Read ambient temperature
5. Update active screen widgets
6. Check alert state

### Temperature screens

#### Numeric display

- large object temperature value
- ambient temperature value
- unit label
- status label
- color-coded object temperature

#### Gauge display

- analog-style scale
- moving needle
- smoothed value label

## Settings and Persistence

Settings are stored with the ESP32 Preferences API.

### Current settings handled by the application

- temperature unit selection
- display brightness
- sound enable/disable
- sound volume
- alert enable/disable
- alert temperature threshold
- debug logging
- battery alert settings
- emissivity value

### Emissivity support

The settings flow includes an emissivity page that:

- shows the current emissivity
- allows adjustment in `0.01` steps
- writes the new value to the MLX90614
- restarts the device after writing

Because the MLX90614 is behind the Pa.HUB, the application selects the NCIR hub channel before performing the emissivity write.

## File Layout

### Primary files

- [platformio.ini](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/platformio.ini): PlatformIO environment and library dependencies
- [src/main.cpp](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/src/main.cpp): main application logic, UI, hardware integration, sensor reads, servo mode
- [include/lv_conf.h](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/include/lv_conf.h): LVGL configuration
- [memory-bank/projectbrief.md](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/memory-bank/projectbrief.md): project summary and goals
- [memory-bank/techContext.md](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/memory-bank/techContext.md): technical notes and stack context

### Code organization inside `main.cpp`

The main source file is organized roughly as:

- includes
- global configuration and state
- setup
- main loop
- hardware setup helpers
- preferences helpers
- screen creation and screen switching
- sensor update routines
- battery and alert logic
- servo helper functions
- event handlers

## Build and Run

### Prerequisites

- PlatformIO installed
- USB connection to the CoreS3

### Build

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
```

### Upload

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload
```

### Serial monitor

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor
```

The project monitor speed is `115200`.

## Verified Status

At the time of this document update, the project builds successfully with:

```powershell
platformio run
```

The current successful integration includes:

- Pa.HUB 2.1v routing
- NCIR channel selection before sensor access
- `Units.update()` in the main loop
- Joystick2 integration
- PbHub continuous-servo control
- servo screen added to the menu and UI state machine

## Known Assumptions

These are the most important implementation assumptions currently baked into the code:

1. The Pa.HUB 2.1v is at I2C address `0x70`.
2. The MLX90614 is connected on Pa.HUB channel `0`.
3. The Joystick2 is connected on Pa.HUB channel `1`.
4. A PbHub is connected on Pa.HUB channel `2`.
5. The continuous servo is connected to PbHub port `0`, servo output `0`.
6. PortA is the active I2C bus for all routed external devices.

If any of those differ on your bench setup, update the constants in [src/main.cpp](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/src/main.cpp) before deploying.

## Recommended Test Checklist

### Hub and sensor

- Confirm the device boots without `Pa.HUB init failed`
- Confirm the device boots without `MLX90614 init failed`
- Verify stable temperature readings on the display and gauge screens

### Servo mode

- Enter the Servo screen from the main menu
- Center the joystick and verify the UI reports `STOPPED`
- Move joystick left and confirm `CCW` command changes
- Move joystick right and confirm `CW` command changes
- Verify the continuous servo stops cleanly near center

### Settings

- Change units and confirm display updates
- Change volume and verify beep level changes
- Adjust alert temperature and confirm persistence after reboot
- Adjust emissivity only when the NCIR unit is correctly connected

## Extension Notes

If you add more Pa.HUB-routed devices later, keep these patterns:

- assign one fixed channel per device
- select the channel immediately before every device transaction
- avoid assuming the previous operation left the desired channel active
- keep user-facing routing constants grouped at the top of the file

If the project grows further, the next cleanup step would be splitting [src/main.cpp](C:/Users/nickd/Documents/PlatformIO/Projects/attemp23/src/main.cpp) into smaller files such as:

- `ui_*.cpp`
- `sensor_*.cpp`
- `servo_*.cpp`
- `settings_*.cpp`
- `hardware_*.cpp`

That is not required for current functionality, but it would make future changes easier.

## Summary

This project is now more than a standalone NCIR monitor. It is a button-driven CoreS3 application with a Pa.HUB-based I2C expansion architecture, a routed MLX90614 measurement path, and a continuous-servo control mode driven by a joystick and PbHub.

The most important implementation detail to remember is that external device communication is now channel-selected through the Pa.HUB, so all routed devices depend on correct hub selection before use.
