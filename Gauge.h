#include <ctime>
#include <string>
#include <vector>

#include <raylib.h>

#ifndef GAUGE_H
#define GAUGE_H

#define DATA_TIMEOUT 2 // seconds until the data is considered stale and no longer useful. This should probably be defined per-gauge, but this simplifies things for now.
#define BAD_DATA_LINE_THICKNESS 4.0f

typedef enum State {OK, WARN, CRIT, STALE} State;

State str_to_state(std::string);

typedef struct Range {
    State state;
    float min, max;
} Range;


class Gauge {
protected:
    std::time_t last_updated_timestamp = 0;
    int num_ranges;
    int num_digits;
    int num_decimal;
    std::vector<Range> ranges;
    float min, max;
    std::string name;
    std::string parameter_name;
    float value;
    float x, y;
public:
    static Color get_color(State);
    float get_value();
    void set_value(float);
    float get_min();
    float get_max();
    std::string get_name();
    std::string get_parameter_name();
    State get_state();
    virtual void draw() = 0;
};

#endif

