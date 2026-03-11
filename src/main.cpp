#include <Arduino.h>
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedHUB.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <lvgl.h>
#include <m5_unit_joystick2.hpp>
#include <M5_UNIT_8SERVO.h>
#include "lv_conf.h"
#include "m5gfx_lvgl.hpp"
#include <Preferences.h>
#include <math.h>                     // for M_PI

#ifdef FIRMWARE_VERSION_REG
#undef FIRMWARE_VERSION_REG
#endif
#ifdef I2C_ADDRESS_REG
#undef I2C_ADDRESS_REG
#endif

/* ------------------- Debug logging system ------------------- */
#define DEBUG_LOG(fmt, ...) if (debug_logging) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

/* ------------------- Hardware pins ------------------- */
#define LED_PIN      9
#define BUTTON1_PIN 17
#define BUTTON2_PIN 18
#define KEY_PIN      8

/* ------------------- Audio config ------------------- */
#define VOLUME_MIN  64
#define VOLUME_MAX  255

/* ------------------- Screen config ------------------- */
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
static const uint32_t screenTickPeriod = 30;   // ms
static uint32_t lastLvglTick = 0;

/* ------------------- State enums ------------------- */
enum ScreenState {
    SCREEN_MAIN_MENU,
    SCREEN_TEMP_DISPLAY,
    SCREEN_TEMP_GAUGE,
    SCREEN_SERVO,
    SCREEN_SETTINGS
};

enum SettingsScreen {
    SETTINGS_MENU,
    SETTINGS_UNITS,
    SETTINGS_AUDIO,
    SETTINGS_DISPLAY,
    SETTINGS_ALERTS,
    SETTINGS_EMISSIVITY,
    SETTINGS_DEBUG,
    SETTINGS_EXIT
};

/* ------------------- Global settings ------------------- */
int  brightness_level = 128;      // 0-255
bool sound_enabled    = true;
int  sound_volume     = 70;       // 0-100
float alert_temp_threshold = 600.0f;  // Default 600°F, adjustable 400-700°F
bool alerts_enabled   = true;
bool use_celsius      = true;
int  update_rate      = 150;      // ms (faster updates for responsive readings)
bool debug_logging    = false;    // Enable/disable debug logging
bool alerts_changed_by_user = false;  // Track if user manually changed alerts
int main_menu_selection = 0;           // 0 = Thermometer, 1 = Gauge, 2 = Servo, 3 = Settings
uint32_t last_main_menu_button_time[3] = {0, 0, 0};  // debounce
float sensor_emissivity = 1.0f;        // Current MLX90614 emissivity
float pending_emissivity = 1.0f;       // Editable emissivity value

/* ------------------- Pa.HUB / I2C routing ------------------- */
constexpr uint8_t PAHUB_I2C_ADDRESS      = 0x70;
constexpr uint8_t MLX90614_I2C_ADDRESS   = 0x5A;
constexpr uint8_t DEFAULT_NCIR_HUB_CHANNEL     = 5;
constexpr uint8_t DEFAULT_JOYSTICK_HUB_CHANNEL = 1;
constexpr uint8_t DEFAULT_SERVO_HUB_CHANNEL    = 2;
constexpr uint8_t PAHUB_MAX_CHANNELS           = 6;
constexpr uint8_t SERVO_OUTPUT_INDEX     = 0;
constexpr uint8_t SERVO_UNIT_I2C_ADDRESS = M5_UNIT_8SERVO_DEFAULT_ADDR;
constexpr uint8_t JOYSTICK_I2C_ADDRESS   = JOYSTICK2_ADDR;
constexpr uint32_t PORTA_I2C_FREQUENCY   = 400000U;
constexpr uint32_t SERVO_UPDATE_INTERVAL = 25;

m5::unit::UnitUnified Units;
m5::unit::UnitPaHub2 paHub{PAHUB_I2C_ADDRESS};
M5UnitJoystick2 joystick2;
M5_UNIT_8SERVO servoUnit;
int port_a_sda = -1;
int port_a_scl = -1;
bool hub_available = false;
uint8_t pahub_current_channel = 0xFF;
uint8_t ncir_hub_channel = DEFAULT_NCIR_HUB_CHANNEL;
uint8_t joystick_hub_channel = DEFAULT_JOYSTICK_HUB_CHANNEL;
uint8_t servo_hub_channel = DEFAULT_SERVO_HUB_CHANNEL;
bool joystick_available = false;
bool servo_unit_available = false;
int current_servo_command = 90;
int last_servo_displayed_command = -1000;
uint32_t last_servo_update = 0;

/* ------------------- Battery monitoring ------------------- */
int  battery_level     = 100;     // 0-100 percentage
float battery_voltage  = 4.2f;    // Battery voltage
bool battery_alerts_enabled = true;  // Enable battery low alerts
int  battery_low_threshold = 20;     // Low battery alert threshold (%)
int  battery_critical_threshold = 10; // Critical battery threshold (%)
uint32_t last_battery_update = 0;     // Last battery reading time
static const uint32_t battery_update_interval = 30000; // Update every 30 seconds


/* ------------------- Temperature vars ------------------- */
float current_object_temp = 0.0f;
float current_ambient_temp = 0.0f;

/* ------------------- Preferences ------------------- */
Preferences preferences;

/* ------------------- UI objects ------------------- */
lv_obj_t *main_menu_screen;
lv_obj_t *temp_display_screen;
lv_obj_t *temp_gauge_screen;
lv_obj_t *servo_screen;
lv_obj_t *settings_screen;

/* ---- Main menu ---- */
lv_obj_t *temp_display_btn;
lv_obj_t *temp_gauge_btn;
lv_obj_t *servo_menu_btn;
lv_obj_t *settings_menu_btn;

/* ---- Temp display ---- */
lv_obj_t *temp_circle;
lv_obj_t *object_temp_label;
lv_obj_t *ambient_temp_label;
lv_obj_t *temp_unit_label;
lv_obj_t *temp_status_label;
lv_obj_t *temp_display_back_btn;

/* ---- Temp gauge ---- */
lv_obj_t *temp_scale;
lv_obj_t *temp_gauge_needle;
lv_obj_t *temp_gauge_value_label;
lv_obj_t *temp_gauge_back_btn;

/* ---- Servo ---- */
lv_obj_t *servo_angle_label;
lv_obj_t *servo_bar;
lv_obj_t *servo_status_label;
lv_obj_t *servo_back_btn;

/* ------------------- Runtime state ------------------- */
ScreenState current_screen = SCREEN_MAIN_MENU;
SettingsScreen current_settings_screen = SETTINGS_MENU;
int current_settings_selection = 0;
bool exit_selection_cancel = true;          // true = CANCEL, false = SAVE
uint32_t last_button_time[3] = {0,0,0};     // debounce for Btn1, Btn2, Key

/* ------------------- Function prototypes ------------------- */
void setup_hardware();
void load_preferences();
void save_preferences();
void switch_to_screen(ScreenState s);
void create_main_menu_ui();
void create_temp_display_ui();
void create_temp_gauge_ui();
void create_servo_ui();
void create_settings_ui();
void switch_to_settings_screen();
void update_scale_config();
void update_temperature_reading();
void update_temp_display_screen();
void update_temp_gauge_screen();
void update_servo_screen();
void update_battery_reading();
void check_battery_alerts();
void play_beep(int freq, int dur);
void check_temp_alerts();
lv_color_t get_temperature_color(float tempF);
lv_color_t get_battery_color(int level);
void update_main_menu_highlight();
void apply_emissivity_and_restart();
bool select_hub_channel(uint8_t channel, const char* context);
bool raw_select_hub_channel(uint8_t channel);
bool i2c_device_present(uint8_t address);
void log_i2c_scan_for_current_channel(const char* context);
bool find_device_on_hub(uint8_t address, uint8_t& found_channel, const char* label);
bool try_init_mlx_on_channel(uint8_t channel, bool log_scan);
float readJoystickX();
bool setServoSpeed(uint8_t channel, int value);


/* ------------------- Event callbacks ------------------- */
void main_menu_event_cb(lv_event_t *e);
void temp_display_back_event_cb(lv_event_t *e);
void temp_gauge_back_event_cb(lv_event_t *e);
void servo_back_event_cb(lv_event_t *e);
void brightness_slider_event_cb(lv_event_t *e);
void volume_slider_event_cb(lv_event_t *e);
void temp_alert_slider_event_cb(lv_event_t *e);
void temp_alert_up_btn_event_cb(lv_event_t *e);
void temp_alert_down_btn_event_cb(lv_event_t *e);

/* ------------------- LVGL tick task ------------------- */
static void lvgl_tick_task(void *) {
    lv_timer_handler();               // LVGL 9.x
}

