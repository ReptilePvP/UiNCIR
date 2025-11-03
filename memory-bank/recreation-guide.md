# Project Recreation Guide: NCIR Monitor Device

## Complete Step-by-Step Instructions for Recreating the Project

This document provides comprehensive instructions for recreating the NCIR (Non-Contact Infrared) Temperature Monitor Device from scratch.

---

## Part 1: Hardware Requirements

### Required Components
1. **M5Stack CoreS3** - ESP32-based microcontroller with integrated display
   - 320x240 TFT LCD touchscreen (touchscreen disabled in this project)
   - Built-in speaker
   - 3 hardware buttons (Button 1, Button 2, Key Button)
   - USB-C port for power and programming
   - GPIO pins: 8, 9, 17, 18

2. **MLX90614 Non-Contact Infrared Temperature Sensor**
   - I2C interface (default address: 0x5A)
   - Measurement range: -70°C to +380°C
   - Accuracy: ±0.5°C
   - Resolution: 0.02°C
   - 3.3V operation

### Hardware Connections
The MLX90614 sensor connects via I2C:
- **VCC** → 3.3V (on M5Stack)
- **GND** → Ground (on M5Stack)
- **SDA** → GPIO 21 (I2C SDA on M5Stack)
- **SCL** → GPIO 22 (I2C SCL on M5Stack)

**Note**: If using an M5Stack sensor module, it should plug directly into the Grove connector. Otherwise, use jumper wires to connect to the I2C bus.

### GPIO Pin Assignments (M5Stack CoreS3 Built-in)
- **GPIO 8** → Key Button (built-in)
- **GPIO 9** → LED indicator (built-in)
- **GPIO 17** → Button 1 (built-in, Blue)
- **GPIO 18** → Button 2 (built-in, Red)
- **GPIO 21** → I2C SDA (built-in)
- **GPIO 22** → I2C SCL (built-in)

---

## Part 2: Software Environment Setup

### Prerequisites
1. **PlatformIO IDE** (VS Code extension) or PlatformIO Core
   - Download from: https://platformio.org/
   - Or install via VS Code Extensions

2. **Arduino Framework** (will be configured automatically)

### PlatformIO Configuration File

Create `platformio.ini` in the project root:

```ini
[env:m5stack-cores3]
platform = espressif32
board = m5stack-cores3
framework = arduino
monitor_speed = 115200
monitor_filters = esp32_exception_decoder

lib_deps = 
    m5stack/M5CoreS3@^1.0.1
    m5stack/M5Unified@^0.2.10
    m5stack/M5GFX@^0.2.15
    adafruit/Adafruit BusIO@^1.14.5
    lvgl/lvgl@^9.4.0
    adafruit/Adafruit MLX90614 Library@^2.1.5
    fastled/FastLED@^3.10.3

build_flags = 
    -std=c++11
    -DBOARD_HAS_PSRAM
    -DLV_LVGL_H_INCLUDE_SIMPLE
    -DLV_CONF_INCLUDE_SIMPLE
    -DLCD_HEIGHT=240
    -DLCD_WIDTH=320
    -DLV_TICK_PERIOD_MS=10
    -DM5CORES3
    -I./include

[platformio]
description = NCIR Monitor Device
```

---

## Part 3: Project Structure

Create the following directory structure:

```
project-root/
├── platformio.ini
├── src/
│   └── main.cpp
├── include/
│   └── lv_conf.h
├── lib/
│   └── m5gfx_lvgl/
│       ├── m5gfx_lvgl.hpp
│       └── m5gfx_lvgl.cpp
└── memory-bank/  (optional, for documentation)
```

---

## Part 4: LVGL Configuration

### Create `include/lv_conf.h`

This is a critical configuration file for LVGL. Copy the LVGL configuration template and customize it. Key settings:

