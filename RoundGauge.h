
#include <initializer_list>

#include <raylib.h>

#include "Gauge.h"

typedef struct Angle {
    float start, end;
} Angle;

class RoundGauge : public Gauge {
    float start_angle, end_angle;

    void draw_outline();
    void draw_value();
    void draw_text_value();
    Angle get_angle(float start_value, float end_value);
    Font font;

public:
    float size; // TODO Should probably be private
    RoundGauge(const char *name, int x, int y, int size, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states, Font font);
    void draw();
};

