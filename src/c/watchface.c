#include <pebble.h>

// settings
#define SETTING_BATTERY_PERCENT 0
#define SETTING_DARK_MODE 1 // no effect
#define SETTING_HOURLY_CHIME 1

// 2 digits for hour, 2 for minute
#define DIGIT_COUNT 4
// 10 images for digits 0-9
#define IMAGE_COUNT 10
// each image is 69x69 pixels
#define IMG_WIDTH 69
#define IMG_HEIGHT 69

// layout
#define DIGIT_HORIZONTAL_SPACING 6 // 144 - 69 - 69
#define DIGIT_VERICAL_SPACING 30 // 168 - 69 - 69
#define BAR_HEIGHT 2

static Window *s_main_window;

// fonts
static GFont s_date_font;
static GFont s_battery_font;
// text layers
static TextLayer *s_date_layer;
static TextLayer *s_battery_layer; 
// 4 image layers (hhmm)
static BitmapLayer *s_time_digits[DIGIT_COUNT];
// 10 images buffers (0-9)
static GBitmap *s_image_numbers[IMAGE_COUNT];
// ? layer
static Layer *s_battery_bar_layer;
static Layer *s_underline_layer;

// memory for previous state, to avoid unnecessary updates
static int s_prev_digits[DIGIT_COUNT] = {-1, -1, -1, -1};
static char s_prev_date[11] = "";
static int s_prev_battery_percent = -1;
static int s_battery_level;

static void update_battery() {
  BatteryChargeState charge_state = battery_state_service_peek();
  s_battery_level = charge_state.charge_percent;
  
  if (charge_state.charge_percent == s_prev_battery_percent) return;
  s_prev_battery_percent = charge_state.charge_percent;
  
  #if SETTING_BATTERY_PERCENT
    static char s_battery_buffer[8];
    layer_mark_dirty(text_layer_get_layer(s_battery_layer));
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d%%", charge_state.charge_percent);
    text_layer_set_text(s_battery_layer, s_battery_buffer);
  #endif
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Find the width of the bar (total width = 114px)
  int width = (s_battery_level * bounds.size.w) / 100;

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw the bar
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(0, 0, width, bounds.size.h), 0, GCornerNone);
}

static void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Update date string only if changed
  static char s_buffer[11];
  strftime(s_buffer, sizeof(s_buffer), "%a %d %b", tick_time);
  if (strncmp(s_prev_date, s_buffer, sizeof(s_buffer)) != 0) {
    strcpy(s_prev_date, s_buffer);
    text_layer_set_text(s_date_layer, s_buffer);
  }

  // Extract digits
  int digits[DIGIT_COUNT] = {
    tick_time->tm_hour / 10,
    tick_time->tm_hour % 10,
    tick_time->tm_min / 10,
    tick_time->tm_min % 10
  };

  // Set only changed digits
  for (int i = 0; i < DIGIT_COUNT; ++i) {
    if (s_prev_digits[i] != digits[i]) {
      s_prev_digits[i] = digits[i];
      bitmap_layer_set_bitmap(s_time_digits[i], s_image_numbers[digits[i]]);
      layer_mark_dirty(bitmap_layer_get_layer(s_time_digits[i]));
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

  // Create GBitmap
  s_image_numbers[0] = gbitmap_create_with_resource(RESOURCE_ID_ZERO);
  s_image_numbers[1] = gbitmap_create_with_resource(RESOURCE_ID_ONE);
  s_image_numbers[2] = gbitmap_create_with_resource(RESOURCE_ID_TWO);
  s_image_numbers[3] = gbitmap_create_with_resource(RESOURCE_ID_THREE);
  s_image_numbers[4] = gbitmap_create_with_resource(RESOURCE_ID_FOUR);
  s_image_numbers[5] = gbitmap_create_with_resource(RESOURCE_ID_FIVE);
  s_image_numbers[6] = gbitmap_create_with_resource(RESOURCE_ID_SIX);
  s_image_numbers[7] = gbitmap_create_with_resource(RESOURCE_ID_SEVEN);
  s_image_numbers[8] = gbitmap_create_with_resource(RESOURCE_ID_EIGHT);
  s_image_numbers[9] = gbitmap_create_with_resource(RESOURCE_ID_NINE);

  int screen_height = bounds.size.h;
  int screen_width = bounds.size.w;

  GRect digits[4];
  int left_start = (screen_width - IMG_WIDTH * 2 - DIGIT_HORIZONTAL_SPACING) / 2;
  int right_start = left_start + IMG_WIDTH + DIGIT_HORIZONTAL_SPACING;
  int top_start = (screen_height - IMG_HEIGHT * 2 - DIGIT_VERICAL_SPACING) / 2;
  int bottom_start = top_start + IMG_HEIGHT + DIGIT_VERICAL_SPACING;

  digits[0] = GRect(left_start, top_start, IMG_WIDTH, IMG_HEIGHT);
  digits[1] = GRect(right_start, top_start, IMG_WIDTH, IMG_HEIGHT);
  digits[2] = GRect(left_start, bottom_start, IMG_WIDTH, IMG_HEIGHT);
  digits[3] = GRect(right_start, bottom_start, IMG_WIDTH, IMG_HEIGHT);
  
  for (int i = 0; i < DIGIT_COUNT; i++) {
    s_time_digits[i] = bitmap_layer_create(digits[i]);
    bitmap_layer_set_bitmap(s_time_digits[i], s_image_numbers[0]);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_time_digits[i]));
  }

  // Create battery TextLayer
  #if SETTING_BATTERY_PERCENT
    int battery_top = screen_height / 2 - DIGIT_VERICAL_SPACING - 3; // 50
    s_battery_layer = text_layer_create(GRect(left_start, battery_top, IMG_WIDTH, 14));
    s_battery_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUBIK_14));
    text_layer_set_background_color(s_battery_layer, GColorClear);
    text_layer_set_text_color(s_battery_layer, GColorBlack);
    text_layer_set_text(s_battery_layer, "100%");
    text_layer_set_font(s_battery_layer, s_battery_font);
    text_layer_set_text_alignment(s_battery_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  #endif

  // Create battery meter Layer
  int x = (screen_height) / 2 - DIGIT_HORIZONTAL_SPACING - 6;
  s_battery_bar_layer = layer_create(GRect(0, x, screen_width, BAR_HEIGHT));
  layer_set_update_proc(s_battery_bar_layer, battery_update_proc);
  layer_add_child(window_get_root_layer(window), s_battery_bar_layer);

  int y = (screen_height) / 2 + DIGIT_HORIZONTAL_SPACING + 6;
  GRect underline = GRect(0, y, screen_width, BAR_HEIGHT);
  s_underline_layer = layer_create(underline);
  layer_set_update_proc(s_underline_layer, underline_update_proc);
  layer_add_child(window_layer, s_underline_layer);

  // Create date TextLayer
  static int date_font_size = 24;
  GRect date_bounds = GRect(0, (screen_height - date_font_size)/2 - 3, bounds.size.w, date_font_size);
  s_date_layer = text_layer_create(date_bounds);
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUBIK_24));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text(s_date_layer, "Sat 14 Jun");
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
  if (s_battery_font != NULL) {
    fonts_unload_custom_font(s_battery_font);
  }
  if (s_date_layer != NULL) {
    text_layer_destroy(s_date_layer);
  }
  if (s_battery_layer != NULL) {
    text_layer_destroy(s_battery_layer);
  }
  if (s_battery_bar_layer != NULL) {
    layer_destroy(s_battery_bar_layer);
  }
  if (s_underline_layer != NULL) {
    layer_destroy(s_underline_layer);
  }
  for (int i = 0; i < DIGIT_COUNT; i++) {
    if (s_time_digits[i] != NULL) {
      bitmap_layer_destroy(s_time_digits[i]);
    }
  }
  for (int i = 0; i < IMAGE_COUNT; i++) {
    if (s_image_numbers[i] != NULL) {
      gbitmap_destroy(s_image_numbers[i]);
    }
  }
}
 
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
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