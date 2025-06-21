#include <pebble.h>
#include <stdlib.h>
#include "big_digit.h"

GBitmap *s_image_numbers[IMAGE_COUNT] = { 0 };

void widget_big_digit_update(Layer *layer, GContext *ctx) {
    BigDigitWidget *widget = *(BigDigitWidget **)layer_get_data(layer);
    GBitmap *bitmap = s_image_numbers[widget->number];
    if (bitmap){
        graphics_draw_bitmap_in_rect(ctx, bitmap, layer_get_bounds(layer));
    }
}

BigDigitWidget *widget_big_digit_create(GPoint origin, int number) {
    if (!s_image_numbers[0]) {
        widget_big_digit_load_images();
    }
    if (number < 0 || number > 9) return NULL;
    
    BigDigitWidget *widget = malloc(sizeof(BigDigitWidget));
    if (!widget) return NULL;

    GRect bounds = GRect(origin.x, origin.y, IMG_WIDTH, IMG_HEIGHT);
    widget->layer = layer_create_with_data(bounds, sizeof(BigDigitWidget *));
    if (!widget->layer) {
        free(widget);
        return NULL;
    }

    // Attach user data
    *(BigDigitWidget **)layer_get_data(widget->layer) = widget;
    layer_set_update_proc(widget->layer, widget_big_digit_update);

    widget->number = number;

    return widget;
}

void widget_big_digit_set(BigDigitWidget *widget, int number) {
    if (!widget) return;
    if (number == widget->number) return;
    if (number < 0 || number > 9) return;

    widget->number = number;
    
    // bitmap_layer_set_bitmap(widget->layer, s_image_numbers[number]);
    layer_mark_dirty(widget->layer);
}

void widget_big_digit_destroy(BigDigitWidget *widget) {
    if (widget) {
        layer_destroy(widget->layer);
        free(widget);
    }
}

Layer *widget_big_digit_get_layer(BigDigitWidget *widget) {
    if (widget) {
        return widget->layer;
    }
    return NULL;
}

void widget_big_digit_load_images() {
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
}

void widget_big_digit_unload_images(void) {
    for (int i = 0; i < IMAGE_COUNT; i++) {
        if (s_image_numbers[i]) {
            gbitmap_destroy(s_image_numbers[i]);
            s_image_numbers[i] = NULL;
        }
    }
}