# Advanced Project Context

## Purpose
Build a portable, joystick-driven, touchscreen-capable temperature dashboard on an M5Stack CoreS3 for terp-meter style use.

## Current working baseline
- PlatformIO project
- Arduino framework
- LVGL 9.5.0 UI
- Pa.HUB on Port A
- Raw I2C Joystick2 on channel 1
- Adafruit MLX90614 on channel 5
- Servo removed for now
- Working joystick-controlled 5-tab navigation and settings UI
- Persistent preferences enabled
- Alerts and calibration tabs enabled

## Important implementation decisions
1. **Use Adafruit_MLX90614**, not the M5 NCIR library.
2. **Use raw I2C for Joystick2**, not a dedicated joystick library.
3. **Keep Pa.HUB channel selection explicit** before each device transaction.
4. **Preserve the minimal MLX test pattern** as the known-good sensor reference.
5. **Treat servo as a separate future feature**, not part of the stable baseline.

## Current control scheme
- Left / Right: switch tabs
- Up / Down: select rows or adjust values on active settings-style tab
- Press joystick: apply/toggle selected row action

## Current tabs
- Live
- Stats
- Settings
- Alerts
- Calibration

## Sensor behavior
- Object and ambient temp from MLX90614
- Refresh interval configurable
- Session min/max stats tracked
- Temperature zone derived from Fahrenheit thresholds (`COLD`, `GOOD`, `TOO HOT`)
- Calibration offset applied in Fahrenheit

## Device personality / UX goals
- Fast live readings
- Clean dark dashboard
- Fully navigable without a keyboard
- Eventually suitable as a finished standalone handheld tool

## Constraints
- Sensor stability is more important than adding features too quickly
- Any servo work must not break sensor reliability
- Keep joystick handling simple and deterministic
- Avoid unnecessary I2C traffic
- Keep Pa.HUB channel switching explicit before every transaction

## Preferred development workflow
1. Preserve known-good code
2. Add one subsystem at a time
3. Validate MLX first after any regression
4. Use serial logging when bringing up new hardware behavior

## Recommended next coding tasks
1. Polish Live tab visuals
2. Add chart/history
3. Improve settings/edit-mode UX and notices
4. Only after that, revisit servo in a separate branch/test build
