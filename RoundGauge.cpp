
#include "RoundGauge.h"

#include <cfloat>
#include <ctime>
#include <vector>
#include <string>

#include <raylib.h>

#include "RaylibHelper.h"

// TODO parameterize this?
#define OUTLINE_STROKE_WIDTH 8.0f

// TODO support non-linear round_gauges

RoundGauge::RoundGauge(std::string name, std::string parameter_name, int x, int y, int size, int numdigits, std::vector<float> bounds, std::vector<State> states, Font font) {
    this->name = name;
    this->parameter_name = parameter_name;
    std::vector<float> bounds_vector{bounds};
    std::vector<State> states_vector{states};
    float min = FLT_MAX;
    float max = -FLT_MAX;
    std::vector<Range> ranges;
    if (states.size() > 0) {
        for (int c = 0; c < states.size(); c++) {
            Range next_range;
            next_range.min = bounds[c];
            next_range.max = bounds[c+1];
            next_range.state = states_vector[c];
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
    if (get_state() == CRIT && std::time(NULL) % 2 == 0) {
        Fill(255, 0, 0, 1);
        Stroke(255, 255, 0, 1);
        Rect(x-size/2, y-size/2, size, size);
        DrawTextExAlign(font, (char *)get_name(), (Vector2) {x, y}, get_max_text_size((char *)get_name(), font, size*0.8f), 0, WHITE, CENTER, MIDDLE);
    } else {
        */
        if (std::time(NULL) - last_updated_timestamp > DATA_TIMEOUT) {
            // I'm not sure the names here are accurate, but it'll still draw the X I want.
            Vector2 ll = (Vector2) {x - size/2.0f, y - size/2.0f};
            Vector2 ul = (Vector2) {x - size/2.0f, y + size/2.0f};
            Vector2 lr = (Vector2) {x + size/2.0f, y - size/2.0f};
            Vector2 ur = (Vector2) {x + size/2.0f, y + size/2.0f};
            DrawLineEx(ll, ur, BAD_DATA_LINE_THICKNESS, RED);
            DrawLineEx(lr, ul, BAD_DATA_LINE_THICKNESS, RED);

            DrawRectangleLinesEx((Rectangle) {ll.x, ll.y, size, size}, BAD_DATA_LINE_THICKNESS, RED);
        } else {
            draw_outline();
            draw_text_value();
            draw_value();
        }
    //}

}

void RoundGauge::draw_value() {
    Angle angle = get_angle(min, get_value());

    Color color = get_color(get_state());
    // size represents a square the gauge should fit in (effectively the diameter of the circle); DrawCircleSector draws by radius
    float sector_size = (size - OUTLINE_STROKE_WIDTH * 3.0f) / 2.0f;
    // 120 segments per 360 degrees is enough
    int segments = (int)(angle.start - angle.end)/3.0f;
    DrawRing((Vector2){x, y}, sector_size/2.0f, sector_size, angle.start, angle.end, segments, color);
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
    for (int c = 0; c < this->ranges.size(); c++) {
        Color color = get_color(ranges[c].state);
        Angle angle = get_angle(ranges[c].min, ranges[c].max);
        int segments = (int)(angle.start - angle.end)/3.0f;
        DrawRing((Vector2){x, y}, size/2.0f - OUTLINE_STROKE_WIDTH, size/2.0f, angle.start, angle.end, segments, color);
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

