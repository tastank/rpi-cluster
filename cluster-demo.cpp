#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdexcept>
#include <string>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <fontinfo.h>
#include <shapes.h>

#include <wiringSerial.h>

#include "DigitalGauge.h"
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
    RoundGauge water_press_gauge(
        "WATER PRESS",
        width-oil_press_gauge.x, oil_press_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        {CRIT, WARN, OK, WARN}
    );
    RoundGauge water_temp_gauge(
        "WATER TEMP",
        water_press_gauge.x, oil_temp_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 140.0f, 200.0f, 220.0f, 250.0f},
        {WARN, OK, WARN, CRIT}
    );

    DigitalGauge speedometer(
        "MPH",
        512.0f, 325.0f,
        120.0f,
        1,
        1,
        {0.0f, 120.0f},
        {OK}
    );

    std::vector<Gauge*> gauges;
    gauges.push_back(&tachometer);
    gauges.push_back(&oil_press_gauge);
    gauges.push_back(&oil_temp_gauge);
    gauges.push_back(&water_press_gauge);
    gauges.push_back(&water_temp_gauge);
    gauges.push_back(&speedometer);

    float oil_press = 0.0f;
    float oil_temp = 0.0f;
    float water_press = 0.0f;
    float water_temp = 0.0f;
    float rpm = 0.0f;
    float mph = 100.0f;

    int fd;
    if ((fd=serialOpen("/dev/ttyUSB0", 921600)) < 0) {
        fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
        return 1;
    }

    time_t start_time = time(NULL);
    while (time(NULL) < start_time + 60) {

        while(serialDataAvail(fd)) {
            std::string serial_string, parameter_value_str;
            do {
                serial_string.push_back(serialGetchar(fd));
            } while (serial_string.back() != ':' && serialDataAvail(fd));
            // last character in serial_str is now ':'. The next character, then, is the first of the value for the parameter.
            do {
                parameter_value_str.push_back(serialGetchar(fd));
            } while (parameter_value_str.back() != '\n' && serialDataAvail(fd));
            // last character is now '\n'; delete it to get just the number
            parameter_value_str.pop_back();
            try {
                int parameter_value = std::stoi(parameter_value_str);

                if (serial_string == "RPM:") {
                    rpm = parameter_value;
                } else if (serial_string == "OP10:") {
                    oil_press = parameter_value / 10.0f;
                } else if (serial_string == "OT10:") {
                    oil_temp = parameter_value / 10.0f;
                } else if (serial_string == "WP10:") {
                    water_press = parameter_value / 10.0f;
                } else if (serial_string == "WT10:") {
                    water_temp = parameter_value / 10.0f;
                } else {
                    fprintf(stderr, "Unexpected serial parameter name \"%s\"\n", serial_string);
                }
            } catch (std::invalid_argument e) {
                fprintf(stderr, "An exception occurred: %s; parameter_value_str \"%s\"\n", e.what(), parameter_value_str);
            }
        }

        Start(width, height);                   // Start the picture
        Background(0, 0, 0);                   // Black background

        float crit_label_x = width/2 + 20;
        float crit_label_y = height/2 - 150;
        // set background if any state is CRIT
        bool black_text = false;
        for (Gauge *gauge : gauges) {
            State state = gauge->get_state();
            if (gauge->get_state() == CRIT) {
                if (time(NULL) % 2) {
                    Background(150, 0, 0);
                    black_text = true;
                    break;
                }
            }
        }

        for (Gauge *gauge : gauges) {
            State state = gauge->get_state();
            if (gauge->get_state() == CRIT) {
                if (black_text) {
                    Fill(0, 0, 0, 1);
                }else {
                    VGfloat color[4];
                    gauge->get_color(CRIT, color);
                    setfill(color);
                }
                Text(crit_label_x, crit_label_y, gauge->get_name(), MonoTypeface, 36.0f);
                crit_label_y -= 50.0f;
            }

        }


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
        TextActualMid(water_press_gauge.x, height/2.0f, "WATER", MonoTypeface, MAJOR_LABEL_SIZE);

        TextActualMid(oil_press_gauge.x, oil_press_gauge.y+oil_press_gauge.size/2.0f + 30.0f, "PRESS", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(oil_temp_gauge.x, oil_temp_gauge.y-oil_temp_gauge.size/2.0f - 30.0f, "TEMP", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(water_press_gauge.x, water_press_gauge.y+water_press_gauge.size/2.0f + 30.0f, "PRESS", MonoTypeface, MINOR_LABEL_SIZE);
        TextActualMid(water_temp_gauge.x, water_temp_gauge.y-water_temp_gauge.size/2.0f - 30.0f, "TEMP", MonoTypeface, MINOR_LABEL_SIZE);

        tachometer.set_value(rpm);
        oil_press_gauge.set_value(oil_press);
        oil_temp_gauge.set_value(oil_temp);
        water_press_gauge.set_value(water_press);
        water_temp_gauge.set_value(water_temp);
        speedometer.set_value(mph);

        tachometer.draw();
        oil_press_gauge.draw();
        oil_temp_gauge.draw();
        water_press_gauge.draw();
        water_temp_gauge.draw();
        speedometer.draw();

        End();                           // End the picture
    }

    finish();                       // Graphics cleanup
    serialClose(fd); // close serial connection to the Arduino
    return 0;
}

