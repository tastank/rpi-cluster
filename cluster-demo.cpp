#include <cstdio>
#include <cstdlib>
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

    RoundGauge tachometer(
        "RPM",
        512.0f, 300.0f,
        500.0f,
        4,
        4,
        {0.0f, 1500.0f, 5000.0f, 6000.0f, 7500.0f},
        {WARN, OK, WARN, CRIT}
    );
    RoundGauge oil_press_gauge(
        "OIL PRESS",
        100.0f, 100.0f,
        150.0f,
        3,
        4,
        {0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        {CRIT, WARN, OK, WARN}
    );
    RoundGauge oil_temp_gauge(
        "OIL TEMP",
        // keep everything nice and aligned
        oil_press_gauge.x, height-oil_press_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 150.0f, 200.0f, 230.0f, 300.0f},
        {WARN, OK, WARN, CRIT}
    );
    RoundGauge coolant_press_gauge(
        "WATER PRESS",
        width-oil_press_gauge.x, oil_press_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        {CRIT, WARN, OK, WARN}
    );
    RoundGauge coolant_temp_gauge(
        "WATER TEMP",
        coolant_press_gauge.x, oil_temp_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 140.0f, 200.0f, 220.0f, 250.0f},
        {WARN, OK, WARN, CRIT}
    );

    std::vector<Gauge*> gauges;
    gauges.push_back(&tachometer);
    gauges.push_back(&oil_press_gauge);
    gauges.push_back(&oil_temp_gauge);
    gauges.push_back(&coolant_press_gauge);
    gauges.push_back(&coolant_temp_gauge);

    float oil_press = 0.0f;
    float oil_temp = 0.0f;
    float coolant_press = 0.0f;
    float coolant_temp = 0.0f;
    float rpm = 0.0f;

    float oil_press_inc = 0.1f;
    float oil_temp_inc = 0.3f;
    float coolant_press_inc = 0.13f;
    float coolant_temp_inc = 0.23f;
    float rpm_inc = 15.0f;

    while (1) {
        Start(width, height);                   // Start the picture

        float crit_label_x = width/2 + 20;
        float crit_label_y = height/2 - 150;
        Fill(255, 0, 0, 1);
        for (int c = 0; c < gauges.size(); c++) {
            State state = gauges[c]->get_state();
            if (gauges[c]->get_state() == CRIT) {
                Text(crit_label_x, crit_label_y, gauges[c]->get_name(), MonoTypeface, 36.0f);
                crit_label_y -= 50.0f;
            }

        }

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

        // Labels
#define MAJOR_LABEL_SIZE 36.0f
#define MINOR_LABEL_SIZE 18.0f
        Fill(255, 255, 255, 1);
        TextActualMid(oil_press_gauge.x, height/2.0f, "OIL", MonoTypeface, MAJOR_LABEL_SIZE);
        TextActualMid(coolant_press_gauge.x, height/2.0f, "WATER", MonoTypeface, MAJOR_LABEL_SIZE);

        TextActualMid(oil_press_gauge.x, oil_press_gauge.y+oil_press_gauge.size/2.0f + 30.0f, "PRESS", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(oil_temp_gauge.x, oil_temp_gauge.y-oil_temp_gauge.size/2.0f - 30.0f, "TEMP", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(coolant_press_gauge.x, coolant_press_gauge.y+coolant_press_gauge.size/2.0f + 30.0f, "PRESS", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(coolant_temp_gauge.x, coolant_temp_gauge.y-coolant_temp_gauge.size/2.0f - 30.0f, "TEMP", MonoTypeface, MINOR_LABEL_SIZE);

        tachometer.set_value(rpm);
        oil_press_gauge.set_value(oil_press);
        oil_temp_gauge.set_value(oil_temp);
        coolant_press_gauge.set_value(coolant_press);
        coolant_temp_gauge.set_value(coolant_temp);

        tachometer.draw();
        oil_press_gauge.draw();
        oil_temp_gauge.draw();
        coolant_press_gauge.draw();
        coolant_temp_gauge.draw();

        

        if (rpm > tachometer.get_max() || rpm < tachometer.get_min()) rpm_inc *= -1.0f;
        rpm += rpm_inc;

        if (oil_press > oil_press_gauge.get_max() || oil_press < oil_press_gauge.get_min()) oil_press_inc *= -1.0f;
        oil_press += oil_press_inc;

        if (oil_temp > oil_temp_gauge.get_max() || oil_temp < oil_temp_gauge.get_min()) oil_temp_inc *= -1.0f;
        oil_temp += oil_temp_inc;

        if (coolant_press > coolant_press_gauge.get_max() || coolant_press < coolant_press_gauge.get_min()) coolant_press_inc *= -1.0f;
        coolant_press += coolant_press_inc;

        if (coolant_temp > coolant_temp_gauge.get_max() || coolant_temp < coolant_temp_gauge.get_min()) coolant_temp_inc *= -1.0f;
        coolant_temp += coolant_temp_inc;

        End();                           // End the picture
    }

    finish();                       // Graphics cleanup
    exit(0);
}