- **Color Depth**: 16-bit RGB565 (`LV_COLOR_DEPTH 16`)
- **Memory**: Use standard C library malloc (`LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB`)
- **Operating System**: FreeRTOS (`LV_USE_OS LV_OS_FREERTOS`)
- **Fonts**: Enable Montserrat fonts (8, 12, 14, 16, 18, 20, 24)
- **Widgets**: Enable Scale, Label, Button, Slider, Line
- **Default Font**: `&lv_font_montserrat_14`

**Critical Settings**:
```c
#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_OS LV_OS_FREERTOS
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14
#define LV_USE_SCALE 1
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_SLIDER 1
#define LV_USE_LINE 1
```

---

## Part 5: M5GFX LVGL Adapter

### Create `lib/m5gfx_lvgl/m5gfx_lvgl.hpp`

```cpp
#ifndef __M5GFX_LVGL_H__
#define __M5GFX_LVGL_H__

#include "lvgl.h"
#include "M5Unified.h"
#include "M5GFX.h"

extern SemaphoreHandle_t xGuiSemaphore;

void m5gfx_lvgl_init(void);

#endif  // __M5GFX_LVGL_H__
```

### Create `lib/m5gfx_lvgl/m5gfx_lvgl.cpp`

This file provides the bridge between M5Stack's display and LVGL. Complete implementation:

```cpp
#include "m5gfx_lvgl.hpp"

SemaphoreHandle_t xGuiSemaphore;

// Flush callback: transfers LVGL buffer to M5Stack display
static void m5gfx_lvgl_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Swap bytes for RGB565 format
    lv_draw_sw_rgb565_swap(px_map, w * h);
    
    // Use DMA for fast transfer
    M5.Display.startWrite();
    M5.Display.pushImageDMA<uint16_t>(area->x1, area->y1, w, h, (uint16_t *)px_map);
    M5.Display.waitDMA();
    M5.Display.endWrite();
    
    // Signal LVGL that flush is complete
    lv_display_flush_ready(disp);
}

// Touch input callback (disabled in this project but required by LVGL)
static void m5gfx_lvgl_read(lv_indev_t * drv, lv_indev_data_t * data) {
    M5.update();
    auto t = M5.Touch.getDetail();
    
    if (t.isPressed()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = t.x;
        data->point.y = t.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Custom tick function using ESP32 timer
static uint32_t my_tick_function() {
    return (esp_timer_get_time() / 1000LL);
}

// Display buffer size (partial rendering)
#define LCD_BUFFER_SIZE (LCD_WIDTH * 80)

void m5gfx_lvgl_init(void) {
    // Set custom tick function
    lv_tick_set_cb(my_tick_function);

    // Allocate display buffer (using stack allocation)
    // Can use PSRAM if available: heap_caps_aligned_alloc(32, LCD_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM)
    static lv_color_t buf1[LCD_BUFFER_SIZE] __attribute__((aligned(4)));

    // Create LVGL display
    lv_display_t* disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    if (!disp) {
        log_e("Failed to create display");
        return;
    }

    // Configure display buffers (single buffer for partial rendering)
    lv_display_set_buffers(disp, buf1, nullptr, LCD_BUFFER_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, m5gfx_lvgl_flush);

    // Configure touch input (required but disabled via button-only navigation)
    lv_indev_t* touch_indev = lv_indev_create();
    if (!touch_indev) {
        log_e("Failed to create touch input device");
        return;
    }

    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, m5gfx_lvgl_read);

    // Create semaphore for FreeRTOS (if using multi-threaded LVGL)
    xGuiSemaphore = xSemaphoreCreateMutex();
}
```

**Key Implementation Details**:
- Uses partial rendering mode (80 lines buffer) to save RAM
- DMA transfer for fast screen updates
- RGB565 byte swapping required for M5Stack display
- Touch input configured but not used (project uses buttons only)
- Custom tick function using ESP32 high-resolution timer

---

## Part 6: Main Code Implementation

### Core Architecture

