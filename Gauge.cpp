
#include "Gauge.h"

#include <string>

#include <raylib.h>

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

Color Gauge::get_color(State state) {
    if (state == OK) {
        return (Color) {0, 140, 32, 255};
    } else if (state == WARN) {
        return (Color) {200, 200, 0, 255};
    } else if (state == CRIT) {
        return (Color) {255, 0, 0, 255};
    }
    // default
    return (Color) {0, 255, 255, 255};
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

