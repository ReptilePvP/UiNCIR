# Active Context

## Current State
The current firmware is stable enough to build and run as a joystick-driven temperature dashboard. The MLX90614 and Joystick2 both operate through the Pa.HUB, and the UI is built with a five-tab LVGL tabview.

## What Is Working
- Pa.HUB channel switching
- MLX90614 initialization and reading
- Joystick raw I2C input
- Tab navigation with left/right movement
- Row-based settings navigation and edit/apply interactions
- Refresh, units, emissivity, and debug controls
- Alerts enable/threshold controls with tone/visual behavior
- Calibration offset controls
- Live object and ambient display
- Session stats tracking
- Preferences persistence across reboots

## What Is Not Present In Active Firmware
- Servo control
- Historical graphing
- Full module split (logic remains monolithic in `main.cpp`)

## Immediate Documentation Rule
When making future changes, treat `src/main.cpp` as the behavioral source of truth and update `memory-bank/current-firmware.md` if the architecture changes.

## Good Next Engineering Steps
1. Split hardware access, UI, and app state into separate files.
2. Add chart/history while protecting read stability.
3. Decide whether legacy servo and gauge artifacts still belong in the repo.
4. Add a graph or richer live visualization if the display performance allows it.
5. Refresh the README if features materially change.

## Watchouts
- Pa.HUB channel selection is easy to break if device access is refactored carelessly.
- Zone logic currently depends on Fahrenheit thresholds even when Celsius is displayed.
- Emissivity changes require a restart by design.
- Stats reset every boot because session stats are runtime-only.
