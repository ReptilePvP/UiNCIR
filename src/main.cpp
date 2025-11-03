#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <lvgl.h>
#include "lv_conf.h"
#include "m5gfx_lvgl.hpp"
#include <Preferences.h>
#include <math.h>                     // for M_PI

/* ------------------- Debug logging system ------------------- */
#define DEBUG_LOG(fmt, ...) if (debug_logging) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

/* ------------------- Hardware pins ------------------- */
#define LED_PIN      9
#define BUTTON1_PIN 17
#define BUTTON2_PIN 18
#define KEY_PIN      8

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
    SCREEN_SETTINGS
};

enum SettingsScreen {
    SETTINGS_MENU,
    SETTINGS_UNITS,
    SETTINGS_AUDIO,
    SETTINGS_DISPLAY,
    SETTINGS_ALERTS,
    SETTINGS_DEBUG,
    SETTINGS_EXIT
};

/* ------------------- Global settings ------------------- */
int  brightness_level = 128;      // 0-255
bool sound_enabled    = true;
int  sound_volume     = 70;       // 0-100
float low_temp_threshold  = 10.0f;
float high_temp_threshold = 40.0f;
bool alerts_enabled   = true;
bool use_celsius      = true;
int  update_rate      = 150;      // ms (faster updates for responsive readings)
bool debug_logging    = false;    // Enable/disable debug logging
bool alerts_changed_by_user = false;  // Track if user manually changed alerts
int main_menu_selection = 0;           // 0 = Thermometer, 1 = Gauge, 2 = Settings
uint32_t last_main_menu_button_time[3] = {0, 0, 0};  // debounce


/* ------------------- Temperature vars ------------------- */
float current_object_temp = 0.0f;
float current_ambient_temp = 0.0f;

/* ------------------- Preferences ------------------- */
Preferences preferences;

/* ------------------- UI objects ------------------- */
lv_obj_t *main_menu_screen;
lv_obj_t *temp_display_screen;
lv_obj_t *temp_gauge_screen;
lv_obj_t *settings_screen;

/* ---- Main menu ---- */
lv_obj_t *temp_display_btn;
lv_obj_t *temp_gauge_btn;
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
void create_settings_ui();
void switch_to_settings_screen();
void update_scale_config();
void update_temperature_reading();
void update_temp_display_screen();
void update_temp_gauge_screen();
void play_beep(int freq, int dur);
void check_temp_alerts();
lv_color_t get_temperature_color(float tempF);
void update_main_menu_highlight();


