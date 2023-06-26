
#include "Gauge.h"

#include <string>

#include <VG/openvg.h>
#include <shapes.h>

float Gauge::get_min() {
    return min;
}
float Gauge::get_max() {
    return max;
}

float Gauge::get_value() {
    return value;
}
void Gauge::set_value(float value) {
    this->value = value;
}

const char *Gauge::get_name() {
    return name.c_str();
}

void Gauge::get_color(State state, VGfloat color[4]) {
    if (state == OK) {
        RGB(0, 140, 32, color);
    } else if (state == WARN) {
        RGB(200, 200, 0, color);
    } else if (state == CRIT) {
        RGB(255, 0, 0, color);
    } else {
        RGB(0, 255, 255, color);
    }
}

State Gauge::get_state() {
    State current_state;
    bool state_found = false;
    for (int c = 0; c < num_ranges; c++) {
        if (get_value() >= ranges[c].min && get_value() < ranges[c].max) {
            current_state = ranges[c].state;
            state_found = true;
            break;
        }
    }

    if (!state_found) {
        current_state = CRIT;
    }

    return current_state;
}

