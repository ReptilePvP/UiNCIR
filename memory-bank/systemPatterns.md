# System Patterns & Architecture

## Core Architecture

### Tab-Driven UI Pattern
The active firmware uses a single LVGL screen with a `lv_tabview` containing four tabs:
- `Live`
- `Stats`
- `Settings`
- `About`

Navigation state is tracked with `current_tab`, and tab changes are performed through `goto_tab()`.

### Polled Joystick Input Pattern
The joystick is read by raw I2C register access instead of a higher-level device library. Input is polled on a timed interval and passed through a simple filter before being used for navigation.

Relevant pattern:
- select Pa.HUB channel
- read X register
- read Y register
- read button register
- apply center offset and filtering
- apply threshold and debounce/repeat rules

### Pa.HUB Channel Selection Pattern
Every device read begins by selecting the correct Pa.HUB channel through I2C address `0x70`. This is a critical integration pattern because both the joystick and MLX90614 share the same upstream bus.

### Timed Update Pattern
The main loop is driven by separate intervals:
- LVGL tick interval
- joystick polling interval
- temperature sampling interval
- UI refresh interval

This keeps UI work, input handling, and sensor reads separate and predictable.

## Data Patterns

### Dual-Unit Temperature Storage
The firmware stores readings in both Fahrenheit and Celsius after each successful sensor read. Fahrenheit is the base for zone classification and min/max tracking.

### Derived Display Values
UI labels use rounded whole-number display values derived from the latest floating-point readings. This keeps the display easier to read at a glance.

### Session Statistics
The active session tracks:
- minimum object temperature
- maximum object temperature
- last object temperature
- successful read count

These values are in-memory only and reset on reboot.

## UI Update Pattern
The UI is created once in `build_ui()` and updated repeatedly through `update_ui()`. LVGL object pointers are stored globally to avoid dynamic lookup or screen reconstruction during runtime.

## Integration Constraints
- No persistent settings layer yet
- No interrupt-driven input handling
- No background task separation
- No dedicated hardware abstraction layer beyond helper functions in `main.cpp`

## Current Architecture Risks
- Most logic is concentrated in one source file
- The memory-bank previously described a different architecture and must not be reused as-is
- Pa.HUB channel switching is a dependency for every successful peripheral read