/* ============================================================= */
/* =========================== SETUP =========================== */
/* ============================================================= */
void setup() {
    Serial.begin(115200);
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.println("M5Stack CoreS3 started");

    port_a_sda = M5.getPin(m5::pin_name_t::port_a_sda);
    port_a_scl = M5.getPin(m5::pin_name_t::port_a_scl);
    Serial.printf("PortA I2C pins SDA=%d SCL=%d\n", port_a_sda, port_a_scl);
    Wire.end();
    Wire.begin(port_a_sda, port_a_scl, PORTA_I2C_FREQUENCY);

    hub_available = i2c_device_present(PAHUB_I2C_ADDRESS);
    if (hub_available) {
        Serial.println("Pa.HUB detected on PortA");
    } else {
        Serial.println("Pa.HUB not detected, using direct PortA I2C");
    }

    /* ---- Sensor ---- */
    bool mlx_ready = false;
    if (hub_available) {
        Serial.printf("Trying legacy NCIR startup on Pa.HUB channel %u\n", ncir_hub_channel);
        mlx_ready = try_init_mlx_on_channel(ncir_hub_channel, false);

        if (!mlx_ready) {
            Serial.println("Legacy NCIR startup failed, scanning channels for fallback...");
            for (uint8_t channel = 0; channel < PAHUB_MAX_CHANNELS && !mlx_ready; ++channel) {
                if (channel == ncir_hub_channel) {
                    continue;
                }
                if (try_init_mlx_on_channel(channel, true)) {
                    ncir_hub_channel = channel;
                    mlx_ready = true;
                }
            }
        }
    } else {
        Serial.println("Trying direct PortA NCIR startup");
        mlx_ready = try_init_mlx_on_channel(0, true);
    }

    if (!mlx_ready) {
        Serial.println("MLX90614 init failed!");
        while (1) {
            delay(100);
        }
    }
    Serial.println("MLX90614 OK");
    sensor_emissivity = (float)mlx.readEmissivity();
    if (isnan(sensor_emissivity) || sensor_emissivity < 0.1f || sensor_emissivity > 1.0f) {
        sensor_emissivity = 1.0f;
    }
    pending_emissivity = sensor_emissivity;
    Serial.printf("MLX90614 emissivity: %.2f\n", sensor_emissivity);

    joystick_available = find_device_on_hub(JOYSTICK_I2C_ADDRESS, joystick_hub_channel, "Joystick2");
    if (joystick_available) {
        if (!select_hub_channel(joystick_hub_channel, "joystick init") ||
            !joystick2.begin(&Wire, JOYSTICK_I2C_ADDRESS, port_a_sda, port_a_scl, PORTA_I2C_FREQUENCY)) {
            joystick_available = false;
            Serial.println("Joystick2 init failed, disabling servo input");
        } else {
            joystick2.set_rgb_color(0x00AAFF);
            Serial.printf("Joystick2 OK on channel %u\n", joystick_hub_channel);
        }
    } else {
        Serial.println("Joystick2 not found, servo input disabled");
    }

    servo_unit_available = find_device_on_hub(SERVO_UNIT_I2C_ADDRESS, servo_hub_channel, "8Servo Unit");
    if (servo_unit_available) {
        if (!select_hub_channel(servo_hub_channel, "8Servo init") ||
            !servoUnit.begin(&Wire, port_a_sda, port_a_scl, SERVO_UNIT_I2C_ADDRESS)) {
            servo_unit_available = false;
            Serial.println("8Servo Unit init failed, disabling servo output");
        } else {
            servoUnit.setOnePinMode(SERVO_OUTPUT_INDEX, SERVO_CTL_MODE);
            Serial.printf("8Servo Unit OK on channel %u\n", servo_hub_channel);
            setServoSpeed(servo_hub_channel, current_servo_command);
        }
    } else {
        Serial.println("8Servo Unit not found, servo output disabled");
    }
    select_hub_channel(ncir_hub_channel, "restore NCIR channel");

    /* ---- LVGL ---- */
    lv_init();
    m5gfx_lvgl_init();                 // creates the default display
    Serial.println("LVGL ready");

    /* ---- Hardware ---- */
    setup_hardware();
    load_preferences();
    M5.Display.setBrightness(brightness_level);
    M5.Speaker.setVolume(static_cast<uint8_t>(sound_volume * 2.55));

    /* ---- UI ---- */
    create_main_menu_ui();
    create_temp_display_ui();
    create_temp_gauge_ui();
    create_servo_ui();
    create_settings_ui();

    /* ---- Start on main menu ---- */
    lv_screen_load(main_menu_screen);
    lv_refr_now(nullptr);

    Serial.println("UI ready");
}

/* ============================================================= */
/* =========================== LOOP ============================ */
/* ============================================================= */
void loop() {
  M5.update();
  if (hub_available) {
      Units.update();
  }

  /* ---- LVGL refresh ---- */
  uint32_t now = millis();
  if (now - lastLvglTick >= screenTickPeriod) {
      lvgl_tick_task(nullptr);
      lastLvglTick = now;
  }

  /* ---- Temperature update ---- */
  static uint32_t last_update = 0;
  if (millis() - last_update >= update_rate) {
      update_temperature_reading();
      if (current_screen == SCREEN_TEMP_DISPLAY) update_temp_display_screen();
      if (current_screen == SCREEN_TEMP_GAUGE)   update_temp_gauge_screen();
      // Only check alerts if debug is OFF (to avoid spam) OR if user changed alerts
      if (!debug_logging || alerts_changed_by_user) {
          check_temp_alerts();
          // Reset flag after use
          if (alerts_changed_by_user) {
              alerts_changed_by_user = false;
          }
      }
      last_update = millis();
  }

  if (current_screen == SCREEN_SERVO && joystick_available && servo_unit_available &&
      millis() - last_servo_update >= SERVO_UPDATE_INTERVAL) {
      const float joystick_x = readJoystickX();
      const int command = constrain(static_cast<int>(lroundf(90.0f + joystick_x * 90.0f)), 0, 180);
      if (abs(command - current_servo_command) >= 1) {
          setServoSpeed(servo_hub_channel, command);
      }
      update_servo_screen();
      last_servo_update = millis();
  }

  /* ---- Battery update ---- */
  update_battery_reading();

  /* ---- Main Menu & Settings navigation (shared buttons) ---- */
  if (current_screen == SCREEN_MAIN_MENU || current_screen == SCREEN_SETTINGS) {
      static bool b1_last = HIGH, b2_last = HIGH, key_last = HIGH;
      bool b1 = digitalRead(BUTTON1_PIN);
      bool b2 = digitalRead(BUTTON2_PIN);
      bool key = digitalRead(KEY_PIN);
      uint32_t now = millis();

      /* Select correct debounce array */
      uint32_t *debounce = (current_screen == SCREEN_MAIN_MENU) ? last_main_menu_button_time : last_button_time;

      /* ---- Button 1 (Up / Left) ---- */
      if (b1 == LOW && b1_last == HIGH && now - debounce[0] >= 150) {
          DEBUG_LOG("Button 1 pressed (screen %d)", current_screen);
          play_beep(1000, 50);  // Short beep for button feedback
          if (current_screen == SCREEN_MAIN_MENU) {
              main_menu_selection = (main_menu_selection - 1 + 4) % 4;
              update_main_menu_highlight();
              DEBUG_LOG("Main menu selection: %d", main_menu_selection);
          } else if (current_screen == SCREEN_SETTINGS) {
              DEBUG_LOG("Settings screen: %d", current_settings_screen);
              if (current_settings_screen == SETTINGS_MENU) {
                  current_settings_selection = (current_settings_selection + 1) % 7;
                  switch_to_settings_screen();
                  DEBUG_LOG("Settings nav: selection %d", current_settings_selection);
              } else if (current_settings_screen == SETTINGS_UNITS) {
                  use_celsius = true;
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Units: Set to Celsius");
              } else if (current_settings_screen == SETTINGS_AUDIO) {
                  sound_enabled = true;
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Audio: Set to ON");
              } else if (current_settings_screen == SETTINGS_ALERTS) {
                  alerts_enabled = true;
                  alerts_changed_by_user = true;  // User changed alerts
                  check_temp_alerts();  // Check immediately when user changes alerts
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Alerts: Set to ON");
              } else if (current_settings_screen == SETTINGS_EMISSIVITY) {
                  pending_emissivity = min(pending_emissivity + 0.01f, 1.00f);
                  switch_to_settings_screen();
                  DEBUG_LOG("Emissivity increased to %.2f", pending_emissivity);
              } else if (current_settings_screen == SETTINGS_DEBUG) {
                  debug_logging = true;
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Debug: Set to ON");
              } else if (current_settings_screen == SETTINGS_EXIT) {
                  exit_selection_cancel = true;
                  switch_to_settings_screen();
                  DEBUG_LOG("Exit: Selected CANCEL");
              }
          }
          debounce[0] = now;
      }

      /* ---- Button 2 (Down / Right) ---- */
      if (b2 == LOW && b2_last == HIGH && now - debounce[1] >= 150) {
          DEBUG_LOG("Button 2 pressed (screen %d)", current_screen);
          play_beep(1200, 50);  // Short beep for button feedback
          if (current_screen == SCREEN_MAIN_MENU) {
              main_menu_selection = (main_menu_selection + 1) % 4;
              update_main_menu_highlight();
              DEBUG_LOG("Main menu selection: %d", main_menu_selection);
          } else if (current_screen == SCREEN_SETTINGS) {
              DEBUG_LOG("Settings screen: %d", current_settings_screen);
              if (current_settings_screen == SETTINGS_MENU) {
                  current_settings_selection = (current_settings_selection - 1 + 7) % 7;
                  switch_to_settings_screen();
                  DEBUG_LOG("Settings nav: selection %d", current_settings_selection);
              } else if (current_settings_screen == SETTINGS_UNITS) {
                  use_celsius = false;
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Units: Set to Fahrenheit");
              } else if (current_settings_screen == SETTINGS_AUDIO) {
                  sound_enabled = false;
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Audio: Set to OFF");
              } else if (current_settings_screen == SETTINGS_ALERTS) {
                  alerts_enabled = false;
                  alerts_changed_by_user = true;  // User changed alerts
                  check_temp_alerts();  // Check immediately when user changes alerts
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Alerts: Set to OFF");
              } else if (current_settings_screen == SETTINGS_EMISSIVITY) {
                  pending_emissivity = max(pending_emissivity - 0.01f, 0.10f);
                  switch_to_settings_screen();
                  DEBUG_LOG("Emissivity decreased to %.2f", pending_emissivity);
              } else if (current_settings_screen == SETTINGS_DEBUG) {
                  debug_logging = false;
                  save_preferences();
                  switch_to_settings_screen();
                  DEBUG_LOG("Debug: Set to OFF");
              } else if (current_settings_screen == SETTINGS_EXIT) {
                  exit_selection_cancel = false;
                  switch_to_settings_screen();
                  DEBUG_LOG("Exit: Selected SAVE");
              }
          }
          debounce[1] = now;
      }

      /* ---- Key (Select / Confirm) ---- */
      if (key == LOW && key_last == HIGH && now - debounce[2] >= 150) {
          DEBUG_LOG("Key pressed (screen %d)", current_screen);
          play_beep(800, 50);  // Short beep for button feedback
          if (current_screen == SCREEN_MAIN_MENU) {
              DEBUG_LOG("Main menu select: %d", main_menu_selection);
              switch (main_menu_selection) {
                  case 0: switch_to_screen(SCREEN_TEMP_DISPLAY); break;
                  case 1: switch_to_screen(SCREEN_TEMP_GAUGE);   break;
                  case 2: switch_to_screen(SCREEN_SERVO);        break;
                  case 3: switch_to_screen(SCREEN_SETTINGS);     break;
              }
          } else if (current_screen == SCREEN_SETTINGS) {
              DEBUG_LOG("Settings screen: %d, selection: %d", current_settings_screen, current_settings_selection);
              if (current_settings_screen == SETTINGS_MENU) {
                  switch (current_settings_selection) {
                      case 0: current_settings_screen = SETTINGS_UNITS;   break;
                      case 1: current_settings_screen = SETTINGS_AUDIO;   break;
                      case 2: current_settings_screen = SETTINGS_DISPLAY; break;
                      case 3: current_settings_screen = SETTINGS_ALERTS;  break;
                      case 4: current_settings_screen = SETTINGS_EMISSIVITY; break;
                      case 5: current_settings_screen = SETTINGS_DEBUG;   break;
                      case 6: current_settings_screen = SETTINGS_EXIT;    break;
                  }
                  if (current_settings_screen == SETTINGS_EMISSIVITY) {
                      pending_emissivity = sensor_emissivity;
                  }
                  switch_to_settings_screen();
              } else if (current_settings_screen == SETTINGS_EMISSIVITY) {
                  apply_emissivity_and_restart();
              } else if (current_settings_screen == SETTINGS_EXIT) {
                  DEBUG_LOG("Exit: %s", exit_selection_cancel ? "CANCEL - back to menu" : "SAVE - back to menu");
                  if (exit_selection_cancel) {
                      switch_to_screen(SCREEN_MAIN_MENU);
                  } else {
                      save_preferences();
                      switch_to_screen(SCREEN_MAIN_MENU);
                  }
              } else {
                  /* Save and return to settings menu */
                  DEBUG_LOG("Settings saved, return to settings menu");
                  save_preferences();
                  current_settings_screen = SETTINGS_MENU;
                  switch_to_settings_screen();
              }
          }
          debounce[2] = now;
      }

      b1_last = b1; b2_last = b2; key_last = key;
  }

  delay(5);
}