/* ------------------- Event callbacks ------------------- */
void main_menu_event_cb(lv_event_t *e);
void temp_display_back_event_cb(lv_event_t *e);
void temp_gauge_back_event_cb(lv_event_t *e);
void brightness_slider_event_cb(lv_event_t *e);
void volume_slider_event_cb(lv_event_t *e);
void temp_alert_slider_event_cb(lv_event_t *e);

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

    /* ---- Sensor ---- */
    if (!mlx.begin()) {
        Serial.println("MLX90614 init failed!");
        while (1);
    }
    Serial.println("MLX90614 OK");

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
      // Only check alerts if debug is on OR if user changed alerts
      if (debug_logging || alerts_changed_by_user) {
          check_temp_alerts();
          // Reset flag after use - ongoing checks will be handled by debug state
          if (alerts_changed_by_user) {
              alerts_changed_by_user = false;
          }
      }
      last_update = millis();
  }

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
          if (current_screen == SCREEN_MAIN_MENU) {
              main_menu_selection = (main_menu_selection - 1 + 3) % 3;
              update_main_menu_highlight();
              DEBUG_LOG("Main menu selection: %d", main_menu_selection);
          } else if (current_screen == SCREEN_SETTINGS) {
              DEBUG_LOG("Settings screen: %d", current_settings_screen);
              if (current_settings_screen == SETTINGS_MENU) {
                  current_settings_selection = (current_settings_selection + 1) % 6;
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
          if (current_screen == SCREEN_MAIN_MENU) {
              main_menu_selection = (main_menu_selection + 1) % 3;
              update_main_menu_highlight();
              DEBUG_LOG("Main menu selection: %d", main_menu_selection);
          } else if (current_screen == SCREEN_SETTINGS) {
              DEBUG_LOG("Settings screen: %d", current_settings_screen);
              if (current_settings_screen == SETTINGS_MENU) {
                  current_settings_selection = (current_settings_selection - 1 + 6) % 6;
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
          if (current_screen == SCREEN_MAIN_MENU) {
              DEBUG_LOG("Main menu select: %d", main_menu_selection);
              switch (main_menu_selection) {
                  case 0: switch_to_screen(SCREEN_TEMP_DISPLAY); break;
                  case 1: switch_to_screen(SCREEN_TEMP_GAUGE);   break;
                  case 2: switch_to_screen(SCREEN_SETTINGS);     break;
              }
          } else if (current_screen == SCREEN_SETTINGS) {
              DEBUG_LOG("Settings screen: %d, selection: %d", current_settings_screen, current_settings_selection);
              if (current_settings_screen == SETTINGS_MENU) {
                  switch (current_settings_selection) {
                      case 0: current_settings_screen = SETTINGS_UNITS;   break;
                      case 1: current_settings_screen = SETTINGS_AUDIO;   break;
                      case 2: current_settings_screen = SETTINGS_DISPLAY; break;
                      case 3: current_settings_screen = SETTINGS_ALERTS;  break;
                      case 4: current_settings_screen = SETTINGS_DEBUG;   break;
                      case 5: current_settings_screen = SETTINGS_EXIT;    break;
                  }
                  switch_to_settings_screen();
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
    low_temp_threshold  = preferences.getFloat("low", 10.0f);
    high_temp_threshold = preferences.getFloat("high",40.0f);
    debug_logging    = preferences.getBool("debug",   false);
    preferences.end();
    DEBUG_LOG("Preferences loaded - Units:%s Bright:%d Sound:%s Vol:%d Alerts:%s Debug:%s",
              use_celsius ? "C" : "F", brightness_level, sound_enabled ? "ON" : "OFF",
              sound_volume, alerts_enabled ? "ON" : "OFF", debug_logging ? "ON" : "OFF");
}
void save_preferences() {
    DEBUG_LOG("Saving preferences...");
    preferences.begin("ncir", false);
    preferences.putBool ("celsius", use_celsius);
    preferences.putInt  ("bright",  brightness_level);
    preferences.putBool ("sound",   sound_enabled);
    preferences.putInt  ("volume",  sound_volume);
    preferences.putBool ("alerts",  alerts_enabled);
    preferences.putFloat("low",     low_temp_threshold);
    preferences.putFloat("high",    high_temp_threshold);
    preferences.putBool ("debug",   debug_logging);
    preferences.end();
    DEBUG_LOG("Preferences saved - Units:%s Bright:%d Sound:%s Vol:%d Alerts:%s Debug:%s",
              use_celsius ? "C" : "F", brightness_level, sound_enabled ? "ON" : "OFF",
              sound_volume, alerts_enabled ? "ON" : "OFF", debug_logging ? "ON" : "OFF");
}

/* ============================================================= */
/* ====================== SCREEN SWITCH ======================= */
/* ============================================================= */
void switch_to_screen(ScreenState s) {
    if (s == current_screen) return;

    const char *screen_names[] = {"Main Menu", "Temp Display", "Temp Gauge", "Settings"};
    DEBUG_LOG("Screen transition: %s -> %s", screen_names[current_screen], screen_names[s]);

    current_screen = s;

    switch (s) {
        case SCREEN_MAIN_MENU:     lv_screen_load(main_menu_screen);      break;
        case SCREEN_TEMP_DISPLAY:  lv_screen_load(temp_display_screen);
                                   update_temp_display_screen();      break;
        case SCREEN_TEMP_GAUGE:    lv_screen_load(temp_gauge_screen);
                                   update_scale_config();
                                   update_temp_gauge_screen();        break;
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

  /* buttons */
  int bw = 90, bh = 100, sp = 10, yoff = 25;

  temp_display_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(temp_display_btn, bw, bh);
  lv_obj_align(temp_display_btn, LV_ALIGN_LEFT_MID, sp, yoff);
  lv_obj_set_style_bg_color(temp_display_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(temp_display_btn, lv_color_hex(0xFF6B35), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(temp_display_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_add_event_cb(temp_display_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_TEMP_DISPLAY);
  { lv_obj_t *l = lv_label_create(temp_display_btn); lv_label_set_text(l, "Thermometer"); lv_obj_center(l); }

  temp_gauge_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(temp_gauge_btn, bw, bh);
  lv_obj_align(temp_gauge_btn, LV_ALIGN_CENTER, 0, yoff);
  lv_obj_set_style_bg_color(temp_gauge_btn, lv_color_hex(0x2c3e50), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_color(temp_gauge_btn, lv_color_hex(0x4285F4), LV_PART_MAIN);
  lv_obj_set_style_bg_grad_dir(temp_gauge_btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
  lv_obj_add_event_cb(temp_gauge_btn, main_menu_event_cb, LV_EVENT_CLICKED, (void*)SCREEN_TEMP_GAUGE);
  { lv_obj_t *l = lv_label_create(temp_gauge_btn); lv_label_set_text(l, "Gauge"); lv_obj_center(l); }

  settings_menu_btn = lv_btn_create(main_menu_screen);
  lv_obj_set_size(settings_menu_btn, bw, bh);
  lv_obj_align(settings_menu_btn, LV_ALIGN_RIGHT_MID, -sp, yoff);
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
        const char *items[] = {"Units","Audio","Display","Alerts","Debug","Exit"};
        for (int i = 0; i < 6; ++i) {
            lv_obj_t *b = lv_btn_create(settings_screen);
            lv_obj_set_size(b, 90, 50);
            int row = i / 3, col = i % 3;
            lv_obj_align(b, LV_ALIGN_CENTER,
                         (col==0?-100:(col==1?0:100)),
                         (row==0?-35:(row==1?35:95)));
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

        /* Low slider */
        lv_obj_t *lc = lv_obj_create(settings_screen);
        lv_obj_set_size(lc, 280, 50);
        lv_obj_align(lc, LV_ALIGN_CENTER, 0, 10);
        lv_obj_set_style_bg_color(lc, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_border_width(lc, 2, 0);
        lv_obj_set_style_border_color(lc, lv_color_hex(0x0099FF), 0);
        lv_obj_set_style_radius(lc, 10, 0);
        { lv_obj_t *t = lv_label_create(lc); lv_label_set_text(t, "Low Threshold (°C)"); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 5); }

        lv_obj_t *ls = lv_slider_create(lc);
        lv_obj_set_size(ls, 200, 10);
        lv_obj_align(ls, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_slider_set_range(ls, -20, 40);
        lv_slider_set_value(ls, low_temp_threshold, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(ls, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_bg_color(ls, lv_color_hex(0x0099FF), LV_PART_INDICATOR);
        lv_obj_add_event_cb(ls, temp_alert_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)0);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", low_temp_threshold);
        lv_obj_t *ll = lv_label_create(lc);
        lv_label_set_text(ll, buf);
        lv_obj_align(ll, LV_ALIGN_CENTER, 110, 0);

        /* High slider */
        lv_obj_t *hc = lv_obj_create(settings_screen);
        lv_obj_set_size(hc, 280, 50);
        lv_obj_align(hc, LV_ALIGN_CENTER, 0, 70);
        lv_obj_set_style_bg_color(hc, lv_color_hex(0x2c3e50), 0);
        lv_obj_set_style_border_width(hc, 2, 0);
        lv_obj_set_style_border_color(hc, lv_color_hex(0xFF6600), 0);
        lv_obj_set_style_radius(hc, 10, 0);
        { lv_obj_t *t = lv_label_create(hc); lv_label_set_text(t, "High Threshold (°C)"); lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 5); }

        lv_obj_t *hs = lv_slider_create(hc);
        lv_obj_set_size(hs, 200, 10);
        lv_obj_align(hs, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_slider_set_range(hs, 30, 100);
        lv_slider_set_value(hs, high_temp_threshold, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(hs, lv_color_hex(0x34495e), LV_PART_MAIN);
        lv_obj_set_style_bg_color(hs, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
        lv_obj_add_event_cb(hs, temp_alert_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void*)1);

        snprintf(buf, sizeof(buf), "%.1f", high_temp_threshold);
        lv_obj_t *hl = lv_label_create(hc);
        lv_label_set_text(hl, buf);
        lv_obj_align(hl, LV_ALIGN_CENTER, 110, 0);

        lv_obj_t *inst = lv_label_create(settings_screen);
        lv_label_set_text(inst, "Btn1: ON   Btn2: OFF   Key: OK");
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
    current_object_temp   = mlx.readObjectTempC();
    current_ambient_temp  = mlx.readAmbientTempC();

    // Debug logging throttled to every 5 seconds when debug is on
    static uint32_t last_debug_log = 0;
    if (debug_logging && (millis() - last_debug_log >= 5000)) {
        DEBUG_LOG("Temperature read - Object: %.1f°C, Ambient: %.1f°C", current_object_temp, current_ambient_temp);
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

/* ============================================================= */
/* ====================== ALERTS ============================= */
/* ============================================================= */
void check_temp_alerts() {
    if (!alerts_enabled) return;

    DEBUG_LOG("Checking alerts - Temp: %.1f°C, Low: %.1f°C, High: %.1f°C",
              current_object_temp, low_temp_threshold, high_temp_threshold);

    static bool low_trig = false, high_trig = false;

    if (current_object_temp <= low_temp_threshold && !low_trig) {
        DEBUG_LOG("Low temperature alert triggered: %.1f°C <= %.1f°C", current_object_temp, low_temp_threshold);
        play_beep(800, 300); delay(100); play_beep(800, 300);
        low_trig = true; digitalWrite(LED_PIN, HIGH);
    } else if (current_object_temp > low_temp_threshold + 2.0f) {
        if (low_trig) DEBUG_LOG("Low temperature alert reset: %.1f°C > %.1f°C",
                               current_object_temp, low_temp_threshold + 2.0f);
        low_trig = false; digitalWrite(LED_PIN, LOW);
    }

    if (current_object_temp >= high_temp_threshold && !high_trig) {
        DEBUG_LOG("High temperature alert triggered: %.1f°C >= %.1f°C", current_object_temp, high_temp_threshold);
        play_beep(1200, 500); delay(100); play_beep(1200, 500);
        high_trig = true; digitalWrite(LED_PIN, HIGH);
    } else if (current_object_temp < high_temp_threshold - 2.0f) {
        if (high_trig) DEBUG_LOG("High temperature alert reset: %.1f°C < %.1f°C",
                                current_object_temp, high_temp_threshold - 2.0f);
        high_trig = false; digitalWrite(LED_PIN, LOW);
    }
}

/* ============================================================= */
/* ====================== UTILS =============================== */
/* ============================================================= */
void play_beep(int f, int d) {
    if (sound_enabled) M5.Speaker.tone(f, d, 0, true);
}

lv_color_t get_temperature_color(float tF) {
    if (tF < 480) return lv_color_hex(0x87CEEB);
    if (tF <= 560) return lv_color_hex(0xFFA500);
    if (tF <= 620) return lv_color_hex(0x00FF00);
    return lv_color_hex(0xFF4500);
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
    int high = (int)(intptr_t)lv_event_get_user_data(e);
    if (high) high_temp_threshold = lv_slider_get_value(slider);
    else      low_temp_threshold  = lv_slider_get_value(slider);
    alerts_changed_by_user = true;  // Mark that user changed alerts
    check_temp_alerts();  // Check immediately when user changes alerts
    save_preferences();
}
/* ============================================================= */
/* ================ MAIN MENU HIGHLIGHT UPDATE ================= */
/* ============================================================= */
void update_main_menu_highlight() {
  lv_obj_set_style_border_width(temp_display_btn,   main_menu_selection == 0 ? 4 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(temp_gauge_btn,     main_menu_selection == 1 ? 4 : 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(settings_menu_btn,  main_menu_selection == 2 ? 4 : 0, LV_PART_MAIN);

  lv_obj_set_style_border_color(temp_display_btn,   lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_border_color(temp_gauge_btn,     lv_color_hex(0x00FF00), LV_PART_MAIN);
  lv_obj_set_style_border_color(settings_menu_btn,  lv_color_hex(0x00FF00), LV_PART_MAIN);
}
