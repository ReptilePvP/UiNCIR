
# Codex Project Context

## Hardware
CoreS3 Port A I2C
SDA = GPIO2
SCL = GPIO1

Pa.HUB Channels
0 = Servo8 (currently disabled)
1 = Joystick2
5 = MLX90614 NCIR sensor

## I2C Addresses
PaHub: 0x70
Joystick2: 0x63
MLX90614: 0x5A

Joystick registers:
0x10 -> X axis
0x11 -> Y axis
0x20 -> button

Button logic:
0 = pressed

## Software Stack
Arduino (PlatformIO)
LVGL 9.5.0
M5Unified
M5GFX
Adafruit_MLX90614

## UI Tabs
1. Live
2. Stats
3. Settings
4. Alerts
5. Calibration

## Controls
Left / Right : switch tabs
Up / Down : select row or adjust value on active settings-style tab
Joystick press : apply/toggle selected row action

## Temperature Zones
<500F : COLD
500-610F : GOOD
>610F : TOO HOT

## Persistent Preferences
Namespace: `uiflow`
- `use_f` (units)
- `refresh_idx` (refresh rate index)
- `emissivity`
- `alerts_on`
- `alert_f`
- `cal_off_f`
- `debug_on`

## Current Status
Sensor reading correctly
Joystick navigation working
LVGL UI stable
Servo removed temporarily
Alerts and calibration flows active
