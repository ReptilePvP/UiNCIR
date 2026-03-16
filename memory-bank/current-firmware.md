# Current Firmware Map

## Summary
The active firmware is a joystick-driven LVGL dashboard for the M5Stack CoreS3. It reads an MLX90614 infrared temperature sensor through a Pa.HUB and renders a four-tab UI on the built-in display.

This document reflects the code currently compiled from `src/main.cpp`.

## Hardware Topology
- **Core controller**: M5Stack CoreS3
- **I2C bus**: Port A on `SDA=2`, `SCL=1`
- **Pa.HUB address**: `0x70`
- **Joystick2**: Pa.HUB channel `1`, I2C address `0x63`
- **MLX90614**: Pa.HUB channel `5`, I2C address `0x5A`

## Runtime Flow
### Setup
- Initializes `M5Unified`
- Starts serial logging at `115200`
- Starts I2C on the CoreS3 Port A pins
- Initializes LVGL and creates a 320x240 display bridge
- Builds a four-tab UI with `lv_tabview`
- Selects the MLX channel and initializes the MLX90614
- Performs an initial temperature read when the sensor is available

### Main Loop
- Advances the LVGL tick every `5 ms`
- Polls the joystick every `35 ms`
- Polls the MLX90614 based on the selected refresh interval
- Refreshes the visible labels and progress bar every `80 ms`
- Calls `lv_timer_handler()` continuously

## UI Structure
### Live tab
- Large object temperature label
- Ambient temperature label
- Temperature bar
- Temperature zone label

### Stats tab
- Minimum object temperature seen
- Maximum object temperature seen
- Last reading
- Successful read count

### Settings tab
- Unit selection display (`F` or `C`)
- Refresh interval display
- Control hint text

### About tab
- Short hardware and software summary

## Input Model
The project uses the Joystick2 over raw I2C reads rather than a joystick library wrapper.

### Horizontal movement
- Left/right changes the active tab

### Vertical movement
- Up/down changes refresh speed only when the Settings tab is active

### Button press
- Toggles Fahrenheit/Celsius only when the Settings tab is active

### Input filtering
- Simple moving filter based on `JOY_FILTER_DIV`
- Threshold gating for horizontal and vertical navigation
- Separate debounce/repeat timers for axis navigation and button press

## Temperature Model
- Sensor reads are stored in both Fahrenheit and Celsius
- Min/max tracking is kept in Fahrenheit and converted for display when needed
- UI display values are rounded to whole numbers
- Zone color and zone text are derived from the Fahrenheit object temperature

### Fahrenheit zones
- `< 350 F`: `LOW`
- `350-449 F`: `READY`
- `450-549 F`: `HOT`
- `>= 550 F`: `TOO HOT`

## Important Constraints
- No persistent settings storage yet
- No servo logic in the active firmware path
- No alert audio, LED logic, or battery management in the active firmware path
- No touch interaction handling

## Files That Matter Right Now
- `src/main.cpp`: active application logic
- `README.md`: current high-level hardware and feature summary
- `platformio.ini`: build target, libraries, and flags
- `include/lv_conf.h`: LVGL configuration

## Files That Appear To Be Legacy Or Experimental
- `memory-bank/projectbrief.md`
- `memory-bank/productContext.md`
- `memory-bank/systemPatterns.md`
- `memory-bank/techContext.md`
- `memory-bank/activeContext.md`
- `memory-bank/progress.md`
- `memory-bank/recreation-guide.md`
- `src/Old Work verison with temp.md`
- `src/gauge_animation.h`
- `include/gauge_animation.h`

Those files describe a different button-driven NCIR monitor architecture or earlier experiments and should not be treated as the source of truth for current behavior.