The application uses a **state machine pattern** with the following states:

```cpp
enum ScreenState {
    SCREEN_MAIN_MENU,      // Home screen with navigation
    SCREEN_TEMP_DISPLAY,   // Numeric temperature display
    SCREEN_TEMP_GAUGE,     // Analog gauge display
    SCREEN_SETTINGS        // Configuration menu
};

enum SettingsScreen {
    SETTINGS_MENU,         // Settings main menu
    SETTINGS_UNITS,        // Temperature units (C/F)
    SETTINGS_AUDIO,        // Audio settings
    SETTINGS_DISPLAY,      // Display brightness
    SETTINGS_ALERTS,       // Temperature thresholds
    SETTINGS_DEBUG,        // Debug logging
    SETTINGS_EXIT          // Exit confirmation
};
```

### Global Variables Structure

```cpp
// Hardware pins
#define LED_PIN      9
#define BUTTON1_PIN 17
#define BUTTON2_PIN 18
#define KEY_PIN      8

// Screen configuration
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
static const uint32_t screenTickPeriod = 30;  // ms

// Global settings
int  brightness_level = 128;      // 0-255
bool sound_enabled    = true;
int  sound_volume     = 70;       // 0-100
float low_temp_threshold  = 10.0f;
float high_temp_threshold = 40.0f;
bool alerts_enabled   = true;
bool use_celsius      = true;
int  update_rate      = 150;      // ms
bool debug_logging    = false;

// Temperature readings
float current_object_temp = 0.0f;
float current_ambient_temp = 0.0f;

// Runtime state
ScreenState current_screen = SCREEN_MAIN_MENU;
SettingsScreen current_settings_screen = SETTINGS_MENU;
int current_settings_selection = 0;
bool exit_selection_cancel = true;

// UI objects (global pointers for LVGL)
lv_obj_t *main_menu_screen;
lv_obj_t *temp_display_screen;
lv_obj_t *temp_gauge_screen;
lv_obj_t *settings_screen;
// ... (many more UI object pointers)
```

### Setup Function

```cpp
void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    
    // Initialize sensor
    if (!mlx.begin()) {
        Serial.println("MLX90614 init failed!");
        while (1);
    }
    
    // Initialize LVGL
    lv_init();
    m5gfx_lvgl_init();
    
    // Hardware setup
    setup_hardware();
    load_preferences();
    M5.Display.setBrightness(brightness_level);
    M5.Speaker.setVolume(static_cast<uint8_t>(sound_volume * 2.55));
    
    // Create UI screens
    create_main_menu_ui();
    create_temp_display_ui();
    create_temp_gauge_ui();
    create_settings_ui();
    
    // Start on main menu
    lv_screen_load(main_menu_screen);
    lv_refr_now(nullptr);
}
```

### Main Loop Structure

```cpp
void loop() {
    M5.update();
    
    // LVGL refresh (every 30ms)
    uint32_t now = millis();
    if (now - lastLvglTick >= screenTickPeriod) {
        lvgl_tick_task(nullptr);
        lastLvglTick = now;
    }
    
    // Temperature update (every 150ms)
    static uint32_t last_update = 0;
    if (millis() - last_update >= update_rate) {
        update_temperature_reading();
        if (current_screen == SCREEN_TEMP_DISPLAY) update_temp_display_screen();
        if (current_screen == SCREEN_TEMP_GAUGE)   update_temp_gauge_screen();
        check_temp_alerts();
        last_update = millis();
    }
    
    // Button handling (for Main Menu and Settings)
    if (current_screen == SCREEN_MAIN_MENU || current_screen == SCREEN_SETTINGS) {
        // Button polling with debounce
        // Context-aware button behavior
    }
    
    delay(5);
}
```

### Key Implementation Functions

#### 1. Hardware Setup
```cpp
void setup_hardware() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    pinMode(KEY_PIN, INPUT_PULLUP);
    M5.Speaker.begin();
}
```