/* ============================================================= */
/* ====================== HARDWARE SETUP ======================= */
/* ============================================================= */
void setup_hardware() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    pinMode(KEY_PIN, INPUT_PULLUP);
    M5.Speaker.begin();
}

/* ============================================================= */
/* ====================== PREFERENCES ========================= */
/* ============================================================= */
void load_preferences() {
    DEBUG_LOG("Loading preferences...");
    preferences.begin("ncir", false);
    use_celsius      = preferences.getBool("celsius", true);
    brightness_level = preferences.getInt ("bright", 128);
    sound_enabled    = preferences.getBool("sound",   true);
    sound_volume     = preferences.getInt ("volume",  70);
    alerts_enabled   = preferences.getBool("alerts",  true);
    alert_temp_threshold = preferences.getFloat("alert_temp", 600.0f);
    debug_logging    = preferences.getBool("debug",   false);
    battery_alerts_enabled = preferences.getBool("batt_alerts", true);
    battery_low_threshold = preferences.getInt("batt_low", 20);
    battery_critical_threshold = preferences.getInt("batt_crit", 10);
    preferences.end();
    DEBUG_LOG("Preferences loaded - Units:%s Bright:%d Sound:%s Vol:%d Alerts:%s AlertTemp:%.1f°F Debug:%s Batt:%s",
              use_celsius ? "C" : "F", brightness_level, sound_enabled ? "ON" : "OFF",
              sound_volume, alerts_enabled ? "ON" : "OFF", alert_temp_threshold,
              debug_logging ? "ON" : "OFF", battery_alerts_enabled ? "ON" : "OFF");
}
void save_preferences() {
    DEBUG_LOG("Saving preferences...");
    preferences.begin("ncir", false);
    preferences.putBool ("celsius", use_celsius);
    preferences.putInt  ("bright",  brightness_level);
    preferences.putBool ("sound",   sound_enabled);
    preferences.putInt  ("volume",  sound_volume);
    preferences.putBool ("alerts",  alerts_enabled);
    preferences.putFloat("alert_temp", alert_temp_threshold);
    preferences.putBool ("debug",   debug_logging);
    preferences.putBool ("batt_alerts", battery_alerts_enabled);
    preferences.putInt  ("batt_low", battery_low_threshold);
    preferences.putInt  ("batt_crit", battery_critical_threshold);
    preferences.putFloat("emissivity", sensor_emissivity);
    preferences.end();
    DEBUG_LOG("Preferences saved - Units:%s Bright:%d Sound:%s Vol:%d Alerts:%s AlertTemp:%.1f°F Emiss:%.2f Debug:%s Batt:%s",
              use_celsius ? "C" : "F", brightness_level, sound_enabled ? "ON" : "OFF",
              sound_volume, alerts_enabled ? "ON" : "OFF", alert_temp_threshold, sensor_emissivity,
              debug_logging ? "ON" : "OFF", battery_alerts_enabled ? "ON" : "OFF");
}

/* ============================================================= */
/* ====================== SCREEN SWITCH ======================= */
/* ============================================================= */
void switch_to_screen(ScreenState s) {
    if (s == current_screen) return;

    const char *screen_names[] = {"Main Menu", "Temp Display", "Temp Gauge", "Servo", "Settings"};
    DEBUG_LOG("Screen transition: %s -> %s", screen_names[current_screen], screen_names[s]);

    current_screen = s;

    switch (s) {
        case SCREEN_MAIN_MENU:     lv_screen_load(main_menu_screen);      break;
        case SCREEN_TEMP_DISPLAY:  lv_screen_load(temp_display_screen);
                                   update_temp_display_screen();      break;
        case SCREEN_TEMP_GAUGE:    lv_screen_load(temp_gauge_screen);
                                   update_scale_config();
                                   update_temp_gauge_screen();        break;
        case SCREEN_SERVO:         lv_screen_load(servo_screen);
                                   update_servo_screen();             break;
        case SCREEN_SETTINGS:      lv_screen_load(settings_screen);
                                   current_settings_screen = SETTINGS_MENU;  // Reset to main settings menu
                                   current_settings_selection = 0;           // Reset selection to first item
                                   switch_to_settings_screen();       break;
    }
}

