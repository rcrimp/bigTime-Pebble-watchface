#pragma once
#include <pebble.h>

#define IMAGE_COUNT 10
#define IMG_WIDTH 69
#define IMG_HEIGHT 69

extern GBitmap *s_image_numbers[IMAGE_COUNT];

typedef struct {
    Layer *layer;
    int number; // The number to display
} BigDigitWidget;

BigDigitWidget *widget_big_digit_create(GPoint origin, int number);
void widget_big_digit_set(BigDigitWidget *widget, int number);
void widget_big_digit_destroy(BigDigitWidget *widget);
void widget_big_digit_update(Layer *layer, GContext *ctx);
Layer *widget_big_digit_get_layer(BigDigitWidget *widget);

void widget_big_digit_load_images(void);
void widget_big_digit_unload_images(void);