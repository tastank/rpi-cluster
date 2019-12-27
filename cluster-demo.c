#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <fontinfo.h>
#include <shapes.h>

#include "RoundGauge.h"


int main() {
    int width, height;
    char *s;
    init(&width, &height);                   // Graphics initialization

    RoundGauge *round_gauges;
#define NUM_STATES 4
#define NUM_ROUND_GAUGES 5
    float x[NUM_ROUND_GAUGES] = {80.0f, 80.0f, 160.0f, 240.0f, 240.0f};
    float y[NUM_ROUND_GAUGES] = {60.0f, 180.0f, 120.0f, 60.0f, 180.0f};
    float size[NUM_ROUND_GAUGES] = {60.0f, 60.0f, 100.0f, 60.0f, 60.0f};
    // would want to make this different for every one, but this is a demo
    float bounds[NUM_STATES + 1] = {0.0f, 30.0f, 70.0f, 80.0f, 100.0f};
    State states[NUM_STATES] = {WARN, OK, WARN, CRIT};
    round_gauges = (RoundGauge*)malloc(NUM_ROUND_GAUGES * sizeof(RoundGauge));
    for (int c = 0; c < NUM_ROUND_GAUGES; c++) {
        round_gauges[c] = create_round_gauge(x[c], y[c], size[c], 4, bounds, states);
    }
    float value = 0.0f;
    float increment = 1.0f;

    while (1) {
        Start(width, height);                   // Start the picture
        Background(0, 0, 0);                   // Black background

        for (int c = 0; c < NUM_ROUND_GAUGES; c++) {
            draw_round_gauge(round_gauges[c], value);
            if (value > round_gauges[c].max) increment *= -1.0f;
            if (value < round_gauges[c].min) increment *= -1.0f;
        }
        value += increment;

        End();                           // End the picture
    }

    finish();                       // Graphics cleanup
    exit(0);
}

