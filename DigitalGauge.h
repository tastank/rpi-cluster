
#include "Gauge.h"

#include <string>
#include <vector>

#include <raylib.h>

class DigitalGauge : public Gauge {
    float start_angle, end_angle;
    float size;
    Font font;

public:
    DigitalGauge(std::string name, std::string parameter_name, int x, int y, int size, int numdigits, std::vector<float> bounds, std::vector<State> states, Font font);
    DigitalGauge(std::string name, std::string parameter_name, int x, int y, int size, int numdigits, int numdecimal, std::vector<float> bounds, std::vector<State> states, Font font);
    void draw();
};

