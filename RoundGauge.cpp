
#include "RoundGauge.h"

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
#define OUTLINE_STROKE_WIDTH 8.0f

// TODO support non-linear round_gauges

RoundGauge::RoundGauge(const char *name, int x, int y, int size, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states, Font font) {
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
    // TODO Parameterize these angles
    // Raylib measures angles anti-clockwise starting from down and allows negative angles, so this range goes clockwise from 6:00 (min) to 3:00 (max)
    this->start_angle = 0.0f;
    this->end_angle = -270.0f;
    this->font = font;
}

void RoundGauge::draw() {
    // TODO Add unit to text?
    /* I like this idea, but it blocks too much stuff. Better to have the whole background flash, probably
    if (get_state() == CRIT && time(NULL) % 2 == 0) {
        Fill(255, 0, 0, 1);
        Stroke(255, 255, 0, 1);
        Rect(x-size/2, y-size/2, size, size);
        DrawTextExAlign(font, (char *)get_name(), (Vector2) {x, y}, get_max_text_size((char *)get_name(), font, size*0.8f), 0, WHITE, CENTER, MIDDLE);
    } else {
        */
        draw_outline();
        draw_text_value();
        draw_value();
    //}

}

void RoundGauge::draw_value() {
    Angle angle = get_angle(min, get_value());

    Color color = get_color(get_state());
    // size represents a square the gauge should fit in (effectively the diameter of the circle); DrawCircleSector draws by radius
    float sector_size = (size - OUTLINE_STROKE_WIDTH * 3.0f) / 2.0f;
    // TODO use DrawRing to leave a hole in the middle
    DrawRing((Vector2){x, y}, sector_size/2.0f, sector_size, angle.start, angle.end, 360, color);
    //DrawCircleSector((Vector2){x, y}, sector_size, angle.start, angle.end, 0, color);
}

void RoundGauge::draw_text_value() {
    char value_buf[80];
    // TODO protect against buffer overflows!
    sprintf(value_buf, "%*.0f", num_digits, value);

    float point_size = get_max_text_size(value_buf, font, size/2.1f);
    DrawTextExAlign(font, value_buf, (Vector2) {x + size/2, y}, point_size, 0, WHITE, RIGHT, TOP);
}

void RoundGauge::draw_outline() {
    for (int c = 0; c < num_ranges; c++) {
        Color color = get_color(ranges[c].state);
        Angle angle = get_angle(ranges[c].min, ranges[c].max);
        DrawRing((Vector2){x, y}, size/2.0f - OUTLINE_STROKE_WIDTH, size/2.0f, angle.start, angle.end, 360, color);
    };
}


Angle RoundGauge::get_angle(float start_value, float end_value) {
    Angle angle;
    // Raylib draws angles anti-clockwise, so end_value is used to find the start of the angle
    // and start_value its extent
    float end_value_normalized = (end_value - min) / (max - min);
    float start_value_normalized = (start_value - min) / (max - min);
    // TODO make a transformation function?
    angle.start = start_angle + start_value_normalized * (end_angle - start_angle);
    angle.end = start_angle + end_value_normalized * (end_angle - start_angle);

    return angle;
}

