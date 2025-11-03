#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <lvgl.h>
#include "lv_conf.h"
#include "m5gfx_lvgl.hpp"
#include <Preferences.h>
#include "gauge_animation.h"

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Hardware pins for button polling
#define LED_PIN 9
#define NUM_LEDS 1
#define BUTTON1_PIN 17
#define BUTTON2_PIN 18
#define KEY_PIN 8

// Screen dimensions for CoreS3
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define LV_TICK_PERIOD_MS 10

// LVGL Refresh time - increased to reduce flickering
static const uint32_t screenTickPeriod = 30; // Increased from 10ms to 30ms for smoother operation
static uint32_t lastLvglTick = 0;

// Screen states
enum ScreenState {
    SCREEN_MAIN_MENU,
    SCREEN_TEMP_DISPLAY,
    SCREEN_TEMP_GAUGE,
    SCREEN_SETTINGS
};

// Settings screens (page-based instead of tabs)
enum SettingsScreen {
    SETTINGS_MENU,
    SETTINGS_UNITS,
    SETTINGS_AUDIO,
    SETTINGS_DISPLAY,
    SETTINGS_ALERTS,
    SETTINGS_EXIT
};

// Input states
volatile bool button1_pressed = false;
volatile bool button2_pressed = false;
volatile bool key_pressed = false;

  // Settings navigation state
SettingsScreen current_settings_screen = SETTINGS_MENU;
int current_settings_selection = 0; // current selected menu item (0-3 for 4 items)
bool exit_selection_cancel = true; // true = Cancel selected (Button 1), false = Save&Exit selected (Button 2)
unsigned long last_button_time[3] = {0, 0, 0}; // Debounce timers for each button
const unsigned long DEBOUNCE_DELAY = 150; // Reduced debounce delay for better responsiveness

// Display settings
int brightness_level = 128; // 0-255
bool sound_enabled = true;
int sound_volume = 70; // 0-100

// Alert settings
float low_temp_threshold = 10.0;
float high_temp_threshold = 40.0;
bool alerts_enabled = true;

// Current state
ScreenState current_screen = SCREEN_MAIN_MENU;

// Temperature variables
bool use_celsius = true; // Use Celsius by default
int update_rate = 500; // milliseconds - faster update rate for live reading
unsigned long last_update = 0;
float current_object_temp = 0;
float current_ambient_temp = 0;

// Smoothed temperatures for UI display (low-pass filter)
static float smoothed_object_temp = 0.0f;
static float smoothed_ambient_temp = 0.0f;
static bool smoothing_initialized = false;

// Preferences for persistent storage
Preferences preferences;

// LVGL task parameters
#define LVGL_TASK_CORE 1
#define LVGL_TASK_PRIORITY 5
#define LVGL_STACK_SIZE 32768

// UI Objects - Main Menu
lv_obj_t *main_menu_screen;
lv_obj_t *menu_title;
lv_obj_t *temp_display_btn;
lv_obj_t *temp_gauge_btn;
lv_obj_t *settings_menu_btn;

// UI Objects - Temperature Display Screen
lv_obj_t *temp_display_screen;
lv_obj_t *temp_display_back_btn;
lv_obj_t *object_temp_label;
lv_obj_t *ambient_temp_label;
lv_obj_t *temp_status_label;
lv_obj_t *temp_circle;

// UI Objects - Temperature Gauge Screen
lv_obj_t *temp_gauge_screen;
lv_obj_t *temp_gauge_back_btn;
lv_obj_t *temp_scale;
lv_obj_t *temp_gauge_needle;
lv_obj_t *temp_gauge_value_label;

// UI Objects - Settings Screen
lv_obj_t *settings_screen;
lv_obj_t *settings_back_btn;
lv_obj_t *settings_tabview;
lv_obj_t *tab_general;
lv_obj_t *tab_display;
lv_obj_t *tab_sound;
lv_obj_t *tab_alerts;

// Exit tab selection buttons
lv_obj_t *exit_cancel_btn;
lv_obj_t *exit_save_btn;

// Main Menu Temperature Preview
lv_obj_t *temp_preview_label;

// Temperature Display Screen - Unit Label
lv_obj_t *temp_unit_label;

// Settings widgets
lv_obj_t *temp_unit_switch;
lv_obj_t *brightness_slider;
lv_obj_t *brightness_label;
lv_obj_t *sound_enable_switch;
lv_obj_t *volume_slider;
lv_obj_t *volume_label;
lv_obj_t *alerts_enable_switch;
lv_obj_t *low_temp_slider;
lv_obj_t *high_temp_slider;
lv_obj_t *low_temp_label;
lv_obj_t *high_temp_label;

// Function declarations
void setup_hardware();
void load_preferences();
void save_preferences();
void switch_to_screen(ScreenState screen);
void create_main_menu_ui();
void create_temp_display_ui();
void create_temp_gauge_ui();
void create_settings_ui();
void setup_scale_gauge();
void update_temperature_reading();
void update_temp_display_screen();
void update_temp_gauge_screen();
void play_beep(int frequency, int duration);
void check_temp_alerts();
lv_color_t get_temperature_color(float temperature);

// Event handlers
void main_menu_event_cb(lv_event_t *e);
void temp_display_back_event_cb(lv_event_t *e);
void temp_gauge_back_event_cb(lv_event_t *e);
void settings_back_event_cb(lv_event_t *e);
void temp_unit_switch_event_cb(lv_event_t *e);
void brightness_slider_event_cb(lv_event_t *e);
void sound_enable_switch_event_cb(lv_event_t *e);
void volume_slider_event_cb(lv_event_t *e);
void alerts_enable_switch_event_cb(lv_event_t *e);
void temp_alert_slider_event_cb(lv_event_t *e);

