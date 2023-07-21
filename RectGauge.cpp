
#include "RectGauge.h"

#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <initializer_list>
#include <vector>
#include <string>

#include <raylib.h>

#include "RaylibHelper.h"

// TODO parameterize this?
#define OUTLINE_STROKE_WIDTH 4.0f

// TODO support non-linear round_gauges

//RectGauge::RectGauge(const char *name, int x, int y, Vector2 size, Orientation orientation, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states, Font font) {
RectGauge::RectGauge(const char *name, int x, int y, Vector2 size, Orientation orientation, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states) {
    // TODO move some of this repeated code to Gauge
    this->name = name;
    std::vector<float> bounds_vector{bounds};
    std::vector<State> states_vector{states};
    float min = FLT_MAX;
    float max = FLT_MIN;
    std::vector<Range> ranges;
    for (int c = 0; c < numranges; c++) {
        Range next_range;
        next_range.min = bounds_vector[c];
        next_range.max = bounds_vector[c+1];
        next_range.state = states_vector[c];
        // They should be sorted, but what if they're not? will anything else fail?
        if (bounds_vector[c] > max) max = bounds_vector[c];
        if (bounds_vector[c] < min) min = bounds_vector[c];
        ranges.push_back(next_range);
    }
    // The above will have missed the last index
    if (bounds_vector[numranges] > max) max = bounds_vector[numranges];
    if (bounds_vector[numranges] < min) min = bounds_vector[numranges];

    this->x = x - size.x / 2;
    this->y = y - size.y / 2;
    this->size = size;
    this->orientation = orientation;
    this->num_digits = numdigits;
    this->num_ranges = numranges;
    this->ranges = ranges;
    this->min = min;
    this->max = max;
    // TODO Parameterize these angles
    //this->font = font;
    // TODO parameterize this or at least make it adjustable
    //this->text_size = (Vector2) {30, 12};
}

void RectGauge::draw() {
    draw_outline();
    //draw_text_value();
    draw_value();
    draw_bounding_box();

}

void RectGauge::draw_value() {
    float value_size_available;
    if (orientation == HORIZONTAL) {
        value_size_available = size.x - OUTLINE_STROKE_WIDTH * 3;
    } else {
        //value_size_available = size.y - OUTLINE_STROKE_WIDTH * 3 - text_size.y;
        value_size_available = size.y - OUTLINE_STROKE_WIDTH * 3;
    }

    float value_normalized = (value - min) / (max - min);

    float value_size = value_normalized * value_size_available;

    Color color = get_color(get_state());
    float width, height;
    if (orientation == HORIZONTAL) {
        width = value_size;
        //height = size.y - OUTLINE_STROKE_WIDTH * 3 - text_size.y;
        height = size.y - OUTLINE_STROKE_WIDTH * 3;
    } else {
        height = value_size;
        width = size.x - OUTLINE_STROKE_WIDTH * 3;
    }
    // +Y is down, so height should be negative.
    //DrawRectangle(x + OUTLINE_STROKE_WIDTH * 1.5f, y + size.y - height - OUTLINE_STROKE_WIDTH * 1.5 - text_size.y, width, height, color);
    DrawRectangle(x + OUTLINE_STROKE_WIDTH * 1.5f, y + size.y - height - OUTLINE_STROKE_WIDTH * 1.5, width, height, color);
}

/*
void RectGauge::draw_text_value() {
    char value_buf[80];
    // TODO protect against buffer overflows!
    sprintf(value_buf, "%*.0f", num_digits, value);

    // TODO make text size such that vertical height = text_size
    float point_size = get_max_text_size(value_buf, font, text_size.x);
    DrawTextExAlign(font, value_buf, (Vector2) {x + size.x, y + size.y - text_size.y}, point_size, 0, WHITE, RIGHT, TOP);
}
*/

void RectGauge::draw_outline() {
    // TODO support horizontal gauges
    //float size_available = size.y - text_size.y - OUTLINE_STROKE_WIDTH;
    float size_available = size.y - OUTLINE_STROKE_WIDTH;
    float x_left = x + OUTLINE_STROKE_WIDTH/2.0f;
    float x_right = x + size.x - OUTLINE_STROKE_WIDTH/2.0f;
    for (int c = 0; c < num_ranges; c++) {
        Color color = get_color(ranges[c].state);
        float min_normalized = (ranges[c].min - min) / (max - min);
        float max_normalized = (ranges[c].max - min) / (max - min);
        float y_min = y + size.y - OUTLINE_STROKE_WIDTH/2.0f - min_normalized * size_available;
        float y_max = y + size.y - OUTLINE_STROKE_WIDTH/2.0f - max_normalized * size_available;
        //float y_min = y + size.y - text_size.y - OUTLINE_STROKE_WIDTH/2.0f - min_normalized * size_available;
        //float y_max = y + size.y - text_size.y - OUTLINE_STROKE_WIDTH/2.0f - max_normalized * size_available;

        DrawLineEx((Vector2) {x_left, y_min}, (Vector2) {x_left, y_max}, OUTLINE_STROKE_WIDTH, color);
        DrawLineEx((Vector2) {x_right, y_min}, (Vector2) {x_right, y_max}, OUTLINE_STROKE_WIDTH, color);
        if (ranges[c].min == min) {
            DrawLineEx((Vector2) {x, y_min}, (Vector2) {x + size.x, y_min}, OUTLINE_STROKE_WIDTH, color);
        } else if (ranges[c].max == max) {
            DrawLineEx((Vector2) {x, y_max}, (Vector2) {x + size.x, y_max}, OUTLINE_STROKE_WIDTH, color);
        }
    };
}

void RectGauge::draw_bounding_box() {
    //DrawRectangleLines(x, y, size.x, size.y, WHITE);
}

