#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdexcept>
#include <string>

#include <raylib.h>

#include <wiringSerial.h>

#include "DigitalGauge.h"
#include "RectGauge.h"
#include "RoundGauge.h"
#include "RaylibHelper.h"

#define DEBUG
#define TEST

int main() {

    const int SCREEN_WIDTH = 1024;
    const int SCREEN_HEIGHT = 600;

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "");

    Font font = LoadFontEx("/home/pi/rpi-cluster/resources/fonts/SimplyMono-Bold.ttf", 144.0f, NULL, 0);
    //Font font = LoadFont("resources/fonts/SimplyMono-Bold.ttf");
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    SetTargetFPS(60);
    Color text_color = WHITE;

    RoundGauge tachometer(
        "RPM",
        512.0f, 300.0f,
        500.0f,
        4,
        4,
        {0.0f, 1500.0f, 5000.0f, 6000.0f, 7500.0f},
        {WARN, OK, WARN, CRIT},
        font
    );
    RoundGauge oil_press_gauge(
        "OIL PRESS",
        100.0f, 100.0f,
        150.0f,
        3,
        4,
        {0.0f, 10.0f, 20.0f, 80.0f, 100.0f},
        {CRIT, WARN, OK, WARN},
        font
    );
    RoundGauge oil_temp_gauge(
        "OIL TEMP",
        // keep everything nice and aligned
        oil_press_gauge.x, SCREEN_HEIGHT-oil_press_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 200.0f, 240.0f, 260.0f, 330.0f},
        {WARN, OK, WARN, CRIT},
        font
    );
    RoundGauge water_press_gauge(
        "WATER PRESS",
        SCREEN_WIDTH-oil_press_gauge.x, oil_press_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 8.0f, 14.0f, 18.0f, 24.0f},
        {CRIT, WARN, OK, WARN},
        font
    );
    RoundGauge water_temp_gauge(
        "WATER TEMP",
        water_press_gauge.x, oil_temp_gauge.y,
        150.0f,
        3,
        4,
        {0.0f, 140.0f, 200.0f, 220.0f, 250.0f},
        {WARN, OK, WARN, CRIT},
        font
    );

    DigitalGauge speedometer(
        "MPH",
        512.0f, 275.0f,
        120.0f,
        3,
        1,
        {0.0f, 120.0f},
        {OK},
        font

    );
    RectGauge fuel_qty_gauge(
        "FUEL",
        220, 300,
        (Vector2) {60.0f, 400.0f},
        VERTICAL,
        3,
        3,
        {0.0f, 0.15f, 0.3f, 1.0f},
        {CRIT, WARN, OK}
    );


    std::vector<Gauge*> gauges;
    gauges.push_back(&tachometer);
    gauges.push_back(&oil_press_gauge);
    gauges.push_back(&oil_temp_gauge);
    gauges.push_back(&water_press_gauge);
    gauges.push_back(&water_temp_gauge);
    gauges.push_back(&speedometer);
    gauges.push_back(&fuel_qty_gauge);

    float oil_press = 30.0f;
    float oil_temp = 100.0f;
    float water_press = 10.0f;
    float water_temp = 100.0f;
    float rpm = 0.0f;
    float mph = 0.0f;
    float fuel_qty = 1.0f;
#ifdef TEST
    float oil_press_inc = 0.1f;
    float oil_temp_inc = 0.03f;
    float water_press_inc = 0.01f;
    float water_temp_inc = 0.03f;
    float rpm_inc = 1.0f;
    float mph_inc = 0.3f;
    float fuel_qty_inc = -0.0003f;
#else
    int fd;
    if ((fd=serialOpen("/dev/ttyUSB0", 921600)) < 0) {
        fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
        //return 1;
    }
#endif

    time_t start_time = time(NULL);
    while (!WindowShouldClose() && time(NULL) < start_time + 60) {

#ifdef TEST
        rpm += rpm_inc;
        oil_press += oil_press_inc;
        oil_temp += oil_temp_inc;
        water_press += water_press_inc;
        water_temp += water_temp_inc;
        mph += mph_inc;
        fuel_qty += fuel_qty_inc;

        if (rpm >= tachometer.get_max() || rpm <= tachometer.get_min()) {
            rpm_inc *= -1;
        }
        if (oil_press >= oil_press_gauge.get_max() || oil_press <= oil_press_gauge.get_min()) {
            oil_press_inc *= -1;
        }
        if (oil_temp >= oil_temp_gauge.get_max() || oil_temp <= oil_temp_gauge.get_min()) {
            oil_temp_inc *= -1;
        }
        if (water_temp >= water_temp_gauge.get_max() || water_temp <= water_temp_gauge.get_min()) {
            water_temp_inc *= -1;
        }
        if (water_press >= water_press_gauge.get_max() || water_press <= water_press_gauge.get_min()) {
            water_press_inc *= -1;
        }
        if (mph >= speedometer.get_max() || mph <= speedometer.get_min()) {
            mph_inc *= -1;
        }
        if (fuel_qty >= fuel_qty_gauge.get_max() || fuel_qty <= fuel_qty_gauge.get_min()) {
            fuel_qty_inc *= -1;
        }
#else
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
#endif

        BeginDrawing();
        ClearBackground(BLACK);
#ifdef DEBUG
        DrawFPS(200, 20);
#endif

        Vector2 crit_label_pos = {tachometer.x + 20, tachometer.y + 120};
        // set background if any state is CRIT
        bool black_text = false;
        for (Gauge *gauge : gauges) {
            State state = gauge->get_state();
            if (gauge->get_state() == CRIT) {
                if (time(NULL) % 2) {
                    ClearBackground(MAROON);
                    black_text = true;
                    break;
                }
            }
        }

        for (Gauge *gauge : gauges) {
            State state = gauge->get_state();
            if (gauge->get_state() == CRIT) {
                if (black_text) {
                    text_color = BLACK;
                }else {
                    text_color = gauge->get_color(CRIT);
                }
                //TODO set font size more intelligently
                DrawTextEx(font, gauge->get_name(), crit_label_pos, 36.0f, 0, text_color);
                crit_label_pos.y += 40.0f;
            }

        }


        // TODO remove. For alignment purposes only
        /*
        Stroke(255, 255, 255, 1);
        StrokeWidth(1.0f);
        Line(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        Line(0, SCREEN_HEIGHT, SCREEN_WIDTH, 0);
        Line(SCREEN_WIDTH/2.0f, 0, SCREEN_WIDTH/2.0f, SCREEN_HEIGHT);
        Line(0, SCREEN_HEIGHT/2.0f, SCREEN_WIDTH, SCREEN_HEIGHT/2.0f);
        */

        // Labels
#define MAJOR_LABEL_SIZE 36.0f
#define MINOR_LABEL_SIZE 18.0f

        DrawTextExAlign(font, "OIL", {oil_press_gauge.x, SCREEN_HEIGHT/2}, MAJOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);
        DrawPixel(oil_press_gauge.x, SCREEN_HEIGHT/2, MAGENTA);
        DrawTextExAlign(font, "WATER", {water_press_gauge.x, SCREEN_HEIGHT/2}, MAJOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);
        DrawTextExAlign(font, "PRESS", {oil_press_gauge.x, oil_press_gauge.y+oil_press_gauge.size/2+30}, MINOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);
        DrawTextExAlign(font, "TEMP", {oil_press_gauge.x, oil_temp_gauge.y-oil_temp_gauge.size/2-30}, MINOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);
        DrawTextExAlign(font, "PRESS", {water_press_gauge.x, water_press_gauge.y+water_press_gauge.size/2+30}, MINOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);
        DrawTextExAlign(font, "TEMP", {water_press_gauge.x, water_temp_gauge.y-water_temp_gauge.size/2-30}, MINOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);

        tachometer.set_value(rpm);
        oil_press_gauge.set_value(oil_press);
        oil_temp_gauge.set_value(oil_temp);
        water_press_gauge.set_value(water_press);
        water_temp_gauge.set_value(water_temp);
        speedometer.set_value(mph);
        fuel_qty_gauge.set_value(fuel_qty);

        tachometer.draw();
        oil_press_gauge.draw();
        oil_temp_gauge.draw();
        water_press_gauge.draw();
        water_temp_gauge.draw();
        speedometer.draw();
        fuel_qty_gauge.draw();

        EndDrawing();
    }
#ifndef TEST
    serialClose(fd); // close serial connection to the Arduino
#endif
    UnloadFont(font);
    CloseWindow();
    return 0;
}

