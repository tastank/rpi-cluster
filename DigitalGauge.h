
#include "Gauge.h"

#include <initializer_list>

#include <raylib.h>

class DigitalGauge : public Gauge {
    float start_angle, end_angle;
    float size;
    Font font;

public:
    DigitalGauge(const char *name, int x, int y, int size, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states, Font font);
    DigitalGauge(const char *name, int x, int y, int size, int numdigits, int numdecimal, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states, Font font);
    void draw();
};