// Switch between settings screens (page-based navigation)
void switch_to_settings_screen() {
  // Clear current screen and recreate appropriate UI based on current_settings_screen
  if (settings_screen) {
    lv_obj_clean(settings_screen);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x1a1a40), 0);

    // Enhanced title with modern styling
    lv_obj_t *title_bg = lv_obj_create(settings_screen);
    lv_obj_set_size(title_bg, 320, 50);
    lv_obj_align(title_bg, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bg, lv_color_hex(0x2c3e50), 0);

    lv_obj_t *title_border = lv_obj_create(settings_screen);
    lv_obj_set_size(title_border, 320, 2);
    lv_obj_align(title_border, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(title_border, lv_color_hex(0x9b59b6), 0);

    lv_obj_t *title = lv_label_create(title_bg);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 10, 0);

    // Create appropriate screen based on current_settings_screen
    switch (current_settings_screen) {
      case SETTINGS_MENU: {
        lv_label_set_text(title, "Configuration");
        // Settings menu with category selection (2x3 grid layout for 5 items)
        const char *menu_items[] = {"Units", "Audio", "Display", "Alerts", "Exit"};
        for (int i = 0; i < 5; i++) {
          lv_obj_t *menu_btn = lv_btn_create(settings_screen);
          lv_obj_set_size(menu_btn, 90, 50); // Smaller buttons for 2x3 grid

          // 2x3 grid positioning: Top row (y=-35), Bottom row (y=35)
          // Left column (x=-100), Center column (x=0), Right column (x=100)
          int row = i / 3; // 0 for top row, 1 for bottom row
          int col = i % 3; // 0 for left, 1 for center, 2 for right column
          lv_obj_align(menu_btn, LV_ALIGN_CENTER, (col == 0 ? -100 : (col == 1 ? 0 : 100)), (row == 0 ? -35 : 35));

          lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x34495e), LV_PART_MAIN);
          lv_obj_set_style_border_width(menu_btn, 2, LV_PART_MAIN);
          lv_obj_set_style_border_color(menu_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
          if (i == current_settings_selection) {
            lv_obj_set_style_border_color(menu_btn, lv_color_hex(0x00FF00), LV_PART_MAIN);
          }

          lv_obj_t *menu_label = lv_label_create(menu_btn);
          lv_label_set_text(menu_label, menu_items[i]);
          lv_obj_set_style_text_font(menu_label, &lv_font_montserrat_14, 0);
          lv_obj_set_style_text_color(menu_label, lv_color_hex(0xFFFFFF), 0);
          lv_obj_center(menu_label);
        }
        break;
      }

      case SETTINGS_UNITS: {
        lv_label_set_text(title, "Temperature Units");

        // Temperature unit selection buttons with highlighting
        lv_obj_t *celsius_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(celsius_btn, 120, 80);
        lv_obj_align(celsius_btn, LV_ALIGN_CENTER, -80, 0);
        lv_obj_set_style_bg_color(celsius_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
        lv_obj_set_style_border_width(celsius_btn, 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(celsius_btn, use_celsius ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF6B35), LV_PART_MAIN);

        lv_obj_t *celsius_label = lv_label_create(celsius_btn);
        lv_label_set_text(celsius_label, "C\nCelsius");
        lv_obj_set_style_text_font(celsius_label, &lv_font_montserrat_18, 0);
        lv_obj_center(celsius_label);

        lv_obj_t *fahrenheit_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(fahrenheit_btn, 120, 80);
        lv_obj_align(fahrenheit_btn, LV_ALIGN_CENTER, 80, 0);
        lv_obj_set_style_bg_color(fahrenheit_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
        lv_obj_set_style_border_width(fahrenheit_btn, 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(fahrenheit_btn, !use_celsius ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF6B35), LV_PART_MAIN);

        lv_obj_t *fahrenheit_label = lv_label_create(fahrenheit_btn);
        lv_label_set_text(fahrenheit_label, "F\nFahrenheit");
        lv_obj_set_style_text_font(fahrenheit_label, &lv_font_montserrat_18, 0);
        lv_obj_center(fahrenheit_label);

        // Selection indicator showing which unit is currently active
        lv_obj_t *current_indicator = lv_label_create(settings_screen);
        lv_label_set_text(current_indicator, use_celsius ? "← Current: Celsius (C)" : "Current: Fahrenheit (F) →");
        lv_obj_set_style_text_color(current_indicator, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_text_font(current_indicator, &lv_font_montserrat_16, 0);
        lv_obj_align(current_indicator, LV_ALIGN_CENTER, 0, 50);

        lv_obj_t *instruction = lv_label_create(settings_screen);
        lv_label_set_text(instruction, "Btn1: Select Celsius     Btn2: Select F     Key: Accept & Return");
        lv_obj_set_style_text_color(instruction, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_text_font(instruction, &lv_font_montserrat_12, 0);
        lv_obj_align(instruction, LV_ALIGN_BOTTOM_MID, 0, -20);
        break;
      }

      case SETTINGS_AUDIO: {
        lv_label_set_text(title, "Audio Settings");
        // Sound enable/disable
        lv_obj_t *sound_title = lv_label_create(settings_screen);
        lv_label_set_text(sound_title, "Sound Alerts");
        lv_obj_set_style_text_font(sound_title, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(sound_title, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(sound_title, LV_ALIGN_TOP_MID, 0, 60);

        lv_obj_t *sound_on_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(sound_on_btn, 100, 50);
        lv_obj_align(sound_on_btn, LV_ALIGN_CENTER, -60, 20);
        lv_obj_set_style_bg_color(sound_on_btn, sound_enabled ? lv_color_hex(0x00AA00) : lv_color_hex(0x666666), LV_PART_MAIN);

        lv_obj_t *on_label = lv_label_create(sound_on_btn);
        lv_label_set_text(on_label, "ON");
        lv_obj_set_style_text_font(on_label, &lv_font_montserrat_16, 0);
        lv_obj_center(on_label);

        lv_obj_t *sound_off_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(sound_off_btn, 100, 50);
        lv_obj_align(sound_off_btn, LV_ALIGN_CENTER, 60, 20);
        lv_obj_set_style_bg_color(sound_off_btn, !sound_enabled ? lv_color_hex(0xAA0000) : lv_color_hex(0x666666), LV_PART_MAIN);

        lv_obj_t *off_label = lv_label_create(sound_off_btn);
        lv_label_set_text(off_label, "OFF");
        lv_obj_set_style_text_font(off_label, &lv_font_montserrat_16, 0);
        lv_obj_center(off_label);

        lv_obj_t *instruction = lv_label_create(settings_screen);
        lv_label_set_text(instruction, "Key: Toggle Sound");
        lv_obj_set_style_text_color(instruction, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(instruction, LV_ALIGN_BOTTOM_MID, 0, -20);
        break;
      }

      case SETTINGS_ALERTS: {
        lv_label_set_text(title, "Temperature Alerts");
        // Temperature thresholds
        lv_obj_t *low_temp_label = lv_label_create(settings_screen);
        lv_label_set_text(low_temp_label, "Cold Alert:");
        lv_obj_set_style_text_color(low_temp_label, lv_color_hex(0x0099FF), 0);
        lv_obj_align(low_temp_label, LV_ALIGN_TOP_LEFT, 20, 60);

        lv_obj_t *high_temp_label = lv_label_create(settings_screen);
        lv_label_set_text(high_temp_label, "Hot Alert:");
        lv_obj_set_style_text_color(high_temp_label, lv_color_hex(0xFF6600), 0);
        lv_obj_align(high_temp_label, LV_ALIGN_TOP_LEFT, 20, 100);

        // Actual threshold values (would need to be updated dynamically)
        char low_str[16];
        snprintf(low_str, sizeof(low_str), "%.1f C", low_temp_threshold);
        lv_obj_t *low_value = lv_label_create(settings_screen);
        lv_label_set_text_fmt(low_value, low_str);
        lv_obj_set_style_text_color(low_value, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(low_value, LV_ALIGN_TOP_LEFT, 150, 60);

        char high_str[16];
        snprintf(high_str, sizeof(high_str), "%.1f C", high_temp_threshold);
        lv_obj_t *high_value = lv_label_create(settings_screen);
        lv_label_set_text_fmt(high_value, high_str);
        lv_obj_set_style_text_color(high_value, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(high_value, LV_ALIGN_TOP_LEFT, 150, 100);

        lv_obj_t *instruction = lv_label_create(settings_screen);
        lv_label_set_text(instruction, "Key: Toggle Alerts");
        lv_obj_set_style_text_color(instruction, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(instruction, LV_ALIGN_BOTTOM_MID, 0, -20);
        break;
      }

      case SETTINGS_DISPLAY: {
        lv_label_set_text(title, "Display Settings");

        // Brightness slider container
        lv_obj_t *brightness_container = lv_obj_create(settings_screen);
        lv_obj_set_size(brightness_container, 280, 80);
        lv_obj_align(brightness_container, LV_ALIGN_CENTER, 0, -20);
        lv_obj_set_style_bg_color(brightness_container, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_border_width(brightness_container, 2, 0);
        lv_obj_set_style_border_color(brightness_container, lv_color_hex(0xFF6B35), 0);
        lv_obj_set_style_radius(brightness_container, 10, 0);

        // Brightness label
        lv_obj_t *brightness_title = lv_label_create(brightness_container);
        lv_label_set_text(brightness_title, "Screen Brightness");
        lv_obj_set_style_text_font(brightness_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(brightness_title, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(brightness_title, LV_ALIGN_TOP_MID, 0, 8);

        // Brightness slider
        brightness_slider = lv_slider_create(brightness_container);
        lv_obj_set_size(brightness_slider, 200, 20);
        lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, 10);
        lv_slider_set_range(brightness_slider, 0, 255);
        lv_slider_set_value(brightness_slider, brightness_level, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0xFF6B35), LV_PART_INDICATOR);
        lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // Brightness value display
        char brightness_str[10];
        snprintf(brightness_str, sizeof(brightness_str), "%d", brightness_level);
        brightness_label = lv_label_create(brightness_container);
        lv_label_set_text(brightness_label, brightness_str);
        lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(brightness_label, lv_color_hex(0x00FF00), 0);
        lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, 45);

        lv_obj_t *instruction = lv_label_create(settings_screen);
        lv_label_set_text(instruction, "Key: Return to Menu");
        lv_obj_set_style_text_color(instruction, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(instruction, LV_ALIGN_BOTTOM_MID, 0, -20);
        break;
      }

      case SETTINGS_EXIT: {
        lv_label_set_text(title, "Save & Exit");
        // Exit confirmation
        lv_obj_t *question = lv_label_create(settings_screen);
        lv_label_set_text(question, "Save settings\nbefore exiting?");
        lv_obj_set_style_text_font(question, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(question, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(question, LV_ALIGN_CENTER, 0, -30);

        lv_obj_t *cancel_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(cancel_btn, 100, 50);
        lv_obj_align(cancel_btn, LV_ALIGN_CENTER, -60, 40);
        lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_set_style_border_width(cancel_btn, exit_selection_cancel ? 3 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(cancel_btn, lv_color_hex(0x00FF00), LV_PART_MAIN);

        lv_obj_t *cancel_label = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_label, "CANCEL");
        lv_obj_center(cancel_label);

        lv_obj_t *save_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(save_btn, 100, 50);
        lv_obj_align(save_btn, LV_ALIGN_CENTER, 60, 40);
        lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_set_style_border_width(save_btn, exit_selection_cancel ? 1 : 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(save_btn, !exit_selection_cancel ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF6B35), LV_PART_MAIN);

        lv_obj_t *save_label = lv_label_create(save_btn);
        lv_label_set_text(save_label, "SAVE");
        lv_obj_center(save_label);

        lv_obj_t *instruction = lv_label_create(settings_screen);
        lv_label_set_text(instruction, "Btn1: Select Cancel    Btn2: Select Save");
        lv_obj_set_style_text_color(instruction, lv_color_hex(0xCCCCCC), 0);
        lv_obj_align(instruction, LV_ALIGN_BOTTOM_MID, 0, -20);
        break;
      }
    }

    // Hardware control indicators
    lv_obj_t *control_indicator = lv_label_create(settings_screen);
    lv_label_set_text(control_indicator, "Btn1: Navigate    Btn2: Back    Key: Select");
    lv_obj_set_style_text_color(control_indicator, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(control_indicator, &lv_font_montserrat_12, 0);
    lv_obj_align(control_indicator, LV_ALIGN_BOTTOM_MID, 0, -12);
  }
}

// LVGL tick task - simplified to avoid double-checking timing
static void lvgl_tick_task(void *arg) {
  (void)arg;
  // Simply call lv_task_handler since timing is controlled in loop()
  lv_task_handler(); // Process LVGL tasks and handle screen refresh
}

// LVGL task (removed - using main loop refresh instead)

void setup() {
  Serial.begin(115200);
  // Initialize M5Stack
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.println("M5Stack CoreS3 initialized");

  // Initialize NCIR sensor
  if (!mlx.begin()) {
    Serial.println("Error initializing MLX90614 sensor!");
    while (1);
  }
  Serial.println("NCIR sensor initialized");
  
  // Test sensor reading
  float test_obj = mlx.readObjectTempF();
  float test_amb = mlx.readAmbientTempF();
  Serial.printf("Sensor test - Object: %.1f°C, Ambient: %.1f°C\n", test_obj, test_amb);

  // Initialize LVGL
  Serial.println("Before LVGL init");
  lv_init();
  Serial.println("After LVGL init");
  Serial.printf("LVGL %d.%d.%d\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
  Serial.println("Before m5gfx_lvgl_init");
  m5gfx_lvgl_init();
  Serial.println("After m5gfx_lvgl_init");
  Serial.println("LVGL setup complete");

  // LVGL task creation removed - using main loop refresh instead
  Serial.println("LVGL refresh will be handled in main loop");

  // Setup hardware (buttons, interrupts, preferences)
  setup_hardware();
  load_preferences();

  // Create UI screens
  create_main_menu_ui();
  Serial.println("Main menu UI created");

  create_temp_display_ui();
  Serial.println("Temp display UI created");

  create_temp_gauge_ui();
  Serial.println("Temp gauge UI created");

  create_settings_ui();
  Serial.println("Settings UI created");

  // Load the initial main menu screen
  lv_screen_load(main_menu_screen);

  // Force a refresh to ensure display updates
  lv_refr_now(NULL);
  Serial.println("Display refreshed");

  Serial.println("Multi-screen UI created");
  Serial.println("M5Stack CoreS3 NCIR UI Ready!");
}

void loop() {
  M5.update();

  // Improved LVGL refresh timing
  uint32_t current_time = millis();
  if (current_time - lastLvglTick >= screenTickPeriod) {
    lvgl_tick_task(NULL);
    lastLvglTick = current_time;
  }

  // Handle button polling for main menu navigation
  if (current_screen == SCREEN_MAIN_MENU) {
    static bool last_key_state = HIGH;
    unsigned long current_time = millis();

    // Read current key button state
    bool current_key_state = digitalRead(KEY_PIN);

    // Check Key (GPIO8) - Go to settings menu
    if (current_key_state == LOW && last_key_state == HIGH && current_time - last_button_time[2] >= DEBOUNCE_DELAY) {
      Serial.println("Key pressed (Main menu - go to settings)");
      switch_to_screen(SCREEN_SETTINGS); // Go to settings menu
      last_button_time[2] = current_time;
    }

    // Update last state
    last_key_state = current_key_state;
  }

  // Handle button polling for settings navigation
  if (current_screen == SCREEN_SETTINGS) {
    static bool last_button1_state = HIGH;
    static bool last_button2_state = HIGH;
    static bool last_key_state = HIGH;
    unsigned long current_time = millis();

    // Read current button states
    bool current_button1_state = digitalRead(BUTTON1_PIN);
    bool current_button2_state = digitalRead(BUTTON2_PIN);
    bool current_key_state = digitalRead(KEY_PIN);

    // Check Button 1 (GPIO17) - Navigate forward/Select Cancel/Select Celsius
    if (current_button1_state == LOW && last_button1_state == HIGH && current_time - last_button_time[0] >= DEBOUNCE_DELAY) {
      Serial.println("Button 1 pressed (Settings navigation)");
      if (current_settings_screen == SETTINGS_MENU) {
        // Navigate forward through menu items (0-4)
        current_settings_selection = (current_settings_selection + 1) % 5;
        switch_to_settings_screen(); // Refresh UI to show new selection
      } else if (current_settings_screen == SETTINGS_UNITS) {
        // In units page, select Celsius
        use_celsius = true;
        Serial.printf("Temperature units set to: %s\n", use_celsius ? "Celsius" : "Fahrenheit");
        save_preferences();
        switch_to_settings_screen(); // Refresh UI to show selection
      } else if (current_settings_screen == SETTINGS_EXIT) {
        // In exit tab, select Cancel
        exit_selection_cancel = true;
        switch_to_settings_screen(); // Refresh UI to show selection
      }
      last_button_time[0] = current_time;
    }

    // Check Button 2 (GPIO18) - Navigate backward/Select Save/Select Fahrenheit
    if (current_button2_state == LOW && last_button2_state == HIGH && current_time - last_button_time[1] >= DEBOUNCE_DELAY) {
      Serial.println("Button 2 pressed (Settings navigation)");
      if (current_settings_screen == SETTINGS_MENU) {
        // Navigate backward through menu items (0-4)
        current_settings_selection = (current_settings_selection - 1 + 5) % 5;
        switch_to_settings_screen(); // Refresh UI to show new selection
      } else if (current_settings_screen == SETTINGS_UNITS) {
        // In units page, select Fahrenheit
        use_celsius = false;
        Serial.printf("Temperature units set to: %s\n", use_celsius ? "Celsius" : "Fahrenheit");
        save_preferences();
        switch_to_settings_screen(); // Refresh UI to show selection
      } else if (current_settings_screen == SETTINGS_EXIT) {
        // In exit tab, select Save
        exit_selection_cancel = false;
        switch_to_settings_screen(); // Refresh UI to show selection
      }
      last_button_time[1] = current_time;
    }

    // Check Key (GPIO8) - Accept/Confirm selection and return to main menu
    if (current_key_state == LOW && last_key_state == HIGH && current_time - last_button_time[2] >= DEBOUNCE_DELAY) {
      Serial.println("Key pressed (Settings confirm & return)");
      if (current_settings_screen == SETTINGS_MENU) {
        // Enter selected menu item
        SettingsScreen selected_screen;
        switch (current_settings_selection) {
          case 0: selected_screen = SETTINGS_UNITS; break;
          case 1: selected_screen = SETTINGS_AUDIO; break;
          case 2: selected_screen = SETTINGS_DISPLAY; break;
          case 3: selected_screen = SETTINGS_ALERTS; break;
          case 4: selected_screen = SETTINGS_EXIT; break;
        }
        current_settings_screen = selected_screen;
        switch_to_settings_screen(); // Show the selected settings page
      } else if (current_settings_screen == SETTINGS_UNITS) {
        // Unit is already selected by Button 1/2, just return to main menu
        Serial.printf("Temperature units confirmed: %s - returning to main menu\n", use_celsius ? "Celsius" : "Fahrenheit");
        save_preferences(); // Save the current selection
        switch_to_screen(SCREEN_MAIN_MENU); // Return to main menu
      } else if (current_settings_screen == SETTINGS_AUDIO) {
        // Toggle sound alerts and return
        sound_enabled = !sound_enabled;
        Serial.printf("Sound alerts toggled to: %s - returning to main menu\n", sound_enabled ? "ON" : "OFF");
        save_preferences();
        switch_to_screen(SCREEN_MAIN_MENU); // Return to main menu
      } else if (current_settings_screen == SETTINGS_DISPLAY) {
        // Brightness is adjusted via slider, just return to main menu
        Serial.printf("Brightness set to: %d - returning to main menu\n", brightness_level);
        save_preferences();
        switch_to_screen(SCREEN_MAIN_MENU); // Return to main menu
      } else if (current_settings_screen == SETTINGS_ALERTS) {
        // Toggle alerts and return
        alerts_enabled = !alerts_enabled;
        Serial.printf("Temperature alerts toggled to: %s - returning to main menu\n", alerts_enabled ? "ON" : "OFF");
        save_preferences();
        switch_to_screen(SCREEN_MAIN_MENU); // Return to main menu
      } else if (current_settings_screen == SETTINGS_EXIT) {
        // Execute exit action based on selection
        if (exit_selection_cancel) {
          Serial.println("Exit cancelled - returning to main menu without saving");
          // Return to main menu without saving changes (already have current settings)
          switch_to_screen(SCREEN_MAIN_MENU);
        } else {
          Serial.println("Exit with save - saving preferences and returning to main menu");
          save_preferences();
          switch_to_screen(SCREEN_MAIN_MENU);
        }
      }
      last_button_time[2] = current_time;
    }

    // Update last states
    last_button1_state = current_button1_state;
    last_button2_state = current_button2_state;
    last_key_state = current_key_state;
  }

  // Update temperature at specified rate
  if (millis() - last_update >= update_rate) {
    update_temperature_reading();

    // Update current screen display immediately
    if (current_screen == SCREEN_TEMP_DISPLAY) {
      update_temp_display_screen();
    } else if (current_screen == SCREEN_TEMP_GAUGE) {
      update_temp_gauge_screen();
    }

    // Update main menu temperature preview
    if (current_screen == SCREEN_MAIN_MENU && temp_preview_label) {
      char temp_str[64];
      float display_obj_temp = use_celsius ? current_object_temp : (current_object_temp * 9.0/5.0 + 32.0);
      float display_amb_temp = use_celsius ? current_ambient_temp : (current_ambient_temp * 9.0/5.0 + 32.0);
      snprintf(temp_str, sizeof(temp_str), "Current: %.1f°%c | Ambient: %.1f°%c",
              display_obj_temp, use_celsius ? 'C' : 'F',
              display_amb_temp, use_celsius ? 'C' : 'F');
      lv_label_set_text(temp_preview_label, temp_str);
    }

    check_temp_alerts();
    last_update = millis();
  }

  // Reduced delay to prevent watchdog issues while maintaining responsiveness
  // Reduced from 10ms to 5ms to work better with the slower refresh rate
  delay(5);
}

// Setup hardware pins and button polling
void setup_hardware() {
  // Configure LED pin as output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Start with LED off

  // Configure button pins for digital read polling
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(KEY_PIN, INPUT_PULLUP);

  // Initialize speaker
  M5.Speaker.begin();
  Serial.println("Speaker initialized");
  Serial.println("Hardware button pins configured for polling");
}

// Load settings from persistent storage
void load_preferences() {
  preferences.begin("ncir_monitor", false);

  use_celsius = preferences.getBool("use_celsius", true);
  update_rate = preferences.getInt("update_rate", 1000);
  brightness_level = preferences.getInt("brightness", 128);
  sound_enabled = preferences.getBool("sound_enabled", true);
  sound_volume = preferences.getInt("sound_volume", 70);
  alerts_enabled = preferences.getBool("alerts_enabled", true);
  low_temp_threshold = preferences.getFloat("low_temp_threshold", 10.0);
  high_temp_threshold = preferences.getFloat("high_temp_threshold", 40.0);

  preferences.end();
}

// Save settings to persistent storage
void save_preferences() {
  preferences.begin("ncir_monitor", false);

  preferences.putBool("use_celsius", use_celsius);
  preferences.putInt("update_rate", update_rate);
  preferences.putInt("brightness", brightness_level);
  preferences.putBool("sound_enabled", sound_enabled);
  preferences.putInt("sound_volume", sound_volume);
  preferences.putBool("alerts_enabled", alerts_enabled);
  preferences.putFloat("low_temp_threshold", low_temp_threshold);
  preferences.putFloat("high_temp_threshold", high_temp_threshold);

  preferences.end();
}

// Switch between screens instantly (no animation)
void switch_to_screen(ScreenState new_screen) {
  if (current_screen == new_screen) return;

  current_screen = new_screen;

  switch (new_screen) {
    case SCREEN_MAIN_MENU:
      lv_screen_load(main_menu_screen);
      break;
    case SCREEN_TEMP_DISPLAY:
      lv_screen_load(temp_display_screen);
      update_temp_display_screen();
      break;
    case SCREEN_TEMP_GAUGE:
      lv_screen_load(temp_gauge_screen);
      update_temp_gauge_screen();
      break;
    case SCREEN_SETTINGS:
      lv_screen_load(settings_screen);
      current_settings_screen = SETTINGS_MENU; // Reset to menu when entering settings
      switch_to_settings_screen(); // Populate settings screen with menu
      break;
  }
}

// Create main menu screen with improved layout and modern design
void create_main_menu_ui() {
  main_menu_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(main_menu_screen, lv_color_hex(0x0f1419), 0); // Dark background
  lv_obj_remove_flag(main_menu_screen, LV_OBJ_FLAG_SCROLLABLE);

  // Modern header section with gradient effect - optimized height and positioning
  lv_obj_t *header_bg = lv_obj_create(main_menu_screen);
  lv_obj_set_size(header_bg, 320, 85);
  lv_obj_align(header_bg, LV_ALIGN_TOP_MID, 0, 0); // Back to standard position to avoid cutoff
  lv_obj_set_style_bg_color(header_bg, lv_color_hex(0x1a1a2e), 0);
  lv_obj_remove_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE); // Make header container non-scrollable

  // Header accent line - adjusted position for optimized header height
  lv_obj_t *header_accent = lv_obj_create(main_menu_screen);
  lv_obj_set_size(header_accent, 320, 2);
  lv_obj_align(header_accent, LV_ALIGN_TOP_MID, 0, 81); // Back to standard position
  lv_obj_set_style_bg_color(header_accent, lv_color_hex(0xFF6B35), 0);

  // Main title with enhanced typography - repositioned for better layout
  menu_title = lv_label_create(header_bg);
  lv_label_set_text(menu_title, "NCIR Monitor");
  lv_obj_set_style_text_font(menu_title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(menu_title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(menu_title, LV_ALIGN_TOP_MID, 0, 8);

  // Subtitle with device status - repositioned to ensure full visibility
  lv_obj_t *subtitle = lv_label_create(header_bg);
  lv_label_set_text(subtitle, "Non-Contact Temperature Monitor");
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xB0B0B0), 0);
  lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 45);

  // Ensure main screen is not scrollable
  lv_obj_remove_flag(main_menu_screen, LV_OBJ_FLAG_SCROLLABLE);

  // Additional check to ensure no child objects are scrollable
  lv_obj_remove_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE);

  // Main action buttons in an improved horizontal layout - raised up with more spacing
  int button_width = 90;
  int button_height = 100;
  int button_spacing = 10; // Increased spacing between buttons
  int button_y_offset = 25; // Raise buttons up significantly

  // Left: Temperature Display (Orange theme)
  temp_display_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(temp_display_btn, button_width, button_height);
  lv_obj_align(temp_display_btn, LV_ALIGN_LEFT_MID, button_spacing, button_y_offset);
  lv_obj_set_style_bg_color(temp_display_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(temp_display_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(temp_display_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_set_style_border_width(temp_display_btn, 4, LV_PART_MAIN);
  lv_obj_set_style_border_color(temp_display_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
  lv_obj_set_style_radius(temp_display_btn, 15, 0);
  lv_obj_set_style_shadow_width(temp_display_btn, 5, 0);
  lv_obj_set_style_shadow_color(temp_display_btn, lv_color_hex(0xFF6B35), 0);
  lv_obj_add_event_cb(temp_display_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_TEMP_DISPLAY);

  lv_obj_t *temp_display_icon = lv_label_create(temp_display_btn);
  lv_label_set_text(temp_display_icon, "🌡️");
  lv_obj_set_style_text_font(temp_display_icon, &lv_font_montserrat_24, 0);
  lv_obj_align(temp_display_icon, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *temp_display_label = lv_label_create(temp_display_btn);
  lv_label_set_text(temp_display_label, "Display");
  lv_obj_set_style_text_font(temp_display_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(temp_display_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(temp_display_label, LV_ALIGN_BOTTOM_MID, 0, -10);

  // Center: Temperature Gauge (Blue theme)
  temp_gauge_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(temp_gauge_btn, button_width, button_height);
  lv_obj_align(temp_gauge_btn, LV_ALIGN_CENTER, 0, button_y_offset);
  lv_obj_set_style_bg_color(temp_gauge_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(temp_gauge_btn, lv_color_hex(0x4285F4), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(temp_gauge_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_set_style_border_width(temp_gauge_btn, 4, LV_PART_MAIN);
  lv_obj_set_style_border_color(temp_gauge_btn, lv_color_hex(0x4285F4), LV_PART_MAIN);
  lv_obj_set_style_radius(temp_gauge_btn, 15, 0);
  lv_obj_set_style_shadow_width(temp_gauge_btn, 5, 0);
  lv_obj_set_style_shadow_color(temp_gauge_btn, lv_color_hex(0x4285F4), 0);
  lv_obj_add_event_cb(temp_gauge_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_TEMP_GAUGE);

  // Create gauge animation using LVGL example pattern
  lv_obj_t *gauge_lottie = lv_lottie_create(temp_gauge_btn);
  
  // Load gauge animation from embedded data (following LVGL example pattern)
  lv_lottie_set_src_data(gauge_lottie, gauge_animation, gauge_animation_size);
  
  // Set up buffer following LVGL example
#if LV_DRAW_BUF_ALIGN == 4 && LV_DRAW_BUF_STRIDE_ALIGN == 1
  /*If there are no special requirements, just declare a buffer
    x4 because the Lottie is rendered in ARGB8888_PREMULTIPLIED format*/
  static uint8_t buf[80 * 50 * 4];
  lv_lottie_set_buffer(gauge_lottie, 80, 50, buf);
#else
  /*For GPUs and special alignment/stride setting use a draw_buf instead*/
  LV_DRAW_BUF_DEFINE_STATIC(draw_buf, 80, 50, LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED);
  lv_lottie_set_draw_buf(gauge_lottie, &draw_buf);
#endif

  lv_obj_center(gauge_lottie);

  lv_obj_t *temp_gauge_label = lv_label_create(temp_gauge_btn);
  lv_label_set_text(temp_gauge_label, "Gauge");
  lv_obj_set_style_text_font(temp_gauge_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(temp_gauge_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(temp_gauge_label, LV_ALIGN_BOTTOM_MID, 0, -10);

  // Right: Settings (Purple theme)
  settings_menu_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(settings_menu_btn, button_width, button_height);
  lv_obj_align(settings_menu_btn, LV_ALIGN_RIGHT_MID, -button_spacing, button_y_offset);
  lv_obj_set_style_bg_color(settings_menu_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(settings_menu_btn, lv_color_hex(0x9b59b6), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(settings_menu_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_set_style_border_width(settings_menu_btn, 4, LV_PART_MAIN);
  lv_obj_set_style_border_color(settings_menu_btn, lv_color_hex(0x9b59b6), LV_PART_MAIN);
  lv_obj_set_style_radius(settings_menu_btn, 15, 0);
  lv_obj_set_style_shadow_width(settings_menu_btn, 5, 0);
  lv_obj_set_style_shadow_color(settings_menu_btn, lv_color_hex(0x9b59b6), 0);
  lv_obj_add_event_cb(settings_menu_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_SETTINGS);

  lv_obj_t *settings_icon = lv_label_create(settings_menu_btn);
  lv_label_set_text(settings_icon, "⚙️");
  lv_obj_set_style_text_font(settings_icon, &lv_font_montserrat_24, 0);
  lv_obj_align(settings_icon, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *settings_label = lv_label_create(settings_menu_btn);
  lv_label_set_text(settings_label, "Settings");
  lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(settings_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(settings_label, LV_ALIGN_BOTTOM_MID, 0, -10);
  
  // Remove bottom status bar and temperature preview on main menu
  temp_preview_label = NULL;
}

// Create modern mobile app-style temperature display screen
void create_temp_display_ui() {
  temp_display_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(temp_display_screen, lv_color_hex(0xf5f5f5), 0); // Light gray background
  lv_obj_remove_flag(temp_display_screen, LV_OBJ_FLAG_SCROLLABLE);

  // Header matching main menu style
  lv_obj_t *header = lv_obj_create(temp_display_screen);
  lv_obj_set_size(header, 320, 80);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x1a1a2e), 0); // Match main menu header
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);

  // Header accent line matching main menu
  lv_obj_t *header_accent = lv_obj_create(temp_display_screen);
  lv_obj_set_size(header_accent, 320, 2);
  lv_obj_align(header_accent, LV_ALIGN_TOP_MID, 0, 78);
  lv_obj_set_style_bg_color(header_accent, lv_color_hex(0xFF6B35), 0); // Orange accent

  // Header title with modern typography
  lv_obj_t *header_title = lv_label_create(header);
  lv_label_set_text(header_title, "Temperature");
  lv_obj_set_style_text_color(header_title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(header_title, &lv_font_montserrat_20, 0);
  lv_obj_align(header_title, LV_ALIGN_TOP_MID, 0, 15);

  // Subtitle matching main menu style
  lv_obj_t *header_subtitle = lv_label_create(header);
  lv_label_set_text(header_subtitle, "Real-time monitoring");
  lv_obj_set_style_text_color(header_subtitle, lv_color_hex(0xB0B0B0), 0); // Match main menu subtitle
  lv_obj_set_style_text_font(header_subtitle, &lv_font_montserrat_12, 0);
  lv_obj_align(header_subtitle, LV_ALIGN_TOP_MID, 0, 45);

  // Main temperature display - enhanced circular style
  temp_circle = lv_obj_create(temp_display_screen);
  lv_obj_set_size(temp_circle, 180, 180);
  lv_obj_align(temp_circle, LV_ALIGN_CENTER, 0, -5);
  lv_obj_set_style_bg_color(temp_circle, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(temp_circle, 6, 0);
  lv_obj_set_style_border_color(temp_circle, lv_color_hex(0xFF6B35), 0); // Match main menu orange
  lv_obj_set_style_radius(temp_circle, 90, 0); // Perfect circle
  lv_obj_set_style_shadow_width(temp_circle, 20, 0);
  lv_obj_set_style_shadow_color(temp_circle, lv_color_hex(0x30000000), 0);

  // Large temperature reading - bigger and bolder
  object_temp_label = lv_label_create(temp_circle);
  lv_label_set_text(object_temp_label, "--°");
  lv_obj_set_style_text_color(object_temp_label, lv_color_hex(0xFF6B35), 0); // Match main menu orange
  lv_obj_set_style_text_font(object_temp_label, &lv_font_montserrat_24, 0);
  lv_obj_align(object_temp_label, LV_ALIGN_CENTER, 0, -15);

  // Temperature unit - dynamic based on user preference
  temp_unit_label = lv_label_create(temp_circle);
  lv_label_set_text(temp_unit_label, use_celsius ? "Celsius" : "Fahrenheit");
  lv_obj_set_style_text_color(temp_unit_label, lv_color_hex(0x9E9E9E), 0);
  lv_obj_set_style_text_font(temp_unit_label, &lv_font_montserrat_14, 0);
  lv_obj_align(temp_unit_label, LV_ALIGN_CENTER, 0, 20);

  // Ambient temperature - enhanced card style
  lv_obj_t *ambient_card = lv_obj_create(temp_display_screen);
  lv_obj_set_size(ambient_card, 280, 70);
  lv_obj_align(ambient_card, LV_ALIGN_CENTER, 0, 95);
  lv_obj_set_style_bg_color(ambient_card, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(ambient_card, 0, 0);
  lv_obj_set_style_radius(ambient_card, 16, 0);
  lv_obj_set_style_shadow_width(ambient_card, 12, 0);
  lv_obj_set_style_shadow_color(ambient_card, lv_color_hex(0x15000000), 0);

  // Ambient icon - larger and more prominent
  lv_obj_t *ambient_icon = lv_label_create(ambient_card);
  lv_label_set_text(ambient_icon, "");
  lv_obj_set_style_text_font(ambient_icon, &lv_font_montserrat_20, 0);
  lv_obj_align(ambient_icon, LV_ALIGN_LEFT_MID, 20, 0);

  // Ambient temperature text - better typography
  ambient_temp_label = lv_label_create(ambient_card);
  lv_label_set_text(ambient_temp_label, "Ambient: --C");
  lv_obj_set_style_text_color(ambient_temp_label, lv_color_hex(0x424242), 0);
  lv_obj_set_style_text_font(ambient_temp_label, &lv_font_montserrat_18, 0);
  lv_obj_align(ambient_temp_label, LV_ALIGN_LEFT_MID, 55, 0);

  // Status indicator - modern pill style
  lv_obj_t *status_pill = lv_obj_create(temp_display_screen);
  lv_obj_set_size(status_pill, 120, 30);
  lv_obj_align(status_pill, LV_ALIGN_CENTER, 0, 180);
  lv_obj_set_style_bg_color(status_pill, lv_color_hex(0x4CAF50), 0); // Green
  lv_obj_set_style_border_width(status_pill, 0, 0);
  lv_obj_set_style_radius(status_pill, 15, 0);

  temp_status_label = lv_label_create(status_pill);
  lv_label_set_text(temp_status_label, "● Ready");
  lv_obj_set_style_text_color(temp_status_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(temp_status_label, &lv_font_montserrat_12, 0);
  lv_obj_center(temp_status_label);

  // Modern floating action button matching main menu colors
  temp_display_back_btn = lv_btn_create(temp_display_screen);
  lv_obj_set_size(temp_display_back_btn, 60, 60);
  lv_obj_align(temp_display_back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
  lv_obj_set_style_bg_color(temp_display_back_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN); // Match main menu orange
  lv_obj_set_style_border_width(temp_display_back_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(temp_display_back_btn, 30, 0); // Perfect circle
  lv_obj_set_style_shadow_width(temp_display_back_btn, 10, 0);
  lv_obj_set_style_shadow_color(temp_display_back_btn, lv_color_hex(0x20000000), 0);
  lv_obj_add_event_cb(temp_display_back_btn, temp_display_back_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_icon = lv_label_create(temp_display_back_btn);
  lv_label_set_text(back_icon, "←");
  lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(back_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(back_icon);
}

// Create modernized temperature gauge screen with alternative orange/blue theme
void create_temp_gauge_ui() {
  temp_gauge_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(temp_gauge_screen, lv_color_hex(0x0d1117), 0); // Dark GitHub-style background

  // Decorative header
  lv_obj_t *header_bg = lv_obj_create(temp_gauge_screen);
  lv_obj_set_size(header_bg, 320, 50);
  lv_obj_align(header_bg, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header_bg, lv_color_hex(0x161b22), 0); // Slightly lighter header

  lv_obj_t *header_border = lv_obj_create(temp_gauge_screen);
  lv_obj_set_size(header_border, 320, 2);
  lv_obj_align(header_border, LV_ALIGN_TOP_MID, 0, 48);
  lv_obj_set_style_bg_color(header_border, lv_color_hex(0x4285F4), 0); // Blue accent line

  lv_obj_t *title = lv_label_create(header_bg);
  lv_label_set_text(title, "Temperature Gauge");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 10, 0);

  // Modern gauge container with alternative styling
  lv_obj_t *gauge_container = lv_obj_create(temp_gauge_screen);
  lv_obj_set_size(gauge_container, 220, 140);
  lv_obj_align(gauge_container, LV_ALIGN_CENTER, 0, -30);
  lv_obj_set_style_bg_color(gauge_container, lv_color_hex(0x1e2936), 0); // Blue background
  lv_obj_set_style_border_width(gauge_container, 3, 0);
  lv_obj_set_style_border_color(gauge_container, lv_color_hex(0xFF6B35), 0); // Orange border
  lv_obj_set_style_radius(gauge_container, 20, 0);

  // Create the scale/gauge with enhanced styling
  temp_scale = lv_scale_create(gauge_container);
  lv_obj_set_size(temp_scale, 180, 180);
  lv_obj_align(temp_scale, LV_ALIGN_CENTER, 0, -10);
  lv_scale_set_mode(temp_scale, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(temp_scale, 0, 400); // 0°C to 400°C range for better readability
  lv_scale_set_angle_range(temp_scale, 270);
  lv_scale_set_rotation(temp_scale, 135);

  // Configure major ticks with better spacing
  lv_scale_set_total_tick_count(temp_scale, 41); // Every 10°C
  lv_scale_set_major_tick_every(temp_scale, 5); // Major tick every 50°C

  // Modern scale labels (show less frequent values for cleaner look)
  static const char *modern_labels[] = {"0C", "50C", "100C", "150C", "200C", "250C", "300C", "350C", "400C"};
  lv_scale_set_text_src(temp_scale, modern_labels);

  // Enhanced styling with alternative colors
  lv_obj_set_style_bg_color(temp_scale, lv_color_hex(0x2c3e50), LV_PART_MAIN); // Dark blue background
  lv_obj_set_style_bg_color(temp_scale, lv_color_hex(0x4285F4), LV_PART_ITEMS); // Blue major ticks
  lv_obj_set_style_bg_color(temp_scale, lv_color_hex(0x607D8B), LV_PART_INDICATOR); // Gray minor ticks

  // Add a prominent needle with enhanced styling
  temp_gauge_needle = lv_line_create(temp_gauge_screen);
  lv_obj_set_style_line_width(temp_gauge_needle, 5, 0);
  lv_obj_set_style_line_color(temp_gauge_needle, lv_color_hex(0xFF6B35), 0); // Orange needle to match theme
  lv_obj_set_style_line_rounded(temp_gauge_needle, true, 0); // Rounded line caps

  // Add a small center dot for the needle
  lv_obj_t *center_dot = lv_obj_create(temp_gauge_screen);
  lv_obj_set_size(center_dot, 8, 8);
  lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, -40); // Center of gauge
  lv_obj_set_style_bg_color(center_dot, lv_color_hex(0xFF6B35), 0);
  lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);

  // Enhanced temperature value display with container
  lv_obj_t *value_container = lv_obj_create(temp_gauge_screen);
  lv_obj_set_size(value_container, 120, 40);
  lv_obj_align(value_container, LV_ALIGN_BOTTOM_MID, 0, -65);
  lv_obj_set_style_bg_color(value_container, lv_color_hex(0x2c3e50), 0);
  lv_obj_set_style_border_width(value_container, 2, 0);
  lv_obj_set_style_border_color(value_container, lv_color_hex(0x9b59b6), 0); // Purple border
  lv_obj_set_style_radius(value_container, 8, 0);

  temp_gauge_value_label = lv_label_create(value_container);
  lv_label_set_text(temp_gauge_value_label, "0C");
  lv_obj_set_style_text_color(temp_gauge_value_label, lv_color_hex(0xFF6B35), 0); // Orange text
  lv_obj_set_style_text_font(temp_gauge_value_label, &lv_font_montserrat_20, 0);
  lv_obj_center(temp_gauge_value_label);

  // Modernized back button
  temp_gauge_back_btn = lv_btn_create(temp_gauge_screen);
  lv_obj_set_size(temp_gauge_back_btn, 90, 45);
  lv_obj_align(temp_gauge_back_btn, LV_ALIGN_BOTTOM_LEFT, 15, -15);
  lv_obj_set_style_bg_color(temp_gauge_back_btn, lv_color_hex(0x34495e), LV_PART_MAIN);
  lv_obj_set_style_border_width(temp_gauge_back_btn, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(temp_gauge_back_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
  lv_obj_add_event_cb(temp_gauge_back_btn, temp_gauge_back_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_label = lv_label_create(temp_gauge_back_btn);
  lv_label_set_text(back_label, "Back");
  lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
  lv_obj_center(back_label);

  // Hardware control indicator for gauge screen
  lv_obj_t *control_indicator = lv_label_create(temp_gauge_screen);
  lv_label_set_text(control_indicator, "Btn1: ---     Btn2: Menu     Key: ---");
  lv_obj_set_style_text_color(control_indicator, lv_color_hex(0x607D8B), 0);
  lv_obj_set_style_text_font(control_indicator, &lv_font_montserrat_12, 0);
  lv_obj_align(control_indicator, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// Create settings screen (replaced with page-based navigation - removed old LVGL tabview)
void create_settings_ui() {
  // Only create the base screen object - UI will be populated dynamically by switch_to_settings_screen()
  settings_screen = lv_obj_create(NULL);
}

// Read temperature from sensor (using Celsius directly - no conversion needed)
void update_temperature_reading() {
  current_object_temp = mlx.readObjectTempC();    // Use Celsius reading directly
  current_ambient_temp = mlx.readAmbientTempC();  // Use Celsius reading directly

  // Debug output every 5 seconds
  static unsigned long last_debug = 0;
  if (millis() - last_debug >= 5000) {
    Serial.printf("Temps - Object: %.1f°C, Ambient: %.1f°C\n", current_object_temp, current_ambient_temp);
    last_debug = millis();
  }
}

// Update temperature display screen
void update_temp_display_screen() {
  if (current_screen != SCREEN_TEMP_DISPLAY) return;

  float display_obj_temp, display_amb_temp;

  // Use appropriate sensor reading based on unit preference
  if (use_celsius) {
    display_obj_temp = current_object_temp;    // Already in Celsius from sensor
    display_amb_temp = current_ambient_temp;   // Already in Celsius from sensor
  } else {
    display_obj_temp = mlx.readObjectTempF();   // Read Fahrenheit directly from sensor
    display_amb_temp = mlx.readAmbientTempF();  // Read Fahrenheit directly from sensor
  }

  // REMOVE smoothing — only use direct sensor value
  // Update circular temperature display border color based on temperature ranges
  if (temp_circle) {
    float temp_for_color;
    if (use_celsius) {
      temp_for_color = (display_obj_temp * 9.0/5.0) + 32.0;
    } else {
      temp_for_color = display_obj_temp;
    }
    if (temp_for_color <= 0) {
      temp_for_color = 77.0; // Default to 77°F (25°C) if no reading
    }
    lv_color_t temp_color = get_temperature_color(temp_for_color);
    lv_obj_set_style_border_color(temp_circle, temp_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(object_temp_label, temp_color, 0);
  }

  // Show as whole number
  char temp_str[32];
  snprintf(temp_str, sizeof(temp_str), "%.0f°", display_obj_temp);
  lv_label_set_text(object_temp_label, temp_str);
  snprintf(temp_str, sizeof(temp_str), "Ambient: %.0f°%c", display_amb_temp, use_celsius ? 'C' : 'F');
  lv_label_set_text(ambient_temp_label, temp_str);

  // Update unit label dynamically
  if (temp_unit_label) {
    lv_label_set_text(temp_unit_label, use_celsius ? "Celsius" : "Fahrenheit");
  }

  // Update status indicator
  lv_label_set_text(temp_status_label, "● Active");
}

// Update temperature gauge screen
void update_temp_gauge_screen() {
  if (current_screen != SCREEN_TEMP_GAUGE) return;

  float display_temp;

  // Use appropriate sensor reading based on unit preference
  if (use_celsius) {
    display_temp = current_object_temp;    // Already in Celsius from sensor
  } else {
    display_temp = mlx.readObjectTempF();   // Read Fahrenheit directly from sensor
  }


  // Update needle position for the gauge
  if (temp_gauge_needle) {
    // Map temperature range (0 to 400°C) to angle range (135° to 405° for 270° arc)
    float temp_range = 400.0 - 0.0; // 400 degree temperature range (0-400°C)
    float normalized_temp = max(0.0f, min(display_temp, 400.0f)) / temp_range; // Clamp to 0.0 to 1.0
    float angle_range = 270.0; // degrees of arc
    float angle_offset = 135.0; // starting angle (bottom-left of circle)
    float needle_angle = angle_offset + (normalized_temp * angle_range);

    // Clamp to range
    needle_angle = max(135.0f, min(405.0f, needle_angle));

    // Calculate needle endpoint from center
    int center_x = 160; // Gauge center X (320/2)
    int center_y = 100; // Adjusted center Y for new gauge container
    int needle_length = 70; // Shorter needle for new container

    lv_point_precise_t points[2];
    points[0].x = center_x;
    points[0].y = center_y;

    float angle_rad = (needle_angle - 90.0) * PI / 180.0; // Convert to radians, adjust for 0°=up
    points[1].x = center_x + (needle_length * cos(angle_rad));
    points[1].y = center_y + (needle_length * sin(angle_rad));

    lv_line_set_points(temp_gauge_needle, points, 2);
  }

  // Update temperature value label with whole number
  char temp_str[32];
  snprintf(temp_str, sizeof(temp_str), "%.0f%c", display_temp, use_celsius ? 'C' : 'F');
  lv_label_set_text(temp_gauge_value_label, temp_str);
}

// Play beep sound using built-in speaker
void play_beep(int frequency, int duration) {
  if (!sound_enabled) return;

  // Use M5 Speaker instead of raw PWM
  M5.Speaker.tone(frequency, duration, 0, true);
}

// Get color based on temperature ranges
// Temperature ranges: <480°F = Snowy Blue, 480-560°F = Lava Yellow, 561-620°F = Green, >620°F = Lava Red
lv_color_t get_temperature_color(float temperature) {
  Serial.printf("get_temperature_color: %.1f°F\n", temperature);
  
  if (temperature < 480) {
    Serial.println("Color: Snowy Blue");
    return lv_color_hex(0x87CEEB); // Snowy blue
  } else if (temperature >= 480 && temperature <= 560) {
    Serial.println("Color: Lava Yellow");
    return lv_color_hex(0xFFA500); // Lava yellow
  } else if (temperature >= 561 && temperature <= 620) {
    Serial.println("Color: Green");
    return lv_color_hex(0x00FF00); // Green
  } else {
    Serial.println("Color: Lava Red");
    return lv_color_hex(0xFF4500); // Lava red
  }
}

// Check for temperature alerts
void check_temp_alerts() {
  if (!alerts_enabled) return;

  static bool low_alert_triggered = false;
  static bool high_alert_triggered = false;

  // Check low temperature alert
  if (current_object_temp <= low_temp_threshold && !low_alert_triggered) {
    play_beep(800, 300); // Low frequency beep
    delay(100);
    play_beep(800, 300); // Repeat beep
    low_alert_triggered = true;
    digitalWrite(LED_PIN, HIGH); // Turn on LED
    Serial.printf("Low temperature alert: %.1f°C <= %.1f°C\n", current_object_temp, low_temp_threshold);
  } else if (current_object_temp > low_temp_threshold + 2.0) { // Hysteresis
    low_alert_triggered = false;
    digitalWrite(LED_PIN, LOW);
  }

  // Check high temperature alert
  if (current_object_temp >= high_temp_threshold && !high_alert_triggered) {
    play_beep(1200, 500); // High frequency beep
    delay(100);
    play_beep(1200, 500); // Repeat beep
    high_alert_triggered = true;
    digitalWrite(LED_PIN, HIGH); // Turn on LED
    Serial.printf("High temperature alert: %.1f°C >= %.1f°C\n", current_object_temp, high_temp_threshold);
  } else if (current_object_temp < high_temp_threshold - 2.0) { // Hysteresis
    high_alert_triggered = false;
    digitalWrite(LED_PIN, LOW);
  }
}

// Event handlers
void main_menu_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    int screen_int = (int)lv_event_get_user_data(e);
    ScreenState screen = (ScreenState)screen_int;
    switch_to_screen(screen);
  }
}

void temp_display_back_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    switch_to_screen(SCREEN_MAIN_MENU);
  }
}

void temp_gauge_back_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    switch_to_screen(SCREEN_MAIN_MENU);
  }
}

void settings_back_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    save_preferences();
    switch_to_screen(SCREEN_MAIN_MENU);
  }
}

void temp_unit_switch_event_cb(lv_event_t *e) {
  // Simple unit toggle (since we now use separate buttons)
  use_celsius = !use_celsius;
  save_preferences();
  update_temp_display_screen();
  update_temp_gauge_screen();
}

void brightness_slider_event_cb(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  brightness_level = lv_slider_get_value(slider);

  // Update brightness value label (global brightness_label is created in switch_to_settings_screen)
  if (brightness_label) {
    char brightness_str[10];
    snprintf(brightness_str, sizeof(brightness_str), "%d", brightness_level);
    lv_label_set_text(brightness_label, brightness_str);
  }

  // TODO: Apply brightness to display if supported
  save_preferences();
}

void sound_enable_switch_event_cb(lv_event_t *e) {
  // Check which button was pressed by the user data
  int enable_sound = (int)lv_event_get_user_data(e);
  sound_enabled = (enable_sound == 1);
  save_preferences();
}

void volume_slider_event_cb(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  sound_volume = lv_slider_get_value(slider);

  // Update volume value label
  lv_obj_t *volume_value_label = lv_obj_get_child(slider, 0); // Assuming it's the next sibling
  char volume_str[10];
  snprintf(volume_str, sizeof(volume_str), "%d", sound_volume);
  lv_label_set_text(volume_value_label, volume_str);

  save_preferences();
}

void alerts_enable_switch_event_cb(lv_event_t *e) {
  // Check which button was pressed by the user data
  int enable_alerts = (int)lv_event_get_user_data(e);
  alerts_enabled = (enable_alerts == 1);
  save_preferences();
}

void temp_alert_slider_event_cb(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);

  if (slider == low_temp_slider) {
    low_temp_threshold = (float)lv_slider_get_value(slider);
    char low_temp_str[10];
    snprintf(low_temp_str, sizeof(low_temp_str), "%.0f C", low_temp_threshold);
    lv_label_set_text(low_temp_label, low_temp_str);
  } else if (slider == high_temp_slider) {
    high_temp_threshold = (float)lv_slider_get_value(slider);
    char high_temp_str[10];
    snprintf(high_temp_str, sizeof(high_temp_str), "%.0f C", high_temp_threshold);
    lv_label_set_text(high_temp_label, high_temp_str);
  }

  save_preferences();
}
