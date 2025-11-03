# Project Brief: NCIR Monitor Device

## Executive Summary
Development of a handheld ESP32-based temperature monitoring device using NCIR (Non-Contact Infrared) sensing technology. The device features an M5Stack CoreS3 microcontroller with LCD display, MLX90614 temperature sensor, and hardware buttons for navigation. The project emphasizes hardware-only operation (no touchscreen) for use in challenging environments.

## Project Overview
This project creates a portable, battery-powered temperature monitoring device designed for non-contact temperature measurement in industrial, medical, or general purpose applications. The device prioritizes hardware button navigation to enable operation with gloves, wet hands, or in various environmental conditions where touchscreen interaction is impractical.

## Core Requirements

### Hardware Integration
- **M5Stack CoreS3**: ESP32-based microcontroller platform
- **MLX90614 NCIR Sensor**: Non-contact infrared temperature sensor via I2C
- **320x240 LCD Display**: TFT screen with LVGL graphics library
- **Hardware Buttons**: 3-button navigation system (Button 1, Button 2, Key Button)
- **Audio Output**: Built-in speaker for alerts and feedback
- **LED Indicator**: Visual status indication

### User Interface Requirements
- **Multi-Screen Architecture**: Four primary screens
  - Main Menu: Navigation hub with visual selection highlighting
  - Temperature Display: Real-time numeric temperature readings with color coding
  - Temperature Gauge: Analog-style gauge with needle indicator
  - Settings: Comprehensive configuration interface with tab navigation
- **Hardware-Only Navigation**: Complete operation via buttons, no touchscreen interaction
- **Visual Feedback**: Clear highlighting and selection indicators
- **Responsive Updates**: Real-time temperature refresh (150ms update rate)

### Functional Requirements
- **Real-Time Temperature Monitoring**
  - Object temperature measurement (-70°C to +380°C range)
  - Ambient temperature display
  - Dual unit support (Celsius/Fahrenheit)
- **Visual Temperature Gauge**
  - Analog-style circular gauge with animated needle
  - Color-coded temperature zones
  - Real-time value display
- **Alert System**
  - Configurable high/low temperature thresholds
  - Audio alerts with volume control
  - Visual indicators (LED and screen feedback)
  - Hysteresis to prevent rapid on/off cycling
- **Settings Management**
  - Temperature units (Celsius/Fahrenheit)
  - Audio enable/disable and volume control
  - Alert enable/disable and threshold configuration
  - Display brightness control
  - Debug logging toggle
  - Persistent storage of all settings

## Technical Specifications

### Hardware Platform
- **Microcontroller**: ESP32 (M5Stack CoreS3)
  - Dual-core Tensilica LX6 @ 240MHz
  - 520KB SRAM, 16MB Flash
  - WiFi and Bluetooth capabilities
  - 25 GPIO pins
- **Display**: 320x240 TFT LCD (ILI9342C controller)
  - 16-bit RGB565 color depth
  - Hardware-accelerated rendering
- **Sensor**: MLX90614 Non-Contact Infrared Thermometer
  - I2C interface (address 0x5A)
  - Measurement range: -70°C to +380°C
  - Accuracy: ±0.5°C, Resolution: 0.02°C
- **Input Controls**:
  - Button 1 (Blue): GPIO 17, forward navigation
  - Button 2 (Red): GPIO 18, backward navigation
  - Key Button: GPIO 8, selection/activation
- **Output Devices**:
  - Built-in speaker: PWM-based audio output
  - LED indicator: GPIO 9 for status indication
- **Power**: Lithium-polymer battery with charging circuit

### Software Stack
- **Framework**: Arduino (PlatformIO)
- **Platform**: Espressif32 v6.4.0
- **Core Libraries**:
  - M5Unified v0.2.10: Hardware abstraction
  - M5CoreS3 v1.0.1: M5Stack platform support
  - M5GFX v0.2.15: Graphics driver
  - LVGL v9.4.0: UI framework
  - Adafruit MLX90614 Library v2.1.5: Sensor driver
  - FastLED v3.10.3: LED control
