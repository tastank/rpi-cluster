#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <initializer_list>
#include <vector>
#include <string>

#include <VG/openvg.h>
#include <VG/vgu.h>
#include <fontinfo.h>
#include <shapes.h>

#include "RoundGauge.h"
#include "OpenVGHelper.h"

#define OUTLINE_STROKE_WIDTH 8.0f

// TODO support non-linear round_gauges

RoundGauge::RoundGauge(const char *name, int x, int y, int size, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states) {
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
    this->start_angle = 270.0f;
    this->end_angle = 0.0f;
}

void RoundGauge::draw() {
    // TODO Add unit to text?
    /* I like this idea, but it blocks too much stuff. Better to have the whole background flash, probably
    if (get_state() == CRIT && time(NULL) % 2 == 0) {
        Fill(255, 0, 0, 1);
        Stroke(255, 255, 0, 1);
        Rect(x-size/2, y-size/2, size, size);
        Fill(0, 0, 0, 1);
        TextActualMid(x, y, (char *)get_name(), MonoTypeface, get_max_text_size((char *)get_name(), MonoTypeface, size*0.8f));
    } else {
        */
        draw_text_value();
        draw_value();
        draw_outline();
    //}

}

void RoundGauge::draw_value() {

    Angle angle = get_angle(min, get_value());

    VGfloat color[4];
    get_color(get_state(), color);
    setstroke(color);
    // Unfortunately, Arc() fills the area bounded by the outline and a straight line from start to end.
    // This leaves a small gap to the outline and draws the correctly sized wedge
    float value_size = (size - OUTLINE_STROKE_WIDTH * 2.0f - 1.0f) / 2.0f;
    StrokeWidth(value_size);
    ArcOutline(x, y, value_size, value_size, angle.start, angle.extent);

}

void RoundGauge::draw_text_value() {
    char value_buf[80];
    // TODO protect against buffer overflows!
    sprintf(value_buf, "%*.0f", num_digits, value);
    Fill(255, 255, 255, 1);

    float point_size = get_max_text_size(value_buf, MonoTypeface, size/2.1f);
    float text_height = TextHeight(MonoTypeface, point_size);
    Text(x + size/20.0f, y - text_height, value_buf, MonoTypeface, point_size);
}

void RoundGauge::draw_outline() {
    // TODO parameterize this?
    StrokeWidth(OUTLINE_STROKE_WIDTH);
    float outline_size = size - OUTLINE_STROKE_WIDTH / 2.0f; // want the outer limit to be round_gauge.size

    for (int c = 0; c < num_ranges; c++) {
        VGfloat color[4];
        get_color(ranges[c].state, color);
        setstroke(color);
        Angle angle = get_angle(ranges[c].min, ranges[c].max);
        ArcOutline(x, y, outline_size, outline_size, angle.start, angle.extent);
    };
}


Angle RoundGauge::get_angle(float start_value, float end_value) {
    // OpenVG draws angles anti-clockwise, so end_value is used to find the start of the angle
    // and start_value its extent
    float end_value_normalized = (end_value - min) / (max - min);
    float start_value_normalized = (start_value - min) / (max - min);
    // TODO make a transformation function?
    float angle_start = start_angle - start_value_normalized * (start_angle - end_angle);
    float angle_end = start_angle - end_value_normalized * (start_angle - end_angle);

    Angle angle;
    angle.start = angle_end;
    angle.extent = angle_start - angle_end;

    return angle;
}

