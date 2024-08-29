
#include "Gauge.h"

#include <ctime>
#include <string>

#include <raylib.h>

State str_to_state(std::string str) {
    if (str == "OK") {
        return OK;
    } else if (str == "WARN") {
        return WARN;
    } else if (str == "CRIT") {
        return CRIT;
    } else {
        return STALE;
    }
}

float Gauge::get_min() {
    return min;
}
float Gauge::get_max() {
    return max;
}

float Gauge::get_value() {
    return value;
}
// TODO add a last updated timestamp to determine whether we're getting updated data
void Gauge::set_value(float value) {
    this->last_updated_timestamp = std::time(NULL);
    this->value = value;
}

std::string Gauge::get_name() {
    return name;
}

std::string Gauge::get_parameter_name() {
    return parameter_name;
}

Color Gauge::get_color(State state) {
    if (state == OK) {
        return (Color) {32, 192, 64, 255};
    } else if (state == WARN) {
        return (Color) {255, 255, 0, 255};
    } else if (state == CRIT) {
        return (Color) {255, 0, 0, 255};
    }
    // default
    return (Color) {0, 255, 255, 255};
}

State Gauge::get_state() {
    if (std::time(NULL) - last_updated_timestamp > DATA_TIMEOUT) {
        // data is stale. There should probably be another state for that. For now, use CRIT
        return STALE;
    }
    State current_state;
    bool state_found = false;
    for (int c = 0; c < this->ranges.size(); c++) {
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