- **Storage**: ESP32 Preferences library for non-volatile settings

### Software Architecture
- **State-Driven UI**: Screen state enumeration controls navigation
  - `SCREEN_MAIN_MENU`: Home screen
  - `SCREEN_TEMP_DISPLAY`: Numeric temperature view
  - `SCREEN_TEMP_GAUGE`: Analog gauge view
  - `SCREEN_SETTINGS`: Configuration interface
- **Interrupt-Driven Input**: GPIO interrupts for responsive button handling
- **Timed Updates**: 
  - LVGL refresh: 30ms cycle
  - Temperature reading: 150ms cycle
- **Settings Tab System**: Enumeration-based tab navigation
  - Units, Audio, Display, Alerts, Debug, Exit tabs

## User Interface Design

### Main Menu
- **Layout**: Three option cards (Thermometer, Gauge, Settings)
- **Navigation**: 
  - Buttons 1 & 2: Disabled (no function)
  - Key Button: Opens Settings screen
- **Visual Feedback**: Selection highlighting for current option

### Temperature Display Screen
- **Object Temperature**: Large numeric display with color coding
  - Blue: Cold (< 20°C)
  - Green: Normal (20-30°C)
  - Yellow: Warm (30-40°C)
  - Red: Hot (> 40°C)
- **Ambient Temperature**: Secondary display
- **Status Indicator**: Current alert state
- **Back Button**: Returns to main menu

### Temperature Gauge Screen
- **Analog Gauge**: Circular gauge with animated needle
- **Value Display**: Current temperature reading
- **Color Zones**: Visual temperature ranges
- **Back Button**: Returns to main menu

### Settings Screen
- **Tab Navigation**: Six configuration tabs
  - **Units**: Temperature unit selection (Celsius/Fahrenheit)
  - **Audio**: Sound enable/disable, volume control
  - **Display**: Brightness adjustment
  - **Alerts**: Alert enable/disable, threshold configuration
  - **Debug**: Debug logging toggle
  - **Exit**: Cancel or Save & Exit options
- **Navigation Controls**:
  - Button 1: Navigate forward through tabs (left→right)
  - Button 2: Navigate backward through tabs (right→left)
  - Key Button: Select/activate current tab's settings
- **Visual Highlighting**: Active tab and selection indicators
- **Exit Flow**: Explicit Cancel/Save & Exit confirmation

## Current Implementation Status

### Completed Features ✓
- ✅ Multi-screen UI architecture with LVGL
- ✅ Sensor integration with real-time readings
- ✅ Temperature display with color coding
- ✅ Analog gauge with animated needle
- ✅ Settings system with persistent storage
- ✅ Alert system with audio and visual feedback
- ✅ Hardware button interrupt handling
- ✅ Main menu with visual selection highlighting
- ✅ Settings tab navigation system
- ✅ Exit tab with Cancel/Save & Exit options
- ✅ On-screen button labels

### In Progress
- ⏳ Visual highlighting for settings tabs and selections
- ⏳ Exit tab selection switching (Buttons 1/2 for Cancel/Save toggle)
- ⏳ Complete button behavior validation across all screens

### Remaining Work
- ⬜ Finalize visual highlighting system styling
- ⬜ Complete exit tab selection logic
- ⬜ Comprehensive testing of navigation flows
- ⬜ Button behavior refinement and optimization
- ⬜ User experience validation

## Navigation System

### Button Behavior by Screen

#### Main Menu
- **Button 1**: No function (disabled)
- **Button 2**: No function (disabled)
- **Key Button**: Navigate to Settings screen

#### Temperature Display/Gauge Screens
- **Button 1**: No function (disabled)
- **Button 2**: No function (disabled)
- **Key Button**: Return to Main Menu