#### 2. Preferences (Settings Storage)
```cpp
void load_preferences() {
    preferences.begin("ncir", false);
    use_celsius = preferences.getBool("celsius", true);
    brightness_level = preferences.getInt("bright", 128);
    sound_enabled = preferences.getBool("sound", true);
    sound_volume = preferences.getInt("volume", 70);
    alerts_enabled = preferences.getBool("alerts", true);
    low_temp_threshold = preferences.getFloat("low", 10.0f);
    high_temp_threshold = preferences.getFloat("high", 40.0f);
    debug_logging = preferences.getBool("debug", false);
    preferences.end();
}

void save_preferences() {
    preferences.begin("ncir", false);
    preferences.putBool("celsius", use_celsius);
    preferences.putInt("bright", brightness_level);
    preferences.putBool("sound", sound_enabled);
    preferences.putInt("volume", sound_volume);
    preferences.putBool("alerts", alerts_enabled);
    preferences.putFloat("low", low_temp_threshold);
    preferences.putFloat("high", high_temp_threshold);
    preferences.putBool("debug", debug_logging);
    preferences.end();
}
```

#### 3. Temperature Reading
```cpp
void update_temperature_reading() {
    current_object_temp = mlx.readObjectTempC();
    current_ambient_temp = mlx.readAmbientTempC();
}
```

#### 4. Screen Creation Functions

Each screen follows a consistent pattern:

**Main Menu Screen**:
- Header with title "NCIR Monitor"
- Three navigation buttons (Thermometer, Gauge, Settings)
- Visual highlighting for current selection
- Buttons 1 & 2: No function
- Key Button: Navigate to Settings

**Temperature Display Screen**:
- Large circular display with temperature value
- Color-coded border (blue=cold, green=normal, yellow=warm, red=hot)
- Ambient temperature display
- Status indicator
- Back button (Key Button returns to main menu)

**Temperature Gauge Screen**:
- LVGL Scale widget configured as semi-circular gauge
- Animated needle using LVGL Line widget
- Value display label
- Back button

**Settings Screen**:
- Dynamically rebuilt based on current tab
- Tab navigation via Buttons 1 & 2
- Selection via Key Button
- Exit tab with Cancel/Save options

### Button Navigation Logic

**Main Menu**:
- Button 1: No function
- Button 2: No function
- Key Button: Open Settings

**Temperature Screens**:
- Button 1: No function
- Button 2: No function
- Key Button: Return to Main Menu

**Settings Screen**:
- Button 1: Navigate forward through tabs/menu items
- Button 2: Navigate backward through tabs/menu items
- Key Button: Select/activate current option

**Settings Sub-Screens**:
- Button 1: Select first option (e.g., Celsius, ON)
- Button 2: Select second option (e.g., Fahrenheit, OFF)
- Key Button: Confirm and return to Settings menu

**Exit Tab**:
- Button 1: Select Cancel
- Button 2: Select Save
- Key Button: Execute selected action

### Alert System

```cpp
void check_temp_alerts() {
    if (!alerts_enabled) return;
    
    static bool low_trig = false, high_trig = false;
    
    // Low temperature alert with hysteresis
    if (current_object_temp <= low_temp_threshold && !low_trig) {
        play_beep(800, 300); delay(100); play_beep(800, 300);
        low_trig = true;
        digitalWrite(LED_PIN, HIGH);
    } else if (current_object_temp > low_temp_threshold + 2.0f) {
        low_trig = false;
        digitalWrite(LED_PIN, LOW);
    }
    
    // High temperature alert with hysteresis
    if (current_object_temp >= high_temp_threshold && !high_trig) {
        play_beep(1200, 500); delay(100); play_beep(1200, 500);
        high_trig = true;
        digitalWrite(LED_PIN, HIGH);
    } else if (current_object_temp < high_temp_threshold - 2.0f) {
        high_trig = false;
        digitalWrite(LED_PIN, LOW);
    }
}
```

