#include <float.h>
#include <stdlib.h>

#include <VG/openvg.h>
#include <VG/vgu.h>
#include <fontinfo.h>
#include <shapes.h>

#include "RoundGauge.h"

#define OUTLINE_STROKE_WIDTH 4.0f

// TODO support non-linear round_gauges

//private functions
void _set_stroke_color(State state);

RoundGauge create_round_gauge(int x, int y, int size, int numranges, float *bounds, State *states) {
    float min = FLT_MAX;
    float max = FLT_MIN;
    // TODO free?
    Range *ranges = (Range*)malloc((numranges)*sizeof(Range));
    for (int c = 0; c < numranges; c++) {
        ranges[c].min = bounds[c];
        ranges[c].max = bounds[c+1];
        ranges[c].state = states[c];
        // They should be sorted, but what if they're not? will anything else fail?
        if (bounds[c] > max) max = bounds[c];
        if (bounds[c] < min) min = bounds[c];
    }
    // The above will have missed the last index
    if (bounds[numranges] > max) max = bounds[numranges];
    if (bounds[numranges] < min) min = bounds[numranges];

    RoundGauge round_gauge;
    round_gauge.x = x;
    round_gauge.y = y;
    round_gauge.size = size;
    round_gauge.num_ranges = numranges;
    round_gauge.ranges = ranges;
    round_gauge.min = min;
    round_gauge.max = max;
    // TODO Parameterize these angles
    round_gauge.start_angle = 270.0f;
    round_gauge.end_angle = 0.0f;

    return round_gauge;
}

void draw_round_gauge(RoundGauge round_gauge, float value) {
    // TODO draw value as text as well
    // To do that: add format and unit variables to the struct, use sprintf
    draw_value(round_gauge, value);
    draw_outline(round_gauge);
}

void draw_value(RoundGauge round_gauge, float value) {
    if (value < round_gauge.min) value = round_gauge.min;
    if (value > round_gauge.max) value = round_gauge.max;

    State current_state;
    for (int c = 0; c < round_gauge.num_ranges; c++) {
        if (value >= round_gauge.ranges[c].min && value < round_gauge.ranges[c].max) {
            current_state = round_gauge.ranges[c].state;
            break;
        }
    }

    Angle angle = get_angle(round_gauge, round_gauge.min, value);

    _set_stroke_color(current_state);
    // Unfortunately, Arc() fills the area bounded by the outline and a straight line from start to end.
    // This leaves a small gap to the outline and draws the correctly sized wedge
    float size = (round_gauge.size - OUTLINE_STROKE_WIDTH * 2.0f - 1.0f) / 2.0f;
    StrokeWidth(size);
    ArcOutline(round_gauge.x, round_gauge.y, size, size, angle.start, angle.extent);
}

void draw_outline(RoundGauge round_gauge) {
    // TODO parameterize this?
    StrokeWidth(OUTLINE_STROKE_WIDTH);
    float size = round_gauge.size - OUTLINE_STROKE_WIDTH / 2.0f; // want the outer limit to be round_gauge.size

    for (int c = 0; c < round_gauge.num_ranges; c++) {
        _set_stroke_color(round_gauge.ranges[c].state);
	Angle angle = get_angle(round_gauge, round_gauge.ranges[c].min, round_gauge.ranges[c].max);
        ArcOutline(round_gauge.x, round_gauge.y, size, size, angle.start, angle.extent);
    };
}

void _set_stroke_color(State state) {
    // TODO is it possible to make a look-up for this? I don't think it s with C's data types
    // TODO instead, make State a struct with r, g, and b values
    if (state == CRIT) {
        Stroke(255, 0, 0, 1);
    } else if (state == WARN) {
        Stroke(200, 200, 0, 1);
    } else if (state == OK) {
        Stroke(0, 140, 32, 1);
    }
}

Angle get_angle(RoundGauge round_gauge, float start_value, float end_value) {
    // OpenVG draws angles anti-clockwise, so end_value is used to find the start of the angle
    // and start_value its extent
    float end_value_normalized = (end_value - round_gauge.min) / (round_gauge.max - round_gauge.min);
    float start_value_normalized = (start_value - round_gauge.min) / (round_gauge.max - round_gauge.min);
    // TODO make a transformation function?
    float angle_start = round_gauge.start_angle - start_value_normalized * (round_gauge.start_angle - round_gauge.end_angle);
    float angle_end = round_gauge.start_angle - end_value_normalized * (round_gauge.start_angle - round_gauge.end_angle);

    Angle angle;
    angle.start = angle_end;
    angle.extent = angle_start - angle_end;

    return angle;
}

