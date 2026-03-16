#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include <M5Unified.h>
#include <lvgl.h>
#include <Adafruit_MLX90614.h>

// -----------------------------
// CoreS3 / Port A I2C
// -----------------------------
static constexpr int PORTA_SDA = 2;
static constexpr int PORTA_SCL = 1;
static constexpr uint32_t I2C_FREQ = 100000;

// -----------------------------
// Pa.HUB + devices
// -----------------------------
static constexpr uint8_t PAHUB_ADDR   = 0x70;
static constexpr uint8_t PAHUB_JOY_CH = 1;
static constexpr uint8_t PAHUB_MLX_CH = 5;

// Joystick2 raw I2C
static constexpr uint8_t JOYSTICK2_ADDR        = 0x63;
static constexpr uint8_t JOYSTICK2_REG_ADC_X_8 = 0x10;
static constexpr uint8_t JOYSTICK2_REG_ADC_Y_8 = 0x11;
static constexpr uint8_t JOYSTICK2_REG_BUTTON  = 0x20;

// MLX90614
static constexpr uint8_t MLX_SENSOR_ADDR = 0x5A;

// -----------------------------
// Timing
// -----------------------------
static constexpr uint32_t LV_TICK_MS = 5;
static constexpr uint32_t UI_UPDATE_MS = 80;
static constexpr uint32_t JOY_UPDATE_MS = 35;

// -----------------------------
// Joystick tuning
// -----------------------------
static constexpr int JOY_CENTER = 128;
static constexpr int JOY_FILTER_DIV = 3;

// Thresholds for navigation
static constexpr int JOY_NAV_H_THRESH = 45;
static constexpr int JOY_NAV_V_THRESH = 45;
static constexpr uint32_t JOY_NAV_REPEAT_MS = 220;
static constexpr uint32_t JOY_BUTTON_DEBOUNCE_MS = 220;

// -----------------------------
// LVGL globals
// -----------------------------
static lv_display_t* g_display = nullptr;
static uint16_t draw_buf_1[320 * 20];
static uint16_t draw_buf_2[320 * 20];

static lv_obj_t* tabview = nullptr;

// Live tab
static lv_obj_t* lbl_live_object = nullptr;
static lv_obj_t* lbl_live_ambient = nullptr;
static lv_obj_t* lbl_live_status = nullptr;
static lv_obj_t* bar_live_temp = nullptr;

// Stats tab
static lv_obj_t* lbl_stats_min = nullptr;
static lv_obj_t* lbl_stats_max = nullptr;
static lv_obj_t* lbl_stats_last = nullptr;
static lv_obj_t* lbl_stats_reads = nullptr;

// Settings tab
static lv_obj_t* lbl_settings_units = nullptr;
static lv_obj_t* lbl_settings_refresh = nullptr;
static lv_obj_t* lbl_settings_hint = nullptr;

// About tab
static lv_obj_t* lbl_about = nullptr;

// -----------------------------
// App state
// -----------------------------
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

static bool sensor_ok = false;
static bool use_fahrenheit = true;

static int current_tab = 0;
static constexpr int TAB_COUNT = 4;

static int refresh_options_ms[] = {35, 60, 100, 150};
static int refresh_index = 1;  // 60 ms default

static float object_temp_f = 0.0f;
static float ambient_temp_f = 0.0f;
static float object_temp_c = 0.0f;
static float ambient_temp_c = 0.0f;

static float min_temp_f = 9999.0f;
static float max_temp_f = -9999.0f;
static uint32_t successful_reads = 0;
static bool temp_valid = false;

static int filtered_x = 0;
static int filtered_y = 0;
static bool last_button_pressed = false;

static uint32_t last_lv_tick = 0;
static uint32_t last_ui_update = 0;
static uint32_t last_joy_update = 0;
static uint32_t last_temp_update = 0;

static uint32_t last_lr_nav_ms = 0;
static uint32_t last_ud_nav_ms = 0;
static uint32_t last_button_ms = 0;

// -----------------------------
// Helpers
// -----------------------------
template <typename T>
static T clamp_value(T v, T lo, T hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static bool pahub_select(uint8_t ch) {
  Wire.beginTransmission(PAHUB_ADDR);
  Wire.write(1 << ch);
  bool ok = (Wire.endTransmission() == 0);
  delay(2);
  return ok;
}

static bool read_joystick2_raw(uint8_t& x, uint8_t& y, bool& pressed) {
  if (!pahub_select(PAHUB_JOY_CH)) return false;

  Wire.beginTransmission(JOYSTICK2_ADDR);
  Wire.write(JOYSTICK2_REG_ADC_X_8);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(JOYSTICK2_ADDR, (uint8_t)1);
  if (Wire.available() < 1) return false;
  x = Wire.read();

  Wire.beginTransmission(JOYSTICK2_ADDR);
  Wire.write(JOYSTICK2_REG_ADC_Y_8);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(JOYSTICK2_ADDR, (uint8_t)1);
  if (Wire.available() < 1) return false;
  y = Wire.read();

  Wire.beginTransmission(JOYSTICK2_ADDR);
  Wire.write(JOYSTICK2_REG_BUTTON);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(JOYSTICK2_ADDR, (uint8_t)1);
  if (Wire.available() < 1) return false;

  uint8_t btn = Wire.read();
  pressed = (btn == 0);
  return true;
}

static bool init_mlx() {
  if (!pahub_select(PAHUB_MLX_CH)) return false;
  delay(10);
  return mlx.begin(MLX_SENSOR_ADDR, &Wire);
}

static bool read_temps() {
  if (!pahub_select(PAHUB_MLX_CH)) return false;
  delay(1);

  double obj_f = mlx.readObjectTempF();
  double amb_f = mlx.readAmbientTempF();

  if (isnan(obj_f) || isnan(amb_f) || isinf(obj_f) || isinf(amb_f)) {
    return false;
  }

  object_temp_f = (float)obj_f;
  ambient_temp_f = (float)amb_f;
  object_temp_c = (object_temp_f - 32.0f) * (5.0f / 9.0f);
  ambient_temp_c = (ambient_temp_f - 32.0f) * (5.0f / 9.0f);

  if (object_temp_f < min_temp_f) min_temp_f = object_temp_f;
  if (object_temp_f > max_temp_f) max_temp_f = object_temp_f;

  successful_reads++;
  temp_valid = true;
  return true;
}

static const char* current_units_text() {
  return use_fahrenheit ? "F" : "C";
}

static int display_object_temp_whole() {
  return (int)lroundf(use_fahrenheit ? object_temp_f : object_temp_c);
}

static int display_ambient_temp_whole() {
  return (int)lroundf(use_fahrenheit ? ambient_temp_f : ambient_temp_c);
}

static int display_min_temp_whole() {
  float v = use_fahrenheit ? min_temp_f : ((min_temp_f - 32.0f) * (5.0f / 9.0f));
  return (int)lroundf(v);
}

static int display_max_temp_whole() {
  float v = use_fahrenheit ? max_temp_f : ((max_temp_f - 32.0f) * (5.0f / 9.0f));
  return (int)lroundf(v);
}

static int temp_bar_value() {
  float v = use_fahrenheit ? object_temp_f : object_temp_c;
  if (use_fahrenheit) {
    return clamp_value((int)lroundf(v), 0, 800);
  }
  return clamp_value((int)lroundf(v), 0, 450);
}

static lv_color_t zone_color_from_temp_f(float temp_f) {
  if (temp_f < 350.0f) return lv_color_hex(0x60A5FA);
  if (temp_f < 450.0f) return lv_color_hex(0x4ADE80);
  if (temp_f < 550.0f) return lv_color_hex(0xFB923C);
  return lv_color_hex(0xF87171);
}

static const char* zone_text_from_temp_f(float temp_f) {
  if (temp_f < 350.0f) return "LOW";
  if (temp_f < 450.0f) return "READY";
  if (temp_f < 550.0f) return "HOT";
  return "TOO HOT";
}

// -----------------------------
// LVGL display bridge
// -----------------------------
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;

  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, w, h);
  M5.Display.pushPixels(reinterpret_cast<const uint16_t*>(px_map), w * h, true);
  M5.Display.endWrite();

  lv_display_flush_ready(disp);
}

