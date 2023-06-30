#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <fontinfo.h>
#include <shapes.h>

#include "RoundGauge.h"
#include "OpenVGHelper.h"

int main() {
    int width, height;
    char *s;
    init(&width, &height);                   // Graphics initialization

    RoundGauge tachometer = create_round_gauge(
        512.0f, 300.0f,
        500.0f,
        4,
        4,
        (float[]){0.0f, 1500.0f, 5000.0f, 6000.0f, 7500.0f},
        (State[]){WARN, OK, WARN, CRIT}
    );
    RoundGauge oil_press_gauge = create_round_gauge(
        100.0f, 100.0f,
        150.0f,
        3,
        4,
        (float[]){0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        (State[]){CRIT, WARN, OK, WARN}
    );
    RoundGauge oil_temp_gauge = create_round_gauge(
        // keep everything nice and aligned
        oil_press_gauge.x, height-oil_press_gauge.y,
        150.0f,
        3,
        4,
        (float[]){0.0f, 150.0f, 200.0f, 230.0f, 300.0f},
        (State[]){WARN, OK, WARN, CRIT}
    );
    RoundGauge coolant_press_gauge = create_round_gauge(
        width-oil_press_gauge.x, oil_press_gauge.y,
        150.0f,
        3,
        4,
        (float[]){0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        (State[]){CRIT, WARN, OK, WARN}
    );
    RoundGauge coolant_temp_gauge = create_round_gauge(
        coolant_press_gauge.x, oil_temp_gauge.y,
        150.0f,
        3,
        4,
        (float[]){0.0f, 140.0f, 200.0f, 220.0f, 250.0f},
        (State[]){WARN, OK, WARN, CRIT}
    );


    float oil_press = 0.0f;
    float oil_temp = 0.0f;
    float coolant_press = 0.0f;
    float coolant_temp = 0.0f;
    float rpm = 0.0f;

    float oil_press_inc = 0.2f;
    float oil_temp_inc = 0.5f;
    float coolant_press_inc = 0.2f;
    float coolant_temp_inc = 0.5f;
    float rpm_inc = 10.0f;

    while (1) {
        Start(width, height);                   // Start the picture
        Background(0, 0, 0);                   // Black background

        // TODO remove. For alignment purposes only
        /*
        Stroke(255, 255, 255, 1);
        StrokeWidth(1.0f);
        Line(0, 0, width, height);
        Line(0, height, width, 0);
        Line(width/2.0f, 0, width/2.0f, height);
        Line(0, height/2.0f, width, height/2.0f);
        */

        draw_round_gauge(tachometer, rpm);
        draw_round_gauge(oil_press_gauge, oil_press);
        draw_round_gauge(oil_temp_gauge, oil_temp);
        draw_round_gauge(coolant_press_gauge, coolant_press);
        draw_round_gauge(coolant_temp_gauge, coolant_temp);
        
#define MAJOR_LABEL_SIZE 36.0f
#define MINOR_LABEL_SIZE 18.0f
        Fill(255, 255, 255, 1);
        TextActualMid(oil_press_gauge.x, height/2.0f, "OIL", MonoTypeface, MAJOR_LABEL_SIZE);
        TextActualMid(coolant_press_gauge.x, height/2.0f, "WATER", MonoTypeface, MAJOR_LABEL_SIZE);

        TextActualMid(oil_press_gauge.x, oil_press_gauge.y+oil_press_gauge.size/2.0f + 30.0f, "PRESS", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(oil_temp_gauge.x, oil_temp_gauge.y-oil_temp_gauge.size/2.0f - 30.0f, "TEMP", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(coolant_press_gauge.x, coolant_press_gauge.y+coolant_press_gauge.size/2.0f + 30.0f, "PRESS", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(coolant_temp_gauge.x, coolant_temp_gauge.y-coolant_temp_gauge.size/2.0f - 30.0f, "TEMP", MonoTypeface, MINOR_LABEL_SIZE);

        if (rpm > tachometer.max || rpm < tachometer.min) rpm_inc *= -1.0f;
        rpm += rpm_inc;

        if (oil_press > oil_press_gauge.max || oil_press < oil_press_gauge.min) oil_press_inc *= -1.0f;
        oil_press += oil_press_inc;

        if (oil_temp > oil_temp_gauge.max || oil_temp < oil_temp_gauge.min) oil_temp_inc *= -1.0f;
        oil_temp += oil_temp_inc;

        if (coolant_press > coolant_press_gauge.max || coolant_press < coolant_press_gauge.min) coolant_press_inc *= -1.0f;
        coolant_press += coolant_press_inc;

        if (coolant_temp > coolant_temp_gauge.max || coolant_temp < coolant_temp_gauge.min) coolant_temp_inc *= -1.0f;
        coolant_temp += coolant_temp_inc;

        End();                           // End the picture
    }

    finish();                       // Graphics cleanup
    exit(0);
}