### Temperature Display Updates

The display uses adaptive smoothing:
- Large temperature jumps (>15°C) update immediately
- Small changes use exponential smoothing
- UI updates only when temperature changes by ≥1° to reduce flicker

```cpp
void update_temp_display_screen() {
    float obj = use_celsius ? current_object_temp : (current_object_temp * 9.0f/5.0f + 32.0f);
    float amb = use_celsius ? current_ambient_temp : (current_ambient_temp * 9.0f/5.0f + 32.0f);
    
    // Adaptive smoothing
    static float s_obj = 0, s_amb = 0;
    // ... smoothing logic ...
    
    // Update UI labels
    // Change border color based on temperature
    lv_color_t c = get_temperature_color(f);
    lv_obj_set_style_border_color(temp_circle, c, LV_PART_MAIN);
}
```

### Gauge Updates

```cpp
void update_temp_gauge_screen() {
    float temp = use_celsius ? current_object_temp : (current_object_temp * 9.0f/5.0f + 32.0f);
    
    // Smoothing
    static float smooth = 0;
    smooth = smooth * 0.8f + temp * 0.2f;
    
    // Calculate needle angle
    float minV = use_celsius ? 0.0f : 32.0f;
    float maxV = use_celsius ? 400.0f : 752.0f;
    float norm = (smooth - minV) / (maxV - minV);
    norm = LV_CLAMP(0.0f, norm, 1.0f);
    float angle = 135.0f + norm * 270.0f;
    
    // Update needle position using lv_line_set_points
    // Update value label
}
```

---

## Part 7: UI Design Details

