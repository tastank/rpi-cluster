#include <float.h>
#include <stdlib.h>
#include <stdio.h>

#include <ctime>
#include <initializer_list>
#include <vector>
#include <string>

#include <raylib.h>

#include "DigitalGauge.h"

#include "RaylibHelper.h"

DigitalGauge::DigitalGauge(const char *name, int x, int y, int size, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states, Font font) {
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

    this->x = x;
    this->y = y;
    this->size = size;
    this->num_digits = numdigits;
    this->num_ranges = numranges;
    this->ranges = ranges;
    this->min = min;
    this->max = max;
    this->font = font;
}

void DigitalGauge::draw() {
    // TODO Add unit to text?
    char value_buf[80];
    // TODO protect against buffer overflows!
    sprintf(value_buf, "%*.0f", num_digits, value);
    // TODO change color for WARN/CRIT states
    float text_size = get_max_text_size(value_buf, font, size);

    if (std::time(NULL) - last_updated_timestamp > DATA_TIMEOUT) {
        Vector2 ll = (Vector2) {x - size/2.0f, y - text_size/2.0f};
        Vector2 ul = (Vector2) {x - size/2.0f, y + text_size/2.0f};
        Vector2 lr = (Vector2) {x + size/2.0f, y - text_size/2.0f};
        Vector2 ur = (Vector2) {x + size/2.0f, y + text_size/2.0f};

        DrawLineEx(ll, ur, BAD_DATA_LINE_THICKNESS, RED);
        DrawLineEx(lr, ul, BAD_DATA_LINE_THICKNESS, RED);
        DrawRectangleLinesEx((Rectangle) {ll.x, ll.y, size, text_size}, BAD_DATA_LINE_THICKNESS, RED);
    } else {
        Color color = WHITE;
        if (get_state() != OK) {
            Color color = get_color(get_state());
        }

        DrawTextExAlign(font, value_buf, (Vector2){x, y}, text_size, 0, color, CENTER, MIDDLE);
    }
}

