#include <pebble.h>
#include "modules/big_digit.h"

// settings
#define SETTING_DARK_MODE 1
#define SETTING_HOURLY_CHIME 1

static Window *s_main_window;

// fonts
static GFont s_date_font;

// layers
static TextLayer *s_date_layer;
static Layer *s_battery_bar_layer;
static Layer *s_underline_layer;
static BigDigitWidget *s_big_digit_widgets[4];

// cache
static int s_prev_battery_percent = -1;
static int s_prev_time_digits[] = {-1, -1, -1, -1};
static time_t s_prev_date = -1;

static void update_battery() {
  BatteryChargeState charge_state = battery_state_service_peek();
  
  if (charge_state.charge_percent == s_prev_battery_percent) return;
  s_prev_battery_percent = charge_state.charge_percent;
  
  if (s_battery_bar_layer != NULL) {
    layer_mark_dirty(s_battery_bar_layer);
  }
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  int width = s_prev_battery_percent * bounds.size.w / 100;
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(0, 0, width, bounds.size.h), 0, GCornerNone);
}

static void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  // update date layer
  if (s_prev_date != temp) {
    static char date_buffer[16];
    strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
    text_layer_set_text(s_date_layer, date_buffer);
    
    s_prev_date = temp;
  }

  // update big digits
  int time_digits[4];
  time_digits[0] = tick_time->tm_hour / 10; // tens of hour
  time_digits[1] = tick_time->tm_hour % 10; // units of hour
  time_digits[2] = tick_time->tm_min / 10; // tens of minute
  time_digits[3] = tick_time->tm_min % 10; // units of minute
  for (int i = 0; i < 4; i++) {
    if (s_prev_time_digits[i] != time_digits[i]) {
      s_prev_time_digits[i] = time_digits[i];
      widget_big_digit_set(s_big_digit_widgets[i], time_digits[i]);
      layer_mark_dirty(widget_big_digit_get_layer(s_big_digit_widgets[i]));
    }
  }
}

static void underline_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}
 
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // GRect layout
  int screen_height = bounds.size.h;
  int screen_width = bounds.size.w;
  int DIGIT_HORIZONTAL_SPACING = 2;
  int date_height = 30;
  int thickness = 2;
  int inset = 2;

  int left_start = (screen_width - IMG_WIDTH * 2 - DIGIT_HORIZONTAL_SPACING) / 2;
  int right_start = left_start + IMG_WIDTH + DIGIT_HORIZONTAL_SPACING;
  int top_start = (screen_height - IMG_HEIGHT * 2 - date_height) / 2;
  int bottom_start = top_start + IMG_HEIGHT + date_height;

  GRect battery_bar_bounds = GRect(
    0, (screen_height - date_height) / 2 + inset,
    screen_width, thickness
  );
  GRect underline = GRect(
    0, (screen_height + date_height) / 2 - thickness - inset,
    screen_width, thickness
  );
  GRect date_bounds = GRect(
    0, (screen_height - date_height) / 2,
    bounds.size.w, date_height
  );
  
  s_big_digit_widgets[0] = widget_big_digit_create(GPoint(left_start, top_start), 0);
  s_big_digit_widgets[1] = widget_big_digit_create(GPoint(right_start, top_start), 0);
  s_big_digit_widgets[2] = widget_big_digit_create(GPoint(left_start, bottom_start), 0);
  s_big_digit_widgets[3] = widget_big_digit_create(GPoint(right_start, bottom_start), 0);
  layer_add_child(window_layer, widget_big_digit_get_layer(s_big_digit_widgets[0]));
  layer_add_child(window_layer, widget_big_digit_get_layer(s_big_digit_widgets[1]));
  layer_add_child(window_layer, widget_big_digit_get_layer(s_big_digit_widgets[2]));
  layer_add_child(window_layer, widget_big_digit_get_layer(s_big_digit_widgets[3]));
  
  // Create battery meter Layer
  s_battery_bar_layer = layer_create(battery_bar_bounds);
  layer_set_update_proc(s_battery_bar_layer, battery_update_proc);
  layer_add_child(window_get_root_layer(window), s_battery_bar_layer);
  
  s_underline_layer = layer_create(underline);
  layer_set_update_proc(s_underline_layer, underline_update_proc);
  layer_add_child(window_layer, s_underline_layer);
  
  // Create date TextLayer
  s_date_layer = text_layer_create(date_bounds);
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUBIK_24));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text(s_date_layer, "Sat 21 Jun");
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // update date and time on launch
  update_time();
  update_battery();
}
 
static void main_window_unload(Window *window) {
  if (s_date_font != NULL) {
    fonts_unload_custom_font(s_date_font);
  }
  if (s_date_layer != NULL) {
    text_layer_destroy(s_date_layer);
  }
  if (s_underline_layer != NULL) {
    layer_destroy(s_underline_layer);
  }
  if (s_battery_bar_layer != NULL) {
    layer_destroy(s_battery_bar_layer);
  }
  for (int i = 0; i < 4; i++) {
    if (s_big_digit_widgets[i] != NULL) {
      widget_big_digit_destroy(s_big_digit_widgets[i]);
      s_big_digit_widgets[i] = NULL;
    }
  }
  widget_big_digit_unload_images();
}
 
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // layer_mark_dirty(s_canvas_layer);

  // update time and date on screen
  update_time();
  // hourly chime
  if (SETTING_HOURLY_CHIME && tick_time->tm_min == 0 && !quiet_time_is_active()) {
    vibes_double_pulse();
  }
}

static void battery_handler(BatteryChargeState charge_state) {
  update_battery();
  layer_mark_dirty(s_battery_bar_layer);
}
  
static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  
  // update time every minute
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
}
 
static void deinit() {
  window_destroy(s_main_window);
}
 
int main(void) {
  init();
  app_event_loop();
  deinit();
}