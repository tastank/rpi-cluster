
#include <string>
#include <vector>

#include <raylib.h>

#include "Gauge.h"

typedef enum Orientation { HORIZONTAL, VERTICAL } Orientation;

Orientation str_to_orientation(std::string);

class RectGauge : public Gauge {
    Vector2 size;
    Vector2 text_size;
    void draw_outline();
    void draw_value();
    //void draw_text_value();
    void draw_bounding_box();
    //Font font;
    Orientation orientation;

public:
    //RectGauge(const char *name, int x, int y, Vector2 size, Orientation orientation, int numdigits, int numranges, std::initializer_list<float> bounds, std::initializer_list<State> states, Font font);
    RectGauge(std::string name, std::string parameter_name, int x, int y, Vector2 size, Orientation orientation, int numdigits, std::vector<float> bounds, std::vector<State> states);
    void draw();
};

