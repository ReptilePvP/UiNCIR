# Product Context: CoreS3 Temperature Dashboard

## Problem
The device needs a simple on-device interface for non-contact temperature monitoring using the MLX90614. The display should be readable at a glance, and navigation should work from a physical joystick instead of touch.

## Current Product Shape
The project is currently a compact dashboard rather than a full workflow application. It is optimized for quick temperature checks and basic session stats, not for long configuration flows or extensive data storage.

## Current User Experience Goals
1. **Fast Readability**: Object temperature should be obvious immediately.
2. **Simple Navigation**: Horizontal joystick movement should move between major views without confusion.
3. **Minimal Friction**: Unit toggle and refresh tuning should be easy from a single settings page.
4. **Stable Operation**: The hardware chain CoreS3 -> Pa.HUB -> Joystick2/MLX90614 should stay reliable during normal use.

## Primary Usage Flow
1. Power on device
2. Land in the tabbed dashboard
3. View live temperature on the Live tab
4. Move to Stats for min/max and read count
5. Move to Settings to change units or refresh speed
6. Return to Live for continued monitoring

## Current Interaction Model
- **Left/Right**: navigate tabs
- **Up/Down**: adjust refresh rate on Settings
- **Press**: toggle Fahrenheit/Celsius on Settings

## What The Product Is Not Yet
- Not yet a persistent measurement logger
- Not yet an alerting device
- Not yet a servo-assisted automation tool
- Not yet a touchscreen workflow

## Design Priorities
- Large temperature text
- Clear zone status
- Predictable control mapping
- Low enough complexity to keep the firmware stable