/* ============================================================= */
/* ====================== MAIN MENU UI ======================== */
/* ============================================================= */
void create_main_menu_ui() {
  main_menu_screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(main_menu_screen, lv_color_hex(0x0f1419), 0);
  lv_obj_remove_flag(main_menu_screen, LV_OBJ_FLAG_SCROLLABLE);

  /* header */
  lv_obj_t *hdr = lv_obj_create(main_menu_screen);
  lv_obj_set_size(hdr, 320, 85);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(hdr, lv_color_hex(0x1a1a2e), 0);
  lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *accent = lv_obj_create(main_menu_screen);
  lv_obj_set_size(accent, 320, 2);
  lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 81);
  lv_obj_set_style_bg_color(accent, lv_color_hex(0xFF6B35), 0);

  lv_obj_t *title = lv_label_create(hdr);
  lv_label_set_text(title, "NCIR Monitor");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *sub = lv_label_create(hdr);
  lv_label_set_text(sub, "Non-Contact Temperature");
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sub, lv_color_hex(0xE0E0E0), 0);
  lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 45);

  /* Battery indicator */
  lv_obj_t *batt_cont = lv_obj_create(main_menu_screen);
  lv_obj_set_size(batt_cont, 80, 25);
  lv_obj_align(batt_cont, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
  lv_obj_set_style_bg_color(batt_cont, lv_color_hex(0x2c3e50), 0);
  lv_obj_set_style_border_width(batt_cont, 1, 0);
  lv_obj_set_style_border_color(batt_cont, lv_color_hex(0xFF6B35), 0);
  lv_obj_set_style_radius(batt_cont, 3, 0);

  /* Battery icon (simple rectangle with fill) */
  lv_obj_t *batt_icon = lv_obj_create(batt_cont);
  lv_obj_set_size(batt_icon, 12, 6);
  lv_obj_align(batt_icon, LV_ALIGN_LEFT_MID, 3, 0);
  lv_obj_set_style_bg_color(batt_icon, lv_color_hex(0x666666), 0);
  lv_obj_set_style_border_width(batt_icon, 1, 0);
  lv_obj_set_style_border_color(batt_icon, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_radius(batt_icon, 1, 0);

  /* Battery fill level */
  lv_obj_t *batt_fill = lv_obj_create(batt_icon);
  lv_obj_set_size(batt_fill, 8, 4);
  lv_obj_align(batt_fill, LV_ALIGN_LEFT_MID, 1, 0);
  lv_obj_set_style_bg_color(batt_fill, get_battery_color(battery_level), 0);
  lv_obj_set_style_radius(batt_fill, 1, 0);

  /* Battery percentage text */
  lv_obj_t *batt_text = lv_label_create(batt_cont);
  char batt_buf[8];
  snprintf(batt_buf, sizeof(batt_buf), "%d%%", battery_level);
  lv_label_set_text(batt_text, batt_buf);
  lv_obj_set_style_text_font(batt_text, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(batt_text, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(batt_text, LV_ALIGN_LEFT_MID, 20, 0);

  /* buttons */
  int bw = 130, bh = 58, xoff = 18, top_y = -5, bottom_y = 70;

  temp_display_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(temp_display_btn, bw, bh);
  lv_obj_align(temp_display_btn, LV_ALIGN_CENTER, -xoff, top_y);
  lv_obj_set_style_bg_color(temp_display_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(temp_display_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(temp_display_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_add_event_cb(temp_display_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_TEMP_DISPLAY);
  { lv_obj_t *l = lv_label_create(temp_display_btn); lv_label_set_text(l, "Thermometer"); lv_obj_center(l); }

  temp_gauge_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(temp_gauge_btn, bw, bh);
  lv_obj_align(temp_gauge_btn, LV_ALIGN_CENTER, xoff, top_y);
  lv_obj_set_style_bg_color(temp_gauge_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(temp_gauge_btn, lv_color_hex(0x4285F4), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(temp_gauge_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_add_event_cb(temp_gauge_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_TEMP_GAUGE);
  { lv_obj_t *l = lv_label_create(temp_gauge_btn); lv_label_set_text(l, "Gauge"); lv_obj_center(l); }

  servo_menu_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(servo_menu_btn, bw, bh);
  lv_obj_align(servo_menu_btn, LV_ALIGN_CENTER, -xoff, bottom_y);
  lv_obj_set_style_bg_color(servo_menu_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(servo_menu_btn, lv_color_hex(0x2ecc71), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(servo_menu_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_add_event_cb(servo_menu_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_SERVO);
  { lv_obj_t *l = lv_label_create(servo_menu_btn); lv_label_set_text(l, "Servo"); lv_obj_center(l); }

  settings_menu_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(settings_menu_btn, bw, bh);
  lv_obj_align(settings_menu_btn, LV_ALIGN_CENTER, xoff, bottom_y);
  lv_obj_set_style_bg_color(settings_menu_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(settings_menu_btn, lv_color_hex(0x9b59b6), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(settings_menu_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_add_event_cb(settings_menu_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_SETTINGS);
  { lv_obj_t *l = lv_label_create(settings_menu_btn); lv_label_set_text(l, "Settings"); lv_obj_center(l); }

  /* Initial highlight */
  update_main_menu_highlight();
}

/* ============================================================= */
/* =================== TEMP DISPLAY UI ======================= */
/* ============================================================= */
void create_temp_display_ui() {
    temp_display_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(temp_display_screen, lv_color_hex(0xf5f5f5), 0);
    lv_obj_remove_flag(temp_display_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* header */
    lv_obj_t *hdr = lv_obj_create(temp_display_screen);
    lv_obj_set_size(hdr, 320, 80);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x1a1a2e), 0);

    lv_obj_t *accent = lv_obj_create(temp_display_screen);
    lv_obj_set_size(accent, 320, 2);
    lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0xFF6B35), 0);

    lv_obj_t *t = lv_label_create(hdr);
    lv_label_set_text(t, "Temperature");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 15);

    /* big circle */
    temp_circle = lv_obj_create(temp_display_screen);
    lv_obj_set_size(temp_circle, 180, 180);
    lv_obj_align(temp_circle, LV_ALIGN_CENTER, 0, -5);
    lv_obj_set_style_bg_color(temp_circle, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(temp_circle, 6, 0);
    lv_obj_set_style_border_color(temp_circle, lv_color_hex(0xFF6B35), 0);
    lv_obj_set_style_radius(temp_circle, 90, 0);

    object_temp_label = lv_label_create(temp_circle);
    lv_label_set_text(object_temp_label, "--°");
    lv_obj_set_style_text_font(object_temp_label, &lv_font_montserrat_24, 0);
    lv_obj_align(object_temp_label, LV_ALIGN_CENTER, 0, -15);

    temp_unit_label = lv_label_create(temp_circle);
    lv_label_set_text(temp_unit_label, use_celsius ? "Celsius" : "Fahrenheit");
    lv_obj_set_style_text_font(temp_unit_label, &lv_font_montserrat_14, 0);
    lv_obj_align(temp_unit_label, LV_ALIGN_CENTER, 0, 20);

    /* ambient */
    lv_obj_t *amb = lv_obj_create(temp_display_screen);
    lv_obj_set_size(amb, 280, 70);
    lv_obj_align(amb, LV_ALIGN_CENTER, 0, 95);
    lv_obj_set_style_bg_color(amb, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(amb, 16, 0);

    ambient_temp_label = lv_label_create(amb);
    lv_label_set_text(ambient_temp_label, "Ambient: --°C");
    lv_obj_set_style_text_font(ambient_temp_label, &lv_font_montserrat_18, 0);
    lv_obj_align(ambient_temp_label, LV_ALIGN_LEFT_MID, 55, 0);

    /* status */
    lv_obj_t *st = lv_obj_create(temp_display_screen);
    lv_obj_set_size(st, 120, 30);
    lv_obj_align(st, LV_ALIGN_CENTER, 0, 180);
    lv_obj_set_style_bg_color(st, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(st, 15, 0);

    temp_status_label = lv_label_create(st);
    lv_label_set_text(temp_status_label, "Ready");
    lv_obj_center(temp_status_label);

    /* back button */
    temp_display_back_btn = lv_btn_create(temp_display_screen);
    lv_obj_set_size(temp_display_back_btn, 60, 60);
    lv_obj_align(temp_display_back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(temp_display_back_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
    lv_obj_set_style_radius(temp_display_back_btn, 30, 0);
    lv_obj_add_event_cb(temp_display_back_btn, temp_display_back_event_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(temp_display_back_btn); lv_label_set_text(l, "Back"); lv_obj_center(l); }
}

/* ============================================================= */
/* ====================== GAUGE UI =========================== */
/* ============================================================= */
void create_temp_gauge_ui() {
    temp_gauge_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(temp_gauge_screen, lv_color_hex(0x0d1117), 0);

    /* header */
    lv_obj_t *hdr = lv_obj_create(temp_gauge_screen);
    lv_obj_set_size(hdr, 320, 50);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x161b22), 0);

    lv_obj_t *accent = lv_obj_create(temp_gauge_screen);
    lv_obj_set_size(accent, 320, 2);
    lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x4285F4), 0);

    lv_obj_t *t = lv_label_create(hdr);
    lv_label_set_text(t, "Temperature Gauge");
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 10, 0);

    /* gauge container */
    lv_obj_t *cont = lv_obj_create(temp_gauge_screen);
    lv_obj_set_size(cont, 220, 140);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1e2936), 0);
    lv_obj_set_style_border_width(cont, 3, 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(0xFF6B35), 0);
    lv_obj_set_style_radius(cont, 20, 0);

    temp_scale = lv_scale_create(cont);
    lv_obj_set_size(temp_scale, 180, 180);
    lv_obj_align(temp_scale, LV_ALIGN_CENTER, 0, -10);
    lv_scale_set_mode(temp_scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_angle_range(temp_scale, 270);
    lv_scale_set_rotation(temp_scale, 135);
    lv_scale_set_total_tick_count(temp_scale, 41);
    lv_scale_set_major_tick_every(temp_scale, 5);
    lv_obj_set_style_bg_color(temp_scale, lv_color_hex(0x2c3e50), LV_PART_MAIN);
    lv_obj_set_style_bg_color(temp_scale, lv_color_hex(0x4285F4), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(temp_scale, lv_color_hex(0x607D8B), LV_PART_INDICATOR);

    /* needle */
    temp_gauge_needle = lv_line_create(temp_gauge_screen);
    lv_obj_set_style_line_width(temp_gauge_needle, 5, 0);
    lv_obj_set_style_line_color(temp_gauge_needle, lv_color_hex(0xFF6B35), 0);
    lv_obj_set_style_line_rounded(temp_gauge_needle, true, 0);

    /* center dot */
    lv_obj_t *dot = lv_obj_create(temp_gauge_screen);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xFF6B35), 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);

    /* value label */
    lv_obj_t *val = lv_obj_create(temp_gauge_screen);
    lv_obj_set_size(val, 120, 40);
    lv_obj_align(val, LV_ALIGN_BOTTOM_MID, 0, -65);
    lv_obj_set_style_bg_color(val, lv_color_hex(0x2c3e50), 0);
    lv_obj_set_style_border_width(val, 2, 0);
    lv_obj_set_style_border_color(val, lv_color_hex(0x9b59b6), 0);
    lv_obj_set_style_radius(val, 8, 0);

    temp_gauge_value_label = lv_label_create(val);
    lv_label_set_text(temp_gauge_value_label, "0°C");
    lv_obj_set_style_text_font(temp_gauge_value_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(temp_gauge_value_label, lv_color_hex(0xFF6B35), 0);
    lv_obj_center(temp_gauge_value_label);

    /* back button */
    temp_gauge_back_btn = lv_btn_create(temp_gauge_screen);
    lv_obj_set_size(temp_gauge_back_btn, 90, 45);
    lv_obj_align(temp_gauge_back_btn, LV_ALIGN_BOTTOM_LEFT, 15, -15);
    lv_obj_set_style_bg_color(temp_gauge_back_btn, lv_color_hex(0x34495e), LV_PART_MAIN);
    lv_obj_set_style_border_width(temp_gauge_back_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(temp_gauge_back_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
    lv_obj_add_event_cb(temp_gauge_back_btn, temp_gauge_back_event_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(temp_gauge_back_btn); lv_label_set_text(l, "Back"); lv_obj_center(l); }
}

/* ============================================================= */
/* ====================== SERVO UI ============================ */
/* ============================================================= */
void create_servo_ui() {
    servo_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(servo_screen, lv_color_hex(0x101820), 0);
    lv_obj_remove_flag(servo_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *hdr = lv_obj_create(servo_screen);
    lv_obj_set_size(hdr, 320, 60);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x16324f), 0);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "Servo Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(title);

    servo_angle_label = lv_label_create(servo_screen);
    lv_label_set_text(servo_angle_label, "STOPPED");
    lv_obj_set_style_text_font(servo_angle_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(servo_angle_label, lv_color_hex(0x7DFFB3), 0);
    lv_obj_align(servo_angle_label, LV_ALIGN_TOP_MID, 0, 78);

    servo_bar = lv_bar_create(servo_screen);
    lv_obj_set_size(servo_bar, 250, 22);
    lv_obj_align(servo_bar, LV_ALIGN_CENTER, 0, 8);
    lv_bar_set_range(servo_bar, 0, 180);
    lv_bar_set_value(servo_bar, current_servo_command, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(servo_bar, lv_color_hex(0x273746), LV_PART_MAIN);
    lv_obj_set_style_bg_color(servo_bar, lv_color_hex(0x2ecc71), LV_PART_INDICATOR);

    servo_status_label = lv_label_create(servo_screen);
    lv_label_set_text(servo_status_label, "90 stop, <90 CCW, >90 CW");
    lv_obj_set_style_text_color(servo_status_label, lv_color_hex(0xD0D8E0), 0);
    lv_obj_align(servo_status_label, LV_ALIGN_CENTER, 0, 45);

    servo_back_btn = lv_btn_create(servo_screen);
    lv_obj_set_size(servo_back_btn, 90, 45);
    lv_obj_align(servo_back_btn, LV_ALIGN_BOTTOM_LEFT, 15, -15);
    lv_obj_set_style_bg_color(servo_back_btn, lv_color_hex(0x34495e), LV_PART_MAIN);
    lv_obj_set_style_border_width(servo_back_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(servo_back_btn, lv_color_hex(0x2ecc71), LV_PART_MAIN);
    lv_obj_add_event_cb(servo_back_btn, servo_back_event_cb, LV_EVENT_CLICKED, nullptr);
    { lv_obj_t *l = lv_label_create(servo_back_btn); lv_label_set_text(l, "Back"); lv_obj_center(l); }
}

/* ============================================================= */
/* ===================== SETTINGS UI ========================= */
/* ============================================================= */
void create_settings_ui() {
    settings_screen = lv_obj_create(nullptr);
}

/* ------------------------------------------------------------------ */
/*  Dynamically rebuild the settings page (called every navigation)   */
/* ------------------------------------------------------------------ */
void switch_to_settings_screen() {
    lv_obj_clean(settings_screen);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x1a1a40), 0);

    /* title bar */
    lv_obj_t *title_bg = lv_obj_create(settings_screen);
    lv_obj_set_size(title_bg, 320, 50);
    lv_obj_align(title_bg, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bg, lv_color_hex(0x2c3e50), 0);

    lv_obj_t *border = lv_obj_create(settings_screen);
    lv_obj_set_size(border, 320, 2);
    lv_obj_align(border, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(border, lv_color_hex(0x9b59b6), 0);

    lv_obj_t *title = lv_label_create(title_bg);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 10, 0);

    /* ---------- MENU ---------- */
    if (current_settings_screen == SETTINGS_MENU) {
        lv_label_set_text(title, "Configuration");
        const char *items[] = {"Units","Audio","Display","Alerts","Emiss","Debug","Exit"};
        for (int i = 0; i < 7; ++i) {
            lv_obj_t *b = lv_btn_create(settings_screen);
            lv_obj_set_size(b, 90, 50);
            int x = 0;
            int y = 0;
            if (i < 3) {
                int col = i;
                x = (col == 0 ? -100 : (col == 1 ? 0 : 100));
                y = -60;
            } else if (i < 6) {
                int col = i - 3;
                x = (col == 0 ? -100 : (col == 1 ? 0 : 100));
                y = 5;
            } else {
                x = 0;
                y = 70;
            }
            lv_obj_align(b, LV_ALIGN_CENTER, x, y);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x34495e), LV_PART_MAIN);
            lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(0xFF6B35), LV_PART_MAIN);
            if (i == current_settings_selection)
                lv_obj_set_style_border_color(b, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_obj_t *l = lv_label_create(b);
            lv_label_set_text(l, items[i]);
            lv_obj_center(l);
        }
        return;
    }

    /* ---------- UNITS ---------- */
    if (current_settings_screen == SETTINGS_UNITS) {
        lv_label_set_text(title, "Temperature Units");
        /* Celsius button */
        lv_obj_t *c = lv_btn_create(settings_screen);
        lv_obj_set_size(c, 120, 80);
        lv_obj_align(c, LV_ALIGN_CENTER, -80, 0);
        lv_obj_set_style_bg_color(c, lv_color_hex(0x2c3e50), LV_PART_MAIN);
        lv_obj_set_style_border_width(c, 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(c, use_celsius ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF6B35), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(c); lv_label_set_text(l, "C\nCelsius"); lv_obj_center(l); }

        /* Fahrenheit button */
        lv_obj_t *f = lv_btn_create(settings_screen);
        lv_obj_set_size(f, 120, 80);
        lv_obj_align(f, LV_ALIGN_CENTER, 80, 0);
        lv_obj_set_style_bg_color(f, lv_color_hex(0x2c3e50), LV_PART_MAIN);
        lv_obj_set_style_border_width(f, 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(f, !use_celsius ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF6B35), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(f); lv_label_set_text(l, "F\nFahrenheit"); lv_obj_center(l); }

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Btn1: C   Btn2: F   Key: OK");
        lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -20);
        return;
    }

    /* ---------- AUDIO ---------- */
    if (current_settings_screen == SETTINGS_AUDIO) {
        lv_label_set_text(title, "Audio Settings");

        /* ON / OFF */
        lv_obj_t *on = lv_btn_create(settings_screen);
        lv_obj_set_size(on, 100, 50);
        lv_obj_align(on, LV_ALIGN_CENTER, -60, -40);
        lv_obj_set_style_bg_color(on, sound_enabled ? lv_color_hex(0x00AA00) : lv_color_hex(0x666666), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(on); lv_label_set_text(l, "ON"); lv_obj_center(l); }

        lv_obj_t *off = lv_btn_create(settings_screen);
        lv_obj_set_size(off, 100, 50);
        lv_obj_align(off, LV_ALIGN_CENTER, 60, -40);
        lv_obj_set_style_bg_color(off, !sound_enabled ? lv_color_hex(0xAA0000) : lv_color_hex(0x666666), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(off); lv_label_set_text(l, "OFF"); lv_obj_center(l); }

        /* Volume slider */
        lv_obj_t *vc = lv_obj_create(settings_screen);
        lv_obj_set_size(vc, 280, 80);
        lv_obj_align(vc, LV_ALIGN_CENTER, 0, 50);
        lv_obj_set_style_bg_color(vc, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_border_width(vc, 2, 0);
        lv_obj_set_style_border_color(vc, lv_color_hex(0xFF6B35), 0);
        lv_obj_set_style_radius(vc, 10, 0);

        lv_obj_t *vt = lv_label_create(vc);
        lv_label_set_text(vt, "Volume");
        lv_obj_align(vt, LV_ALIGN_TOP_MID, 0, 8);

        lv_obj_t *vs = lv_slider_create(vc);
        lv_obj_set_size(vs, 200, 20);
        lv_obj_align(vs, LV_ALIGN_CENTER, 0, 10);
        lv_slider_set_range(vs, 0, 100);
        lv_slider_set_value(vs, sound_volume, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(vs, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_bg_color(vs, lv_color_hex(0xFF6B35), LV_PART_INDICATOR);
        lv_obj_add_event_cb(vs, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d", sound_volume);
        lv_obj_t *vl = lv_label_create(vc);
        lv_label_set_text(vl, buf);
        lv_obj_align(vl, LV_ALIGN_CENTER, 0, 45);

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Btn1: ON   Btn2: OFF   Key: OK");
        lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -20);
        return;
    }

    /* ---------- DISPLAY ---------- */
    if (current_settings_screen == SETTINGS_DISPLAY) {
        lv_label_set_text(title, "Display Settings");

        lv_obj_t *bc = lv_obj_create(settings_screen);
        lv_obj_set_size(bc, 280, 80);
        lv_obj_align(bc, LV_ALIGN_CENTER, 0, -20);
        lv_obj_set_style_bg_color(bc, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_border_width(bc, 2, 0);
        lv_obj_set_style_border_color(bc, lv_color_hex(0xFF6B35), 0);
        lv_obj_set_style_radius(bc, 10, 0);

        lv_obj_t *bt = lv_label_create(bc);
        lv_label_set_text(bt, "Screen Brightness");
        lv_obj_align(bt, LV_ALIGN_TOP_MID, 0, 8);

        lv_obj_t *bs = lv_slider_create(bc);
        lv_obj_set_size(bs, 200, 20);
        lv_obj_align(bs, LV_ALIGN_CENTER, 0, 10);
        lv_slider_set_range(bs, 0, 255);
        lv_slider_set_value(bs, brightness_level, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bs, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bs, lv_color_hex(0xFF6B35), LV_PART_INDICATOR);
        lv_obj_add_event_cb(bs, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

        char buf[8];
        snprintf(buf, sizeof(buf), "%d", brightness_level);
        lv_obj_t *bl = lv_label_create(bc);
        lv_label_set_text(bl, buf);
        lv_obj_align(bl, LV_ALIGN_CENTER, 0, 45);

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Key: OK");
        lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -20);
        return;
    }

    /* ---------- ALERTS ---------- */
    if (current_settings_screen == SETTINGS_ALERTS) {
        lv_label_set_text(title, "Temperature Alerts");

        /* ON / OFF */
        lv_obj_t *on = lv_btn_create(settings_screen);
        lv_obj_set_size(on, 100, 50);
        lv_obj_align(on, LV_ALIGN_CENTER, -60, -60);
        lv_obj_set_style_bg_color(on, alerts_enabled ? lv_color_hex(0x00AA00) : lv_color_hex(0x666666), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(on); lv_label_set_text(l, "ON"); lv_obj_center(l); }

        lv_obj_t *off = lv_btn_create(settings_screen);
        lv_obj_set_size(off, 100, 50);
        lv_obj_align(off, LV_ALIGN_CENTER, 60, -60);
        lv_obj_set_style_bg_color(off, !alerts_enabled ? lv_color_hex(0xAA0000) : lv_color_hex(0x666666), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(off); lv_label_set_text(l, "OFF"); lv_obj_center(l); }

        /* Alert Temperature slider */
        lv_obj_t *ac = lv_obj_create(settings_screen);
        lv_obj_set_size(ac, 280, 80);
        lv_obj_align(ac, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_color(ac, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_border_width(ac, 2, 0);
        lv_obj_set_style_border_color(ac, lv_color_hex(0xFF6B35), 0);
        lv_obj_set_style_radius(ac, 10, 0);

        lv_obj_t *at = lv_label_create(ac);
        lv_label_set_text(at, "Alert Temperature (°F)");
        lv_obj_align(at, LV_ALIGN_TOP_MID, 0, 8);

        lv_obj_t *as = lv_slider_create(ac);
        lv_obj_set_size(as, 200, 20);
        lv_obj_align(as, LV_ALIGN_CENTER, 0, 15);
        lv_slider_set_range(as, 400, 700);  // 400°F to 700°F range
        lv_slider_set_value(as, alert_temp_threshold, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(as, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_bg_color(as, lv_color_hex(0xFF6B35), LV_PART_INDICATOR);
        lv_obj_add_event_cb(as, temp_alert_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f°F", alert_temp_threshold);
        lv_obj_t *al = lv_label_create(ac);
        lv_label_set_text(al, buf);
        lv_obj_align(al, LV_ALIGN_BOTTOM_MID, 0, -5);

        /* Up/Down buttons for fine adjustment */
        lv_obj_t *up_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(up_btn, 80, 40);
        lv_obj_align(up_btn, LV_ALIGN_CENTER, -60, 100);
        lv_obj_set_style_bg_color(up_btn, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_border_width(up_btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(up_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
        lv_obj_add_event_cb(up_btn, temp_alert_up_btn_event_cb, LV_EVENT_CLICKED, nullptr);
        { lv_obj_t *l = lv_label_create(up_btn); lv_label_set_text(l, "+5°"); lv_obj_center(l); }

        lv_obj_t *down_btn = lv_btn_create(settings_screen);
        lv_obj_set_size(down_btn, 80, 40);
        lv_obj_align(down_btn, LV_ALIGN_CENTER, 60, 100);
        lv_obj_set_style_bg_color(down_btn, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_border_width(down_btn, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(down_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
        lv_obj_add_event_cb(down_btn, temp_alert_down_btn_event_cb, LV_EVENT_CLICKED, nullptr);
        { lv_obj_t *l = lv_label_create(down_btn); lv_label_set_text(l, "-5°"); lv_obj_center(l); }

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Btn1: ON   Btn2: OFF   Key: OK");
        lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -20);
        return;
    }

    /* ---------- EMISSIVITY ---------- */
    if (current_settings_screen == SETTINGS_EMISSIVITY) {
        lv_label_set_text(title, "Sensor Emissivity");

        lv_obj_t *card = lv_obj_create(settings_screen);
        lv_obj_set_size(card, 280, 125);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, -10);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xFF6B35), 0);
        lv_obj_set_style_radius(card, 10, 0);

        char buf[64];
        lv_obj_t *cur = lv_label_create(card);
        snprintf(buf, sizeof(buf), "Current: %.2f", sensor_emissivity);
        lv_label_set_text(cur, buf);
        lv_obj_align(cur, LV_ALIGN_TOP_MID, 0, 10);

        lv_obj_t *val = lv_label_create(card);
        snprintf(buf, sizeof(buf), "New: %.2f", pending_emissivity);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_24, 0);
        lv_label_set_text(val, buf);
        lv_obj_align(val, LV_ALIGN_CENTER, 0, -5);

        lv_obj_t *hint = lv_label_create(card);
        lv_label_set_text(hint, "Range 0.10 to 1.00");
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);

        lv_obj_t *warn = lv_label_create(settings_screen);
        lv_label_set_text(warn, "Press Key to write emissivity\nand restart device.");
        lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(warn, LV_ALIGN_CENTER, 0, 78);

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Btn1: +0.01   Btn2: -0.01   Key: Apply");
        lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -20);
        return;
    }

    /* ---------- DEBUG ---------- */
    if (current_settings_screen == SETTINGS_DEBUG) {
        lv_label_set_text(title, "Debug Logging");

        /* Current status display */
        lv_obj_t *status = lv_label_create(settings_screen);
        lv_label_set_text(status, debug_logging ? "Status: ENABLED" : "Status: DISABLED");
        lv_obj_set_style_text_font(status, &lv_font_montserrat_18, 0);
        lv_obj_align(status, LV_ALIGN_CENTER, 0, -40);
        lv_obj_set_style_text_color(status, debug_logging ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF6B35), 0);

        /* Description */
        lv_obj_t *desc = lv_label_create(settings_screen);
        lv_label_set_text(desc, "Detailed logging of button\npresses, temperature\nreadings, and system\nevents.");
        lv_obj_set_style_text_font(desc, &lv_font_montserrat_14, 0);
        lv_obj_align(desc, LV_ALIGN_CENTER, 0, 10);

        /* ON / OFF buttons */
        lv_obj_t *on = lv_btn_create(settings_screen);
        lv_obj_set_size(on, 100, 50);
        lv_obj_align(on, LV_ALIGN_CENTER, -60, 70);
        lv_obj_set_style_bg_color(on, debug_logging ? lv_color_hex(0x00AA00) : lv_color_hex(0x666666), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(on); lv_label_set_text(l, "ON"); lv_obj_center(l); }

        lv_obj_t *off = lv_btn_create(settings_screen);
        lv_obj_set_size(off, 100, 50);
        lv_obj_align(off, LV_ALIGN_CENTER, 60, 70);
        lv_obj_set_style_bg_color(off, !debug_logging ? lv_color_hex(0xAA0000) : lv_color_hex(0x666666), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(off); lv_label_set_text(l, "OFF"); lv_obj_center(l); }

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Btn1: ON   Btn2: OFF   Key: OK");
        lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -20);
        return;
    }

    /* ---------- EXIT ---------- */
    if (current_settings_screen == SETTINGS_EXIT) {
        lv_label_set_text(title, "Save & Exit");
        lv_obj_t *q = lv_label_create(settings_screen);
        lv_label_set_text(q, "Save settings\nbefore exiting?");
        lv_obj_set_style_text_font(q, &lv_font_montserrat_20, 0);
        lv_obj_align(q, LV_ALIGN_CENTER, 0, -30);

        lv_obj_t *c = lv_btn_create(settings_screen);
        lv_obj_set_size(c, 100, 50);
        lv_obj_align(c, LV_ALIGN_CENTER, -60, 40);
        lv_obj_set_style_bg_color(c, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_set_style_border_width(c, exit_selection_cancel ? 3 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(c, lv_color_hex(0x00FF00), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(c); lv_label_set_text(l, "CANCEL"); lv_obj_center(l); }

        lv_obj_t *s = lv_btn_create(settings_screen);
        lv_obj_set_size(s, 100, 50);
        lv_obj_align(s, LV_ALIGN_CENTER, 60, 40);
        lv_obj_set_style_bg_color(s, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_obj_set_style_border_width(s, exit_selection_cancel ? 1 : 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(s, exit_selection_cancel ? lv_color_hex(0xFF6B35) : lv_color_hex(0x00FF00), LV_PART_MAIN);
        { lv_obj_t *l = lv_label_create(s); lv_label_set_text(l, "SAVE"); lv_obj_center(l); }

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Btn1: Cancel   Btn2: Save");
        lv_obj_align(inst, LV_ALIGN_BOTTOM_MID, 0, -20);
        return;
    }
}

/* ============================================================= */
/* ====================== GAUGE CONFIG ======================== */
/* ============================================================= */
void update_scale_config() {
    if (use_celsius) {
        lv_scale_set_range(temp_scale, 0, 400);
        static const char *lbl[] = {"0°C","50°C","100°C","150°C","200°C","250°C","300°C","350°C","400°C",nullptr};
        lv_scale_set_text_src(temp_scale, lbl);
    } else {
        lv_scale_set_range(temp_scale, 32, 752);
        static const char *lbl[] = {"32°F","122°F","212°F","302°F","392°F","482°F","572°F","662°F","752°F",nullptr};
        lv_scale_set_text_src(temp_scale, lbl);
    }
}

/* ============================================================= */
/* ====================== SENSOR READ ========================= */
/* ============================================================= */
void update_temperature_reading() {
    select_hub_channel(ncir_hub_channel, "object temperature read");
    current_object_temp   = mlx.readObjectTempC();
    select_hub_channel(ncir_hub_channel, "ambient temperature read");
    current_ambient_temp  = mlx.readAmbientTempC();

    // Debug logging throttled to every 5 seconds when debug is on
    static uint32_t last_debug_log = 0;
    if (debug_logging && (millis() - last_debug_log >= 5000)) {
        // Log raw sensor readings
        float obj_f = use_celsius ? current_object_temp : (current_object_temp * 9.0f/5.0f + 32.0f);
        float amb_f = use_celsius ? current_ambient_temp : (current_ambient_temp * 9.0f/5.0f + 32.0f);
        DEBUG_LOG("Sensor RAW - Object: %.2f°C (%.2f°%c), Ambient: %.2f°C (%.2f°%c)", 
                  current_object_temp, obj_f, use_celsius?'C':'F',
                  current_ambient_temp, amb_f, use_celsius?'C':'F');
        last_debug_log = millis();
    }

    // Basic temperature logging every 20 seconds when debug is off
    static uint32_t temp_interval = 0;
    if (millis() - temp_interval > 20000) {  // Every 20 seconds for main temperature logging
        if (!debug_logging) {  // Only show basic temp if debug is off
            Serial.printf("OBJ %.1f°C  AMB %.1f°C\n", current_object_temp, current_ambient_temp);
        }
        temp_interval = millis();
    }
}

/* ============================================================= */
/* ====================== DISPLAY UPDATE ====================== */
/* ============================================================= */
void update_temp_display_screen() {
    float obj = use_celsius ? current_object_temp : (current_object_temp * 9.0f/5.0f + 32.0f);
    float amb = use_celsius ? current_ambient_temp : (current_ambient_temp * 9.0f/5.0f + 32.0f);

    /* adaptive smoothing: instant on big jumps, fast smoothing on small moves */
    static float s_obj = 0, s_amb = 0;
    static bool init = false;
    if (!init) { s_obj = obj; s_amb = amb; init = true; }
    else {
        float obj_delta = fabsf(obj - s_obj);
        float amb_delta = fabsf(amb - s_amb);
        float big_step_obj = use_celsius ? 15.0f : 27.0f;   // ~15°C or ~27°F
        float big_step_amb = use_celsius ? 5.0f  : 9.0f;    // ambient less jumpy

        float alpha_fast = 0.6f;   // faster response
        float alpha_slow = 0.4f;   // still snappy

        if (obj_delta > big_step_obj) s_obj = obj;           // jump immediately
        else s_obj = s_obj + ( (obj_delta > 2.0f ? alpha_fast : alpha_slow) * (obj - s_obj) );

        if (amb_delta > big_step_amb) s_amb = amb;
        else s_amb = s_amb + ( (amb_delta > 1.0f ? alpha_fast : alpha_slow) * (amb - s_amb) );
    }

    // Debug logging: show raw vs displayed values (throttled to every 5 seconds)
    static uint32_t last_display_debug_log = 0;
    if (debug_logging && (millis() - last_display_debug_log >= 5000)) {
        float raw_obj = use_celsius ? current_object_temp : (current_object_temp * 9.0f/5.0f + 32.0f);
        float raw_amb = use_celsius ? current_ambient_temp : (current_ambient_temp * 9.0f/5.0f + 32.0f);
        DEBUG_LOG("Display TEMP - Raw: %.2f°%c -> Displayed: %.2f°%c (smoothed) | Ambient Raw: %.2f°%c -> Displayed: %.2f°%c",
                  raw_obj, use_celsius?'C':'F', s_obj, use_celsius?'C':'F',
                  raw_amb, use_celsius?'C':'F', s_amb, use_celsius?'C':'F');
        last_display_debug_log = millis();
    }

    /* Selective UI update: only update display if temperature changed by 1° or more */
    static float last_displayed_temp = 0;
    static bool display_init = false;
    if (!display_init) {
        last_displayed_temp = s_obj;
        display_init = true;
    }

    float temp_change = fabsf(s_obj - last_displayed_temp);
    bool should_update_ui = (temp_change >= 1.0f);

    if (should_update_ui) {
        last_displayed_temp = s_obj;

        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f°", s_obj);
        lv_label_set_text(object_temp_label, buf);

        snprintf(buf, sizeof(buf), "Ambient: %.0f°%c", s_amb, use_celsius?'C':'F');
        lv_label_set_text(ambient_temp_label, buf);

        lv_label_set_text(temp_unit_label, use_celsius?"Celsius":"Fahrenheit");

        lv_label_set_text(temp_status_label, "Active");

        /* colour according to Fahrenheit range */
        float f = use_celsius ? (s_obj * 9.0f/5.0f + 32.0f) : s_obj;
        lv_color_t c = get_temperature_color(f);
        lv_obj_set_style_border_color(temp_circle, c, LV_PART_MAIN);
        lv_obj_set_style_text_color(object_temp_label, c, 0);
    }
}

/* ============================================================= */
/* ====================== GAUGE UPDATE ======================== */
/* ============================================================= */
void update_temp_gauge_screen() {
    float temp = use_celsius ? current_object_temp : (current_object_temp * 9.0f/5.0f + 32.0f);

    static float smooth = 0;
    static bool init = false;
    if (!init) { smooth = temp; init = true; }
    else smooth = smooth * 0.8f + temp * 0.2f;

    /* Selective UI update: only update display if temperature changed by 1° or more */
    static float last_displayed_temp = 0;
    static bool gauge_init = false;
    if (!gauge_init) {
        last_displayed_temp = smooth;
        gauge_init = true;
    }

    // Debug logging: show raw vs displayed values (throttled to every 5 seconds)
    static uint32_t last_gauge_debug_log = 0;
    if (debug_logging && (millis() - last_gauge_debug_log >= 5000)) {
        float raw_obj = use_celsius ? current_object_temp : (current_object_temp * 9.0f/5.0f + 32.0f);
        DEBUG_LOG("Display GAUGE - Raw: %.2f°%c -> Displayed: %.2f°%c (smoothed)",
                  raw_obj, use_celsius?'C':'F', smooth, use_celsius?'C':'F');
        last_gauge_debug_log = millis();
    }

    float temp_change = fabsf(smooth - last_displayed_temp);
    bool should_update_ui = (temp_change >= 1.0f);

    if (should_update_ui) {
        last_displayed_temp = smooth;

        float minV = use_celsius ? 0.0f : 32.0f;
        float maxV = use_celsius ? 400.0f : 752.0f;
        float norm = (smooth - minV) / (maxV - minV);
        norm = LV_CLAMP(0.0f, norm, 1.0f);

        float angle = 135.0f + norm * 270.0f;

        const int cx = 160, cy = 100, len = 70;
        lv_point_precise_t pts[2];
        pts[0].x = cx;
        pts[0].y = cy;
        float rad = (angle - 90.0f) * (float)M_PI / 180.0f;
        pts[1].x = cx + len * cosf(rad);
        pts[1].y = cy + len * sinf(rad);

        lv_line_set_points(temp_gauge_needle, pts, 2);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f°%c", smooth, use_celsius?'C':'F');
        lv_label_set_text(temp_gauge_value_label, buf);
    }
}

void update_servo_screen() {
    const int clamped = constrain(current_servo_command, 0, 180);
    if (abs(clamped - last_servo_displayed_command) < 1) {
        return;
    }

    last_servo_displayed_command = clamped;

    char buf[32];
    if (clamped == 90) {
        snprintf(buf, sizeof(buf), "STOPPED");
    } else if (clamped < 90) {
        snprintf(buf, sizeof(buf), "CCW %d", 90 - clamped);
    } else {
        snprintf(buf, sizeof(buf), "CW %d", clamped - 90);
    }
    lv_label_set_text(servo_angle_label, buf);
    lv_bar_set_value(servo_bar, clamped, LV_ANIM_OFF);

    if (!joystick_available || !servo_unit_available) {
        snprintf(buf, sizeof(buf), "Servo hardware unavailable");
    } else {
        snprintf(buf, sizeof(buf), "Hub %u  8Servo %u  Cmd %d", servo_hub_channel, SERVO_OUTPUT_INDEX, clamped);
    }
    lv_label_set_text(servo_status_label, buf);
}

/* ============================================================= */
/* ====================== BATTERY MONITORING ================== */
/* ============================================================= */
void update_battery_reading() {
    uint32_t now = millis();
    if (now - last_battery_update >= battery_update_interval) {
        battery_voltage = M5.Power.getBatteryVoltage() / 1000.0f;  // Convert mV to V
        battery_level = M5.Power.getBatteryLevel();  // Get percentage (0-100)

        // Ensure battery level is within valid range
        if (battery_level < 0) battery_level = 0;
        if (battery_level > 100) battery_level = 100;

        DEBUG_LOG("Battery update - Level: %d%%, Voltage: %.2fV", battery_level, battery_voltage);
        last_battery_update = now;

        // Check battery alerts
        check_battery_alerts();
    }
}

void check_battery_alerts() {
    if (!battery_alerts_enabled) return;

    static bool low_alert_triggered = false;
    static bool critical_alert_triggered = false;

    // Low battery alert (20%)
    if (battery_level <= battery_low_threshold && !low_alert_triggered) {
        DEBUG_LOG("Low battery alert: %d%% <= %d%%", battery_level, battery_low_threshold);
        play_beep(600, 200); delay(150); play_beep(600, 200);
        low_alert_triggered = true;
    } else if (battery_level > battery_low_threshold + 5) {  // Reset with hysteresis
        low_alert_triggered = false;
    }

    // Critical battery alert (10%)
    if (battery_level <= battery_critical_threshold && !critical_alert_triggered) {
        DEBUG_LOG("Critical battery alert: %d%% <= %d%%", battery_level, battery_critical_threshold);
        play_beep(400, 500); delay(200); play_beep(400, 500); delay(200); play_beep(400, 500);
        critical_alert_triggered = true;
        // Dim display to conserve power
        M5.Display.setBrightness(32);  // Very dim
    } else if (battery_level > battery_critical_threshold + 5) {  // Reset with hysteresis
        if (critical_alert_triggered) {
            // Restore normal brightness
            M5.Display.setBrightness(brightness_level);
            DEBUG_LOG("Battery recovered, restoring brightness");
        }
        critical_alert_triggered = false;
    }
}

/* ============================================================= */
/* ====================== ALERTS ============================= */
/* ============================================================= */
void check_temp_alerts() {
    if (!alerts_enabled) return;

    // Convert alert threshold to Celsius for comparison
    float alert_threshold_c = use_celsius ? alert_temp_threshold : ((alert_temp_threshold - 32.0f) * 5.0f/9.0f);

    DEBUG_LOG("Checking alerts - Temp: %.1f°C, Alert Threshold: %.1f°C (%.1f°F)",
              current_object_temp, alert_threshold_c, alert_temp_threshold);

    static bool alert_trig = false;

    if (current_object_temp >= alert_threshold_c && !alert_trig) {
        DEBUG_LOG("Temperature alert triggered: %.1f°C >= %.1f°C", current_object_temp, alert_threshold_c);
        play_beep(1200, 500); delay(100); play_beep(1200, 500);
        alert_trig = true; digitalWrite(LED_PIN, HIGH);
    } else if (current_object_temp < alert_threshold_c - 2.0f) {
        if (alert_trig) DEBUG_LOG("Temperature alert reset: %.1f°C < %.1f°C",
                                 current_object_temp, alert_threshold_c - 2.0f);
        alert_trig = false; digitalWrite(LED_PIN, LOW);
    }
}

/* ============================================================= */
/* ====================== UTILS =============================== */
/* ============================================================= */
void play_beep(int frequency, int duration) {
    if (sound_enabled) {
        // Convert percentage to M5Stack volume range (64-255)
        uint8_t m5_volume = map(sound_volume, 25, 100, VOLUME_MIN, VOLUME_MAX);
        M5.Speaker.setVolume(m5_volume);
        M5.Speaker.tone(frequency, duration);
    }
}

void apply_emissivity_and_restart() {
    pending_emissivity = constrain(pending_emissivity, 0.10f, 1.00f);
    DEBUG_LOG("Applying emissivity %.2f and restarting", pending_emissivity);

    lv_obj_clean(settings_screen);
    lv_obj_set_style_bg_color(settings_screen, lv_color_hex(0x1a1a40), 0);

    lv_obj_t *msg = lv_label_create(settings_screen);
    lv_label_set_text(msg, "Writing emissivity...\nRestarting device...");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);
    lv_refr_now(nullptr);

    select_hub_channel(ncir_hub_channel, "write emissivity");
    mlx.writeEmissivity(pending_emissivity);
    delay(50);
    sensor_emissivity = pending_emissivity;
    save_preferences();
    Serial.printf("Emissivity written: %.2f. Restarting now.\n", sensor_emissivity);
    delay(800);
    ESP.restart();
}

lv_color_t get_temperature_color(float tF) {
    if (tF < 480) return lv_color_hex(0x87CEEB);
    if (tF <= 560) return lv_color_hex(0xFFA500);
    if (tF <= 620) return lv_color_hex(0x00FF00);
    return lv_color_hex(0xFF4500);
}

lv_color_t get_battery_color(int level) {
    if (level <= 10) return lv_color_hex(0xFF0000);  // Red - critical
    if (level <= 20) return lv_color_hex(0xFFA500);  // Orange - low
    if (level <= 50) return lv_color_hex(0xFFFF00);  // Yellow - medium
    return lv_color_hex(0x00FF00);                   // Green - good
}

bool select_hub_channel(uint8_t channel, const char* context) {
    if (!hub_available) {
        return true;
    }
    if (raw_select_hub_channel(channel)) {
        return true;
    }
    Serial.printf("Pa.HUB select failed for channel %u during %s\n", channel, context ? context : "operation");
    return false;
}

bool raw_select_hub_channel(uint8_t channel) {
    if (channel >= PAHUB_MAX_CHANNELS) {
        return false;
    }
    if (pahub_current_channel == channel) {
        return true;
    }
    Wire.beginTransmission(PAHUB_I2C_ADDRESS);
    Wire.write(static_cast<uint8_t>(1U << channel));
    if (Wire.endTransmission() == 0) {
        pahub_current_channel = channel;
        delayMicroseconds(500);
        return true;
    }
    return false;
}

bool i2c_device_present(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

bool find_device_on_hub(uint8_t address, uint8_t& found_channel, const char* label) {
    if (!hub_available) {
        if (i2c_device_present(address)) {
            found_channel = 0;
            Serial.printf("%s detected directly on PortA at 0x%02X\n", label, address);
            return true;
        }
        return false;
    }

    for (uint8_t channel = 0; channel < PAHUB_MAX_CHANNELS; ++channel) {
        if (!select_hub_channel(channel, label)) {
            continue;
        }
        delay(5);
        if (i2c_device_present(address)) {
            found_channel = channel;
            Serial.printf("%s detected on Pa.HUB channel %u at 0x%02X\n", label, channel, address);
            return true;
        }
    }
    return false;
}

bool try_init_mlx_on_channel(uint8_t channel, bool log_scan) {
    if (hub_available) {
        if (!select_hub_channel(channel, "MLX90614 init")) {
            return false;
        }
        delay(5);
        if (log_scan) {
            char context[48];
            snprintf(context, sizeof(context), "NCIR candidate channel %u", channel);
            log_i2c_scan_for_current_channel(context);
        }
    } else if (log_scan) {
        log_i2c_scan_for_current_channel("direct PortA before MLX init");
    }

    if (mlx.begin(MLX90614_I2C_ADDRESS, &Wire)) {
        if (hub_available) {
            Serial.printf("MLX90614 initialized on Pa.HUB channel %u\n", channel);
        } else {
            Serial.println("MLX90614 initialized directly on PortA");
        }
        return true;
    }
    return false;
}

void log_i2c_scan_for_current_channel(const char* context) {
    Serial.printf("I2C scan for %s:", context ? context : "current channel");
    bool found_any = false;
    for (uint8_t address = 0x03; address < 0x78; ++address) {
        if (i2c_device_present(address)) {
            Serial.printf(" 0x%02X", address);
            found_any = true;
        }
    }
    if (!found_any) {
        Serial.print(" none");
    }
    Serial.println();
}

float readJoystickX() {
    if (!joystick_available || !select_hub_channel(joystick_hub_channel, "joystick read")) {
        return 0.0f;
    }

    const float raw = static_cast<float>(joystick2.get_joy_adc_12bits_offset_value_x());
    float normalized = raw / 2048.0f;
    normalized = constrain(normalized, -1.0f, 1.0f);

    constexpr float dead_zone = 0.08f;
    if (fabsf(normalized) < dead_zone) {
        return 0.0f;
    }

    if (normalized > 0.0f) {
        normalized = (normalized - dead_zone) / (1.0f - dead_zone);
    } else {
        normalized = (normalized + dead_zone) / (1.0f - dead_zone);
    }
    return constrain(normalized, -1.0f, 1.0f);
}

bool setServoSpeed(uint8_t channel, int value) {
    if (!servo_unit_available) {
        return false;
    }
    const int clamped = constrain(value, 0, 180);
    if (!select_hub_channel(channel, "servo write")) {
        return false;
    }

    const bool ok = servoUnit.setServoAngle(SERVO_OUTPUT_INDEX, static_cast<uint8_t>(clamped));
    if (ok) {
        current_servo_command = clamped;
    } else {
        Serial.printf("Servo write failed on 8Servo output %u\n", SERVO_OUTPUT_INDEX);
    }
    return ok;
}

/* ============================================================= */
/* ====================== EVENT HANDLERS ===================== */
/* ============================================================= */
void main_menu_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        switch_to_screen((ScreenState)(intptr_t)lv_event_get_user_data(e));
}
void temp_display_back_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) switch_to_screen(SCREEN_MAIN_MENU);
}
void temp_gauge_back_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) switch_to_screen(SCREEN_MAIN_MENU);
}
void servo_back_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) switch_to_screen(SCREEN_MAIN_MENU);
}
void brightness_slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target_obj(e);
    brightness_level = lv_slider_get_value(slider);
    M5.Display.setBrightness(brightness_level);
    save_preferences();
}
void volume_slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target_obj(e);
    sound_volume = lv_slider_get_value(slider);
    M5.Speaker.setVolume(static_cast<uint8_t>(sound_volume * 2.55));
    save_preferences();
}
void temp_alert_slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target_obj(e);
    alert_temp_threshold = lv_slider_get_value(slider);
    alerts_changed_by_user = true;  // Mark that user changed alerts
    check_temp_alerts();  // Check immediately when user changes alerts
    save_preferences();
    switch_to_settings_screen();  // Refresh the UI to show updated value
}
void temp_alert_up_btn_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        alert_temp_threshold = min(alert_temp_threshold + 5.0f, 700.0f);
        alerts_changed_by_user = true;
        check_temp_alerts();
        save_preferences();
        switch_to_settings_screen();  // Refresh the screen to show updated value
    }
}
void temp_alert_down_btn_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        alert_temp_threshold = max(alert_temp_threshold - 5.0f, 400.0f);
        alerts_changed_by_user = true;
        check_temp_alerts();
        save_preferences();
        switch_to_settings_screen();  // Refresh the screen to show updated value
    }
}
/* ============================================================= */
/* ================ MAIN MENU HIGHLIGHT UPDATE ================= */
/* ============================================================= */
void update_main_menu_highlight() {
  lv_obj_set_style_border_width(temp_display_btn,   main_menu_selection == 0 ? 4 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(temp_gauge_btn,     main_menu_selection == 1 ? 4 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(servo_menu_btn,     main_menu_selection == 2 ? 4 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(settings_menu_btn,  main_menu_selection == 3 ? 4 : 0, LV_PART_MAIN);

  lv_obj_set_style_border_color(temp_display_btn,   lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_border_color(temp_gauge_btn,     lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_border_color(servo_menu_btn,     lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_border_color(settings_menu_btn,  lv_color_hex(0x00FF00), LV_PART_MAIN);
}