static void init_lvgl() {
  lv_init();

  g_display = lv_display_create(320, 240);
  lv_display_set_buffers(
      g_display,
      draw_buf_1,
      draw_buf_2,
      sizeof(draw_buf_1),
      LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(g_display, lvgl_flush_cb);
}

// -----------------------------
// UI
// -----------------------------
static void build_ui() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);

  tabview = lv_tabview_create(scr);
  lv_obj_set_size(tabview, 320, 240);
  lv_obj_center(tabview);

  lv_obj_t* tab_live = lv_tabview_add_tab(tabview, "Live");
  lv_obj_t* tab_stats = lv_tabview_add_tab(tabview, "Stats");
  lv_obj_t* tab_settings = lv_tabview_add_tab(tabview, "Settings");
  lv_obj_t* tab_about = lv_tabview_add_tab(tabview, "About");

  // Live
  lbl_live_object = lv_label_create(tab_live);
  lv_obj_set_style_text_font(lbl_live_object, &lv_font_montserrat_32, 0);
  lv_obj_set_style_text_color(lbl_live_object, lv_color_hex(0x000000), 0);
  lv_obj_align(lbl_live_object, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_label_set_text(lbl_live_object, "Object: -- F");

  lbl_live_ambient = lv_label_create(tab_live);
  lv_obj_set_style_text_font(lbl_live_ambient, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(lbl_live_ambient, lv_color_hex(0xCBD5E1), 0);
  lv_obj_align(lbl_live_ambient, LV_ALIGN_TOP_LEFT, 8, 58);
  lv_label_set_text(lbl_live_ambient, "Ambient: -- F");

  bar_live_temp = lv_bar_create(tab_live);
  lv_obj_set_size(bar_live_temp, 260, 18);
  lv_obj_align(bar_live_temp, LV_ALIGN_TOP_LEFT, 8, 96);
  lv_bar_set_range(bar_live_temp, 0, 800);
  lv_bar_set_value(bar_live_temp, 0, LV_ANIM_OFF);
  lv_obj_set_style_radius(bar_live_temp, 8, 0);

  lbl_live_status = lv_label_create(tab_live);
  lv_obj_set_style_text_font(lbl_live_status, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(lbl_live_status, lv_color_hex(0xFFFFFF), 0);
  lv_obj_align(lbl_live_status, LV_ALIGN_TOP_LEFT, 8, 128);
  lv_label_set_text(lbl_live_status, "Zone: --");

  // Stats
  lbl_stats_min = lv_label_create(tab_stats);
  lv_obj_set_style_text_font(lbl_stats_min, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_stats_min, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_label_set_text(lbl_stats_min, "Min: --");

  lbl_stats_max = lv_label_create(tab_stats);
  lv_obj_set_style_text_font(lbl_stats_max, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_stats_max, LV_ALIGN_TOP_LEFT, 8, 38);
  lv_label_set_text(lbl_stats_max, "Max: --");

  lbl_stats_last = lv_label_create(tab_stats);
  lv_obj_set_style_text_font(lbl_stats_last, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_stats_last, LV_ALIGN_TOP_LEFT, 8, 68);
  lv_label_set_text(lbl_stats_last, "Last: --");

  lbl_stats_reads = lv_label_create(tab_stats);
  lv_obj_set_style_text_font(lbl_stats_reads, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_stats_reads, LV_ALIGN_TOP_LEFT, 8, 98);
  lv_label_set_text(lbl_stats_reads, "Reads: 0");

  // Settings
  lbl_settings_units = lv_label_create(tab_settings);
  lv_obj_set_style_text_font(lbl_settings_units, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_settings_units, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_label_set_text(lbl_settings_units, "Units: F");

  lbl_settings_refresh = lv_label_create(tab_settings);
  lv_obj_set_style_text_font(lbl_settings_refresh, &lv_font_montserrat_18, 0);
  lv_obj_align(lbl_settings_refresh, LV_ALIGN_TOP_LEFT, 8, 38);
  lv_label_set_text(lbl_settings_refresh, "Refresh: 60 ms");

  lbl_settings_hint = lv_label_create(tab_settings);
  lv_obj_set_style_text_font(lbl_settings_hint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(lbl_settings_hint, lv_color_hex(0x94A3B8), 0);
  lv_obj_align(lbl_settings_hint, LV_ALIGN_TOP_LEFT, 8, 80);
  lv_label_set_text(
      lbl_settings_hint,
      "Left/Right: tabs\nUp/Down: refresh\nPress: toggle F/C");

  // About
  lbl_about = lv_label_create(tab_about);
  lv_obj_set_style_text_font(lbl_about, &lv_font_montserrat_16, 0);
  lv_obj_align(lbl_about, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_label_set_text(
      lbl_about,
      "CoreS3 + Pa.HUB\n"
      "Joystick2 raw I2C\n"
      "MLX90614 via Adafruit lib\n"
      "LVGL 9.5 dashboard");
}

static void update_ui() {
  char buf[96];

  // Live tab
  snprintf(buf, sizeof(buf), "Object: %d %s", display_object_temp_whole(), current_units_text());
  lv_label_set_text(lbl_live_object, buf);

  snprintf(buf, sizeof(buf), "Ambient: %d %s", display_ambient_temp_whole(), current_units_text());
  lv_label_set_text(lbl_live_ambient, buf);

  if (use_fahrenheit) {
    lv_bar_set_range(bar_live_temp, 0, 800);
  } else {
    lv_bar_set_range(bar_live_temp, 0, 450);
  }
  lv_bar_set_value(bar_live_temp, temp_bar_value(), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_live_temp, zone_color_from_temp_f(object_temp_f), LV_PART_INDICATOR);

  snprintf(buf, sizeof(buf), "Zone: %s", zone_text_from_temp_f(object_temp_f));
  lv_label_set_text(lbl_live_status, buf);
  lv_obj_set_style_text_color(lbl_live_status, zone_color_from_temp_f(object_temp_f), 0);

  // Stats
  if (successful_reads == 0) {
    lv_label_set_text(lbl_stats_min, "Min: --");
    lv_label_set_text(lbl_stats_max, "Max: --");
  } else {
    snprintf(buf, sizeof(buf), "Min: %d %s", display_min_temp_whole(), current_units_text());
    lv_label_set_text(lbl_stats_min, buf);

    snprintf(buf, sizeof(buf), "Max: %d %s", display_max_temp_whole(), current_units_text());
    lv_label_set_text(lbl_stats_max, buf);
  }

  snprintf(buf, sizeof(buf), "Last: %d %s", display_object_temp_whole(), current_units_text());
  lv_label_set_text(lbl_stats_last, buf);

  snprintf(buf, sizeof(buf), "Reads: %lu", (unsigned long)successful_reads);
  lv_label_set_text(lbl_stats_reads, buf);

  // Settings
  snprintf(buf, sizeof(buf), "Units: %s", current_units_text());
  lv_label_set_text(lbl_settings_units, buf);

  snprintf(buf, sizeof(buf), "Refresh: %d ms", refresh_options_ms[refresh_index]);
  lv_label_set_text(lbl_settings_refresh, buf);
}

// -----------------------------
// Joystick navigation
// -----------------------------
static void goto_tab(int idx) {
  current_tab = clamp_value(idx, 0, TAB_COUNT - 1);
  lv_tabview_set_active(tabview, current_tab, LV_ANIM_OFF);
}

static void handle_joystick_navigation() {
  uint8_t rawX = JOY_CENTER;
  uint8_t rawY = JOY_CENTER;
  bool pressed = false;

  if (!read_joystick2_raw(rawX, rawY, pressed)) return;

  int x = (int)rawX - JOY_CENTER;
  int y = JOY_CENTER - (int)rawY;  // invert so up is positive-ish

  filtered_x = (filtered_x * (JOY_FILTER_DIV - 1) + x) / JOY_FILTER_DIV;
  filtered_y = (filtered_y * (JOY_FILTER_DIV - 1) + y) / JOY_FILTER_DIV;

  uint32_t now = millis();

  // Left / right tabs
  if (filtered_x > JOY_NAV_H_THRESH && (now - last_lr_nav_ms > JOY_NAV_REPEAT_MS)) {
    goto_tab(current_tab + 1);
    last_lr_nav_ms = now;
  } else if (filtered_x < -JOY_NAV_H_THRESH && (now - last_lr_nav_ms > JOY_NAV_REPEAT_MS)) {
    goto_tab(current_tab - 1);
    last_lr_nav_ms = now;
  }

  // Settings adjustments with up/down
  if (current_tab == 2) {
    if (filtered_y > JOY_NAV_V_THRESH && (now - last_ud_nav_ms > JOY_NAV_REPEAT_MS)) {
      refresh_index = clamp_value(refresh_index - 1, 0, (int)(sizeof(refresh_options_ms) / sizeof(refresh_options_ms[0])) - 1);
      last_ud_nav_ms = now;
    } else if (filtered_y < -JOY_NAV_V_THRESH && (now - last_ud_nav_ms > JOY_NAV_REPEAT_MS)) {
      refresh_index = clamp_value(refresh_index + 1, 0, (int)(sizeof(refresh_options_ms) / sizeof(refresh_options_ms[0])) - 1);
      last_ud_nav_ms = now;
    }
  }

  // Button
  if (pressed && !last_button_pressed && (now - last_button_ms > JOY_BUTTON_DEBOUNCE_MS)) {
    if (current_tab == 2) {
      use_fahrenheit = !use_fahrenheit;
    }
    last_button_ms = now;
  }
  last_button_pressed = pressed;
}

// -----------------------------
// Arduino
// -----------------------------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  Serial.println("M5Stack CoreS3 started");
  Serial.printf("PortA I2C pins SDA=%d SCL=%d\n", PORTA_SDA, PORTA_SCL);

  Wire.begin(PORTA_SDA, PORTA_SCL, I2C_FREQ);

  init_lvgl();
  build_ui();

  sensor_ok = init_mlx();
  if (!sensor_ok) {
    temp_valid = false;
    Serial.println("MLX90614 init failed!");
  } else {
    Serial.println("MLX90614 init OK");
    read_temps();
  }

  update_ui();
}

void loop() {
  M5.update();

  uint32_t now = millis();

  if (now - last_lv_tick >= LV_TICK_MS) {
    lv_tick_inc(now - last_lv_tick);
    last_lv_tick = now;
  }

  if (now - last_joy_update >= JOY_UPDATE_MS) {
    handle_joystick_navigation();
    last_joy_update = now;
  }

  if (sensor_ok && (now - last_temp_update >= (uint32_t)refresh_options_ms[refresh_index])) {
    temp_valid = read_temps();
    last_temp_update = now;
  }

  if (now - last_ui_update >= UI_UPDATE_MS) {
    update_ui();
    last_ui_update = now;
  }

  lv_timer_handler();
  delay(2);
}