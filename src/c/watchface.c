#include <pebble.h>

// settings
#define SETTING_BATTERY_PERCENT 1
#define SETTING_DARK_MODE 1 // no effect
#define SETTING_HOURLY_CHIME 1

// 2 digits for hours
#define DIGIT_COUNT 2
// 10 images for digits 0-9
#define IMAGE_COUNT 10
// each image is 69x69 pixels
#define IMG_WIDTH 69
#define IMG_HEIGHT 69

// layout
#define DIGIT_HORIZONTAL_SPACING 0 // 144 - 69 - 69
#define DIGIT_VERICAL_SPACING 24 // 168 - 69 - 69

#define T 3 // thickness of the border

static Window *s_main_window;

// fonts
static GFont s_day_font;
static GFont s_date_font;
static GFont s_minute_font;
static GFont s_battery_font;
// text layers
static TextLayer *s_day_layer;
static TextLayer *s_date_layer;
static TextLayer *s_minute_layer;
static TextLayer *s_battery_layer; 
// 4 image layers (hhmm)
static BitmapLayer *s_time_digits[DIGIT_COUNT];
// 10 images buffers (0-9)
static GBitmap *s_image_numbers[IMAGE_COUNT];
// ? layer
static Layer *s_battery_bar_layer;
static Layer *s_underline_layer;

// memory for previous state, to avoid unnecessary updates
static int s_prev_digits[DIGIT_COUNT] = {-1, -1};
static char s_prev_date[11] = "";
static int s_prev_battery_percent = -1;
static int s_battery_level;

static Layer *s_canvas_layer;

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  const int W = bounds.size.w;
  const int H = bounds.size.h;
  

  const int px_top_half  = W / 2;
  const int px_right     = H - T;
  const int px_bottom    = W - T;
  const int px_left      = H - T;
  const int px_top_left  = (W / 2) - T;

  const int perimeter = px_top_half + px_right + px_bottom + px_left + px_top_left;

  int dur_top_half  = (60 * px_top_half) / perimeter;
  int dur_right     = (60 * px_right)    / perimeter;
  int dur_bottom    = (60 * px_bottom)   / perimeter;
  int dur_left      = (60 * px_left)     / perimeter;
  int dur_top_left  = (60 * px_top_left) / perimeter;

  int dur_total = dur_top_half + dur_right + dur_bottom + dur_left + dur_top_left;
  if (dur_total < 60) dur_top_left += 60 - dur_total; // absorb rounding

  int m = localtime(&(time_t){time(NULL)})->tm_min;
  int elapsed = m;

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);

  if (elapsed < dur_top_half) {
    // Top-right (left → right), full width, no vertical inset
    int px = (W / 2) * elapsed / dur_top_half;
    graphics_fill_rect(ctx, GRect(W / 2, 0, px, T), 0, GCornerNone);

  } else if ((elapsed -= dur_top_half) < dur_right) {
    // Top-right done
    graphics_fill_rect(ctx, GRect(W / 2, 0, W / 2, T), 0, GCornerNone);

    // Right edge (top → bottom), inset by T
    int px = (H - T) * elapsed / dur_right;
    graphics_fill_rect(ctx, GRect(W - T, T, T, px), 0, GCornerNone);

  } else if ((elapsed -= dur_right) < dur_bottom) {
    // Top-right + right full
    graphics_fill_rect(ctx, GRect(W / 2, 0, W / 2, T), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(W - T, T, T, H - T), 0, GCornerNone);

    // Bottom (right → left), inset by T from bottom and sides
    int px = (W - T) * elapsed / dur_bottom;
    graphics_fill_rect(ctx, GRect(W - px - T, H - T, px, T), 0, GCornerNone);

  } else if ((elapsed -= dur_bottom) < dur_left) {
    // Top + right + bottom full
    graphics_fill_rect(ctx, GRect(W / 2, 0, W / 2, T), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(W - T, T, T, H - T), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(0, H - T, W - T, T), 0, GCornerNone);

    // Left (bottom → top), inset from all edges
    int px = (H - T) * elapsed / dur_left;
    graphics_fill_rect(ctx, GRect(0, H - T - px, T, px), 0, GCornerNone);

  } else {
    // All previous full
    graphics_fill_rect(ctx, GRect(W / 2, 0, W / 2, T), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(W - T, T, T, H - T), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(0, H - T, W - T, T), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(0, 0, T, H - T), 0, GCornerNone);

    elapsed -= dur_left;
    // Top-left (left → right), inset by T from top and sides
    int px = (W / 2 - T) * elapsed / dur_top_left;
    graphics_fill_rect(ctx, GRect(T, 0, px, T), 0, GCornerNone);
  }
}

static void update_battery() {
  BatteryChargeState charge_state = battery_state_service_peek();
  s_battery_level = charge_state.charge_percent;
  
  if (charge_state.charge_percent == s_prev_battery_percent) return;
  s_prev_battery_percent = charge_state.charge_percent;
  
  #if SETTING_BATTERY_PERCENT
    static char s_battery_buffer[8];
    layer_mark_dirty(text_layer_get_layer(s_battery_layer));
    snprintf(s_battery_buffer, sizeof(s_battery_buffer), "%d", charge_state.charge_percent);
    text_layer_set_text(s_battery_layer, s_battery_buffer);
  #endif
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_radial(
    ctx,
    bounds,
    GOvalScaleModeFitCircle,
    3,
    DEG_TO_TRIGANGLE(360 * (100 - s_battery_level) / 100),
    DEG_TO_TRIGANGLE(359)
  );
   
}

static void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  // update minutes
  static char s_minute_buffer[3];
  static char s_prev_minute[3] = "00";
  strftime(s_minute_buffer, sizeof(s_minute_buffer), "%M", tick_time);
  if (strncmp(s_prev_minute, s_minute_buffer, sizeof(s_minute_buffer)) != 0) {
    strcpy(s_prev_minute, s_minute_buffer);
    text_layer_set_text(s_minute_layer, s_minute_buffer);
  }

  // Update date string only if changed
  static char s_date_string[11];
  static char s_date_string2[13];
  strftime(s_date_string, sizeof(s_date_string), "%y %m %d", tick_time);

  if (strncmp(s_prev_date, s_date_string, sizeof(s_date_string)) != 0) {
    strcpy(s_prev_date, s_date_string);
    text_layer_set_text(s_date_layer, s_date_string);
    
    strftime(s_date_string2, sizeof(s_date_string2), "%a, %b", tick_time);
    text_layer_set_text(s_day_layer, s_date_string2);
  }

  // update hour
  int digits[4] = {
    tick_time->tm_hour / 10,
    tick_time->tm_hour % 10,
  };
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

  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

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
  int top_start = 2 + (screen_height - IMG_HEIGHT * 2 - DIGIT_VERICAL_SPACING) / 2;
  // int bottom_start = top_start + IMG_HEIGHT + DIGIT_VERICAL_SPACING;

  digits[0] = GRect(left_start, top_start, IMG_WIDTH, IMG_HEIGHT);
  digits[1] = GRect(right_start, top_start, IMG_WIDTH, IMG_HEIGHT);
  // digits[2] = GRect(left_start, bottom_start, IMG_WIDTH, IMG_HEIGHT);
  // digits[3] = GRect(right_start, bottom_start, IMG_WIDTH, IMG_HEIGHT);
  
  for (int i = 0; i < DIGIT_COUNT; i++) {
    s_time_digits[i] = bitmap_layer_create(digits[i]);
    bitmap_layer_set_bitmap(s_time_digits[i], s_image_numbers[0]);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_time_digits[i]));
  }

  // Create battery meter Layer
  // int x = (screen_height) / 2 + 15;
  // s_battery_bar_layer = layer_create(GRect(T*2, x, screen_width-T*4, 10));
  s_battery_bar_layer = layer_create(GRect(21, screen_height / 2 - 7, 32, 32));
  layer_set_update_proc(s_battery_bar_layer, battery_update_proc);
  layer_add_child(window_get_root_layer(window), s_battery_bar_layer);

  // Create battery TextLayer
  #if SETTING_BATTERY_PERCENT
    int battery_top = screen_height / 2; // 50
    s_battery_layer = text_layer_create(GRect(left_start, battery_top, IMG_WIDTH, 14));
    s_battery_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUBIK_14));
    text_layer_set_background_color(s_battery_layer, GColorClear);
    text_layer_set_text_color(s_battery_layer, GColorWhite);
    text_layer_set_text(s_battery_layer, "100%");
    text_layer_set_font(s_battery_layer, s_battery_font);
    text_layer_set_text_alignment(s_battery_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
  #endif

  // int y = (screen_height) / 2 + DIGIT_HORIZONTAL_SPACING + 6;
  GRect underline = GRect(0, screen_height - 55, screen_width, T);
  s_underline_layer = layer_create(underline);
  layer_set_update_proc(s_underline_layer, underline_update_proc);
  layer_add_child(window_layer, s_underline_layer);

  // Create date TextLayer
  static int date_font_size = 30;
  GRect day_bounds = GRect(0, screen_height - 55, bounds.size.w, date_font_size);
  s_day_layer = text_layer_create(day_bounds);
  s_day_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUBIK_24));
  text_layer_set_background_color(s_day_layer, GColorClear);
  text_layer_set_text_color(s_day_layer, GColorWhite);
  text_layer_set_text(s_day_layer, "Wednesday");
  text_layer_set_font(s_day_layer, s_day_font);
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));

  GRect date_bounds = GRect(0, screen_height - 30, bounds.size.w, date_font_size);
  s_date_layer = text_layer_create(date_bounds);
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUBIK_24));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text(s_date_layer, "2025 04 04");
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  GRect minute_bounds = GRect(screen_width / 2, screen_height / 2 - 22, bounds.size.w / 2, 48);
  s_minute_layer = text_layer_create(minute_bounds);
  s_minute_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_RUBIK_48));
  text_layer_set_background_color(s_minute_layer, GColorClear);
  text_layer_set_text_color(s_minute_layer, GColorWhite);
  text_layer_set_text(s_minute_layer, "00");
  text_layer_set_font(s_minute_layer, s_minute_font);
  text_layer_set_text_alignment(s_minute_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_minute_layer));


  // update date and time on launch
  update_time();
  update_battery();
}
 
static void main_window_unload(Window *window) {
  if (s_date_font != NULL) {
    fonts_unload_custom_font(s_date_font);
  }
  if (s_day_font != NULL) {
    fonts_unload_custom_font(s_day_font);
  }
  if (s_minute_font != NULL) {
    fonts_unload_custom_font(s_minute_font);
  }
  if (s_battery_font != NULL) {
    fonts_unload_custom_font(s_battery_font);
  }
  if (s_day_layer != NULL) {
    text_layer_destroy(s_day_layer);
  }
  if (s_date_layer != NULL) {
    text_layer_destroy(s_date_layer);
  }
  if (s_minute_layer != NULL) {
    text_layer_destroy(s_minute_layer);
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
  layer_mark_dirty(s_canvas_layer);

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