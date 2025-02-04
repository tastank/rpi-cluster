
#include "DigitalGauge.h"

#include <cfloat>
#include <ctime>
#include <vector>
#include <string>

#include <raylib.h>

#include "RaylibHelper.h"

DigitalGauge::DigitalGauge(std::string name, std::string parameter_name, int x, int y, int size, int numdigits, std::vector<float> bounds, std::vector<State> states, Font font) {
    DigitalGauge(name, parameter_name, x, y, size, numdigits, 0, bounds, states, font);
}

DigitalGauge::DigitalGauge(std::string name, std::string parameter_name, int x, int y, int size, int numdigits, int numdecimal, std::vector<float> bounds, std::vector<State> states, Font font) {
    this->name = name;
    this->parameter_name = parameter_name;
    float min = FLT_MAX;
    float max = -FLT_MAX;
    std::vector<Range> ranges;
    if (states.size() > 0) {
        for (unsigned long c = 0; c < states.size(); c++) {
            Range next_range;
            next_range.min = bounds[c];
            next_range.max = bounds[c+1];
            next_range.state = states[c];
            // They should be sorted, but what if they're not? will anything else fail?
            if (bounds[c] > max) max = bounds[c];
            if (bounds[c] < min) min = bounds[c];
            ranges.push_back(next_range);
        }
        // The above will have missed the last index
        if (bounds[states.size()] > max) max = bounds[states.size()];
        if (bounds[states.size()] < min) min = bounds[states.size()];
    } else {
        Range range;
        range.min = -FLT_MAX;
        range.max = FLT_MAX;
        range.state = OK;
        ranges.push_back(range);
    }

    this->x = x;
    this->y = y;
    this->size = size;
    this->num_digits = numdigits;
    this->num_decimal = numdecimal;
    this->ranges = ranges;
    this->min = min;
    this->max = max;
    this->font = font;
}

void DigitalGauge::draw() {
    // TODO Add unit to text?
    char value_buf[80];
    // TODO protect against buffer overflows!
    sprintf(value_buf, "%*.*f", num_digits, num_decimal, value);
    // TODO change color for WARN/CRIT states
    float text_size = get_max_text_size(value_buf, font, size);

    if (std::time(NULL) - last_updated_timestamp > DATA_TIMEOUT) {
        Vector2 ll = {x - size/2.0f, y - text_size/2.0f};
        Vector2 ul = {x - size/2.0f, y + text_size/2.0f};
        Vector2 lr = {x + size/2.0f, y - text_size/2.0f};
        Vector2 ur = {x + size/2.0f, y + text_size/2.0f};

        DrawLineEx(ll, ur, BAD_DATA_LINE_THICKNESS, RED);
        DrawLineEx(lr, ul, BAD_DATA_LINE_THICKNESS, RED);
        DrawRectangleLinesEx({ll.x, ll.y, size, text_size}, BAD_DATA_LINE_THICKNESS, RED);
    } else {
        Color color = WHITE;
        if (get_state() != OK) {
            color = get_color(get_state());
        }

        DrawTextExAlign(font, value_buf, {x, y}, text_size, 0, color, CENTER, MIDDLE);
    }
}

