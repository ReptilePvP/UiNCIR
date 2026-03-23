# Current Firmware Map

## Summary
The active firmware is a joystick-driven LVGL dashboard for the M5Stack CoreS3. It reads an MLX90614 infrared temperature sensor through a Pa.HUB and renders a five-tab UI on the built-in display.

Behavioral source of truth: `src/main.cpp`.

## Hardware Topology
- **Core controller**: M5Stack CoreS3
- **I2C bus**: Port A on `SDA=2`, `SCL=1`
- **Pa.HUB address**: `0x70`
- **Joystick2**: Pa.HUB channel `1`, I2C address `0x63`
- **MLX90614**: Pa.HUB channel `5`, I2C address `0x5A`

## Runtime Flow
### Setup
- Initializes `M5Unified` and enables external port power
- Starts serial logging at `115200`
- Starts I2C on the CoreS3 Port A pins
- Loads persisted preferences from namespace `uiflow`
- Initializes LVGL and creates a 320x240 display bridge
- Builds a five-tab UI with `lv_tabview`
- Selects the MLX channel and initializes the MLX90614
- Reads current MLX emissivity and performs an initial temperature read when available

### Main Loop
- Advances the LVGL tick every `5 ms`
- Polls the joystick every `35 ms`
- Polls the MLX90614 based on the selected refresh interval
- Handles threshold alert state and alert tones
- Refreshes labels/sliders/indicators every `80 ms`
- Executes delayed restart after emissivity write when required
- Calls `lv_timer_handler()` continuously

## UI Structure
### Live tab
- Large object temperature label
- Ambient temperature label
- Emissivity display
- Battery status label
- Temperature bar and zone label
- Optional alert visual background when threshold is reached

### Stats tab
- Minimum object temperature seen
- Maximum object temperature seen
- Last reading
- Successful read count

### Settings tab
- Unit selection (`F` / `C`)
- Refresh interval
- Emissivity edit/apply flow (save + restart required)
- Debug mode toggle

### Alerts tab
- Alerts enable/disable
- Alert threshold edit/apply
- Threshold slider and guidance text

### Calibration tab
- Calibration offset edit/apply
- Offset slider and guidance text

## Input Model
The project uses Joystick2 raw I2C reads (not a wrapper library).

### Horizontal movement
- Left/right changes the active tab when no edit mode is active

### Vertical movement
- Up/down selects rows or adjusts values depending on active tab and edit mode

### Button press
- Press applies the selected action in Settings / Alerts / Calibration tabs

### Input filtering
- Simple moving filter based on `JOY_FILTER_DIV`
- Threshold gating for horizontal and vertical navigation
- Separate debounce/repeat timers for axis navigation and button press

## Temperature Model
- Sensor reads are stored in Fahrenheit and Celsius
- Calibration offset is applied in Fahrenheit
- Min/max tracking is kept in Fahrenheit and converted for display as needed
- UI display values are rounded to whole numbers
- Zone color and zone text are derived from Fahrenheit object temperature

### Current zone bands
- `< 500 F`: `COLD`
- `500-610 F`: `GOOD`
- `> 610 F`: `TOO HOT`

## Persistence
Stored in `Preferences` namespace `uiflow`:
- Units (`use_f`)
- Refresh index (`refresh_idx`)
- Emissivity (`emissivity`)
- Alerts enabled (`alerts_on`)
- Alert threshold Fahrenheit (`alert_f`)
- Calibration offset Fahrenheit (`cal_off_f`)
- Debug enabled (`debug_on`)

## Important Constraints
- Pa.HUB channel selection must happen before every downstream transaction
- Servo logic is not in the active runtime flow
- Emissivity writes trigger a delayed restart by design
- Architecture is currently monolithic in `src/main.cpp`

## Files That Matter Right Now
- `src/main.cpp`: active application logic
- `README.md`: high-level hardware and feature summary
- `platformio.ini`: build target, libraries, and flags
- `include/lv_conf.h`: LVGL configuration

## Legacy Note
Some memory-bank files describe older button-driven NCIR monitor concepts or earlier experiments. Use this file and `src/main.cpp` first when behavior conflicts.