#### Settings Screen
- **Button 1**: Navigate forward through tabs (Units → Audio → Display → Alerts → Debug → Exit)
- **Button 2**: Navigate backward through tabs (Exit → Debug → Alerts → Display → Audio → Units)
- **Key Button**: 
  - **On Settings Tabs**: Activate/select current setting option
  - **On Exit Tab**: Execute selected action (Cancel or Save & Exit)

### Visual Feedback System
- **Tab Highlighting**: Active tab visually distinguished (border/background)
- **Selection Indicators**: Current setting option highlighted
- **Exit Tab Selection**: Cancel/Save options highlighted based on selection state
- **Screen Transitions**: Smooth state changes with clear feedback

## Technical Patterns

### State Management
- Global state variables track current screen and settings context
- State machine pattern for screen transitions
- Settings tab enumeration for navigation

### Button Handling
- Debounced interrupt-driven input (150ms debounce)
- Context-aware button behavior based on current screen
- Separate debounce arrays for different screen contexts

### Data Persistence
- ESP32 Preferences namespace: "ncir_monitor"
- Settings loaded at startup, saved on changes
- Non-volatile storage with wear leveling

### Performance Optimization
- Static UI object allocation (no dynamic memory)
- Throttled sensor updates (150ms minimum)
- Efficient LVGL refresh timing (30ms cycle)

## Success Criteria

### Functional Requirements
- ✅ Device operates entirely through hardware buttons (no touch required)
- ✅ Intuitive navigation between all screens and settings
- ✅ Reliable temperature monitoring with appropriate alerts
- ✅ Clean, responsive user interface with clear visual feedback
- ✅ Persistent settings survive power cycles
- ✅ Accurate temperature readings within sensor specifications

### User Experience Requirements
- ✅ Hardware button navigation feels natural and logical
- ✅ Visual feedback makes current state clear
- ✅ Settings changes are immediately apparent
- ✅ No accidental settings loss (exit confirmation)
- ✅ Fast response to button presses (< 200ms)
- ✅ Clear on-screen labels guide user actions

### Technical Requirements
- ✅ Stable operation without crashes or freezes
- ✅ Memory usage within ESP32 constraints
- ✅ Battery-efficient operation
- ✅ Smooth screen transitions and animations
- ✅ Reliable sensor communication

## Development Environment

### PlatformIO Configuration
- **Board**: m5stack-cores3
- **Framework**: Arduino
- **Platform**: espressif32
- **Monitor**: 115200 baud with ESP32 exception decoder

### Build Flags
- `-std=c++11`: C++11 standard
- `-DBOARD_HAS_PSRAM`: Enable external PSRAM
- `-DLV_LVGL_H_INCLUDE_SIMPLE`: Simplified LVGL includes
- `-DLCD_HEIGHT=240`: Display height definition
- `-DLCD_WIDTH=320`: Display width definition
- `-DLV_TICK_PERIOD_MS=10`: LVGL tick period
- `-DM5CORES3`: M5Stack CoreS3 board definition

## Project Constraints

### Hardware Limitations
- Limited RAM requires static allocation strategies
- Limited flash space constrains UI complexity
- Single-core processing for LVGL (one of two cores)
- Battery operation requires power-efficient refresh rates

### Design Constraints
- Hardware-only navigation (no touchscreen)
- Three-button control system (limited input options)
- Small screen size (320x240) limits UI complexity
- Real-time temperature updates require efficient sensor communication

## Future Enhancements (Not in Current Scope)
- WiFi connectivity for remote monitoring
- Data logging and export capabilities
- Advanced sensor calibration options
- Battery level monitoring and power management
- Multiple measurement profiles
- Trend graphs and history display
- LVGL v9 migration (currently on v9.4.0)

## Project Status Summary
**Overall Progress**: ~90% Complete

The core functionality is implemented and working. The remaining work focuses on finalizing the visual highlighting system and completing the exit tab selection logic. The device is functionally complete and ready for final polish and testing.
