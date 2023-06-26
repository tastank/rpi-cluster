
#include <string>
#include <vector>

#include <VG/openvg.h>

typedef enum State {OK, WARN, CRIT} State;

typedef struct Range {
    State state;
    float min, max;
} Range;


class Gauge {
protected:
    int num_ranges;
    int num_digits;
    std::vector<Range> ranges;
    float min, max;
    std::string name;
    float value;
public:
    static void get_color(State, VGfloat color[4]);
    float x, y;
    float get_value();
    void set_value(float);
    float get_min();
    float get_max();
    const char *get_name();
    State get_state();
    void draw();
};