### Color Scheme
- **Background**: Dark theme (#0f1419, #1a1a2e)
- **Accent Colors**: 
  - Orange/Red: #FF6B35 (primary accent)
  - Blue: #4285F4 (gauge accent)
  - Purple: #9b59b6 (settings accent)
- **Text**: White (#FFFFFF) on dark backgrounds
- **Borders**: Green (#00FF00) for selection highlights

### Font Usage
- **Title**: Montserrat 24pt
- **Headers**: Montserrat 20pt
- **Body Text**: Montserrat 14-18pt
- **Labels**: Montserrat 12pt

### Layout Patterns
- **Header Height**: 50-85px depending on screen
- **Button Sizes**: 90x50px for main menu, 100x50px for settings
- **Spacing**: 10px margins, 25-35px vertical offsets
- **Border Radius**: 8-30px for rounded corners

---

## Part 8: Build and Upload Process

### Step-by-Step Build Instructions

1. **Install PlatformIO**
   - Open VS Code
   - Install PlatformIO IDE extension
   - Restart VS Code

2. **Create Project**
   - File → New Project
   - Board: "m5stack-cores3"
   - Framework: "Arduino"
   - Project location: Choose your directory

3. **Configure Project**
   - Copy `platformio.ini` to project root
   - Create `include/` directory
   - Copy `lv_conf.h` to `include/`
   - Create `lib/m5gfx_lvgl/` directory
   - Add M5GFX adapter files

4. **Add Main Code**
   - Replace `src/main.cpp` with your implementation
   - Ensure all includes are correct

5. **Build Project**
   - Click PlatformIO icon in sidebar
   - Click "Build" button
   - Resolve any dependency or compilation errors

6. **Upload to Device**
   - Connect M5Stack CoreS3 via USB-C
   - Wait for driver installation (if needed)
   - Click "Upload" button in PlatformIO
   - Monitor serial output at 115200 baud

### Troubleshooting Common Issues

**Build Errors**:
- Verify all library versions match `platformio.ini`
- Check that `lv_conf.h` is in `include/` directory
- Ensure build flags are correct

**Upload Errors**:
- Check USB cable (data transfer capable)
- Press reset button on M5Stack if upload fails
- Try different USB port

**Runtime Errors**:
- Check serial monitor for error messages
- Verify sensor connection (I2C)
- Check LVGL initialization order
- Ensure M5GFX adapter is properly implemented

---

## Part 9: Key Design Patterns

### State Machine Pattern
All navigation and screen transitions use a state enumeration. The `current_screen` variable drives all UI behavior.

### Debounced Button Handling
Buttons use software debouncing (150ms minimum between presses) to prevent multiple triggers from mechanical switch bounce.

### Static UI Allocation
All LVGL UI objects are created once during setup and stored as global pointers. This avoids dynamic memory allocation during runtime.

### Preferences Pattern
Settings are loaded at startup and saved immediately when changed. Uses ESP32 Preferences library for non-volatile storage.

### Adaptive Smoothing
Temperature display uses exponential smoothing with adaptive alpha values based on temperature change magnitude.

### Selective UI Updates
UI elements only update when temperature changes by ≥1° to reduce unnecessary redraws and improve performance.

---

## Part 10: Testing Checklist

### Hardware Testing
- [ ] Sensor initializes correctly
- [ ] Temperature readings are accurate
- [ ] Buttons respond reliably
- [ ] LED indicator works
- [ ] Speaker produces sound
- [ ] Display shows all screens correctly

### Functional Testing
- [ ] Main menu navigation works
- [ ] Temperature display updates smoothly
- [ ] Gauge needle animates correctly
- [ ] Settings save and load properly
- [ ] Alert system triggers at thresholds
- [ ] Unit conversion (C/F) works
- [ ] Exit confirmation works

### User Experience Testing
- [ ] Button behavior is intuitive
- [ ] Visual feedback is clear
- [ ] Screen transitions are smooth
- [ ] Settings changes persist after reboot
- [ ] No crashes or freezes during normal use

---

## Part 11: Customization Options

### Changing Update Rates
Modify these constants:
```cpp
static const uint32_t screenTickPeriod = 30;  // LVGL refresh (ms)
int update_rate = 150;  // Temperature reading (ms)
```

### Adjusting Alert Thresholds
Default values:
```cpp
float low_temp_threshold = 10.0f;   // °C
float high_temp_threshold = 40.0f;  // °C
```

### Modifying Color Scheme
Update color hex values in UI creation functions:
- Background: `lv_color_hex(0x0f1419)`
- Accents: `lv_color_hex(0xFF6B35)`
- Selection: `lv_color_hex(0x00FF00)`

### Adding New Settings
1. Add preference key in `load_preferences()` and `save_preferences()`
2. Create new settings tab or option
3. Add button handling logic
4. Update UI display

---

## Part 12: Memory and Performance Considerations

### Memory Usage
- **Flash**: ~500KB (depends on fonts enabled)
- **RAM**: ~100KB (includes LVGL buffers)
- **PSRAM**: Used if available for display buffers

### Optimization Tips
- Disable unused LVGL features in `lv_conf.h`
- Limit font sizes to only those used
- Reduce update rates if experiencing lag
- Use static allocation for all UI objects

### Performance Benchmarks
- LVGL refresh: 30ms (33 FPS)
- Temperature updates: 150ms (6.7 Hz)
- Button response: < 200ms
- Screen transition: < 100ms

---

## Conclusion

This guide covers the essential components needed to recreate the NCIR Monitor Device. The project demonstrates:

- ESP32 embedded development with PlatformIO
- LVGL graphics library integration
- I2C sensor communication
- State machine UI architecture
- Hardware button navigation
- Persistent settings storage
- Real-time temperature monitoring with alerts

For additional help, refer to:
- LVGL Documentation: https://docs.lvgl.io/
- M5Stack Documentation: https://docs.m5stack.com/
- PlatformIO Documentation: https://docs.platformio.org/
- MLX90614 Datasheet: Available from Melexis

Good luck with your implementation!

