

#include <VG/openvg.h>
#include <VG/vgu.h>
#include <fontinfo.h>
#include <shapes.h>


typedef enum State {OK, WARN, CRIT} State;


typedef struct Angle {
    float start, extent;
} Angle;

typedef struct Range {
    State state;
    float min, max;
} Range;

typedef struct RoundGauge {
    int x, y;
    int size; // Should be diameter?
    int num_ranges;
    int num_digits;
    Range *ranges;
    float min, max;
    float start_angle, end_angle;
} RoundGauge;

RoundGauge create_round_gauge(int x, int y, int size, int numdigits, int numranges, float *bounds, State *states);
void draw_round_gauge(RoundGauge round_gauge, float value);
void draw_value(RoundGauge round_gauge, float value);
void draw_outline(RoundGauge round_gauge);
Angle get_angle(RoundGauge round_gauge, float start_value, float end_value);

