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

    RoundGauge tachometer = create_round_gauge(
        512.0f, 300.0f,
        400.0f,
        4,
        4,
        (float[]){0.0f, 1500.0f, 5000.0f, 6000.0f, 7500.0f},
        (State[]){WARN, OK, WARN, CRIT}
    );
    RoundGauge oil_press_gauge = create_round_gauge(
        150.0f, 150.0f,
        150.0f,
        3,
        4,
        (float[]){0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        (State[]){CRIT, WARN, OK, WARN}
    );
    RoundGauge oil_temp_gauge = create_round_gauge(
        150.0f, 450.0f,
        150.0f,
        3,
        4,
        (float[]){0.0f, 150.0f, 200.0f, 230.0f, 300.0f},
        (State[]){WARN, OK, WARN, CRIT}
    );
    RoundGauge coolant_press_gauge = create_round_gauge(
        1024.0f-150.0f, 150.0f,
        150.0f,
        3,
        4,
        (float[]){0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        (State[]){CRIT, WARN, OK, WARN}
    );
    RoundGauge coolant_temp_gauge = create_round_gauge(
        1024.0f-150.0f, 450.0f,
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

        draw_round_gauge(tachometer, rpm);
        draw_round_gauge(oil_press_gauge, oil_press);
        draw_round_gauge(oil_temp_gauge, oil_temp);
        draw_round_gauge(coolant_press_gauge, coolant_press);
        draw_round_gauge(coolant_temp_gauge, coolant_temp);

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

