
#include <initializer_list>

#include <VG/openvg.h>
#include <VG/vgu.h>
#include <fontinfo.h>
#include <shapes.h>

#include "Gauge.h"

class DigitalGauge : public Gauge {
    float start_angle, end_angle;
    float size; // TODO Should probably be private

public:
    DigitalGauge(const char *name, int x, int y, int size, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states);
    void draw();
};

