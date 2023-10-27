#include <ctime>
#include <iostream>
#include <string>

#include <raylib.h>

#include <zmqpp/zmqpp.hpp>

#include "DigitalGauge.h"
#include "RectGauge.h"
#include "RoundGauge.h"
#include "RaylibHelper.h"

#define DEBUG
#define DEBUG_FPS

int main() {

#ifdef DEBUG
    std::cout << "Initializing zmqpp...\n";
#endif

    const std::string endpoint = "tcp://*:9961";
    zmqpp::context context;
    zmqpp::socket_type type = zmqpp::socket_type::pull;
    zmqpp::socket socket(context, type);

    socket.bind(endpoint);

    const int SCREEN_WIDTH = 1024;
    const int SCREEN_HEIGHT = 600;

#ifdef DEBUG
    std::cout << "Initializing Raylib window...\n";
#endif
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "");
#ifdef DEBUG
    std::cout << "Finished Raylib initialization.\n";
#endif

#ifdef DEBUG
    std::cout << "Loading font...\n";
#endif
    Font font = LoadFontEx("/home/pi/rpi-cluster/resources/fonts/SimplyMono-Bold.ttf", 144.0f, NULL, 0);
    //Font font = LoadFont("resources/fonts/SimplyMono-Bold.ttf");
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
#ifdef DEBUG
    std::cout << "Setting target FPS...\n";
#endif
    SetTargetFPS(30);
    Color text_color = WHITE;

#ifdef DEBUG
    std::cout << "Defining gauges...\n";
#endif
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
        5,
        {0.0f, 8.0f, 11.0f, 15.0f, 18.0f, 20.0f},
        {CRIT, WARN, OK, WARN, CRIT},
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
        3, 0,
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
        {0.0f, 1.5f, 3.0f, 12.0f},
        {CRIT, WARN, OK}
    );
    DigitalGauge fuel_qty_display(
        "FUEL",
        220, 530,
        60.0f,
        3, 1,
        1,
        {0.0f, 12.0f},
        {OK},
        font
    );
    DigitalGauge voltmeter(
        "VOLTS",
        1024-250, 70,
        100.0f,
        4, 1,
        3,
        {0.0f, 12.0f, 14.4f, 16.0f},
        {CRIT, WARN, OK, CRIT},
        font
    );
#ifdef DEBUG
    std::cout << "Pushing gauges to vector...\n";
#endif


    std::vector<Gauge*> gauges;
    gauges.push_back(&tachometer);
    gauges.push_back(&oil_press_gauge);
    gauges.push_back(&oil_temp_gauge);
    gauges.push_back(&water_press_gauge);
    gauges.push_back(&water_temp_gauge);
    gauges.push_back(&speedometer);
    gauges.push_back(&fuel_qty_gauge);
    gauges.push_back(&voltmeter);

    float oil_press;
    float oil_temp;
    float water_press;
    float water_temp;
    float rpm;
    float mph;
    float fuel_qty;
    float volts;

    bool warn_flash_on = true;

    time_t start_time = std::time(NULL);

    while (!WindowShouldClose()) {
#ifdef DEBUG
        std::cout << "Starting loop.\n";
#endif

        zmqpp::message message;

        int message_count = 0;
        while (socket.receive(message, true)) {
            std::string text;
            message >> text;
            if (text == "STOP") {
                return 0;
            }
#ifdef DEBUG
            std::cout << text << '\n';
#endif
            std::string param_name;
            float param_value;
            int index = text.find(':');
            param_name = text.substr(0, index);
            try {
                param_value = std::stof(text.substr(index+1));
            } catch (std::invalid_argument const& e) {
                std::cout << e.what();
            }
            message_count++;

            if (param_name == "OP") {
                oil_press = param_value;
                oil_press_gauge.set_value(oil_press);
            } else if (param_name == "OT") {
                oil_temp = param_value;
                oil_temp_gauge.set_value(oil_temp);
            } else if (param_name == "WP") {
                water_press = param_value;
                water_press_gauge.set_value(water_press);
            } else if (param_name == "WT") {
                water_temp = param_value;
                water_temp_gauge.set_value(water_temp);
            } else if (param_name == "RPM") {
                rpm = param_value;
                tachometer.set_value(rpm);
            } else if (param_name == "MPH") {
                mph = param_value;
                speedometer.set_value(mph);
            } else if (param_name == "FUEL") {
                fuel_qty = param_value;
                fuel_qty_gauge.set_value(fuel_qty);
                fuel_qty_display.set_value(fuel_qty);
            } else if (param_name == "VOLTS") {
                volts = param_value;
                voltmeter.set_value(volts);
            } else if (param_name == "FLASH") {
                warn_flash_on = (param_value > 0.9f);
            } else {
                std::cout << "Unknown parameter: " << param_name << '\n';
            }

        }
#ifdef DEBUG
        if (message_count == 0) {
            std::cout << "No message received.\n";
        } else {
            std::cout << "Finished reading data.\n";
        }
#endif


        BeginDrawing();
        ClearBackground(BLACK);
#ifdef DEBUG_FPS
        DrawFPS(200, 20);
#endif

        Vector2 crit_label_pos = {tachometer.x + 20, tachometer.y + 120};
        // set background if any state is CRIT
        bool black_text = false;
        for (Gauge *gauge : gauges) {
            State state = gauge->get_state();
            if (gauge->get_state() == CRIT) {
                if (warn_flash_on) {
                    ClearBackground(MAROON);
                    black_text = true;
                    break;
                }
            }
        }

        for (Gauge *gauge : gauges) {
            State state = gauge->get_state();
            if (gauge->get_state() == CRIT || gauge->get_state() == STALE) {
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

#ifdef DEBUG
        std::cout << "Drawing labels...\n";
#endif

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
        // I should probably change this behavior, but to make drawing easier RectGauge changes the x and y values to one of the corners instead of the center. DigitalGauge does not, so use the digital fuel gauge for alignment.
        DrawTextExAlign(font, "FUEL", {fuel_qty_display.x, 70.0f}, MAJOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);
        DrawTextExAlign(font, "VOLTS", {voltmeter.x, voltmeter.y + 30.0f}, MAJOR_LABEL_SIZE, 0, WHITE, CENTER, MIDDLE);


#ifdef DEBUG
        std::cout << "Drawing gauges...\n";
#endif
        tachometer.draw();
        oil_press_gauge.draw();
        oil_temp_gauge.draw();
        water_press_gauge.draw();
        water_temp_gauge.draw();
        speedometer.draw();
        fuel_qty_gauge.draw();
        fuel_qty_display.draw();
        voltmeter.draw();

#ifdef DEBUG
        std::cout << "Finished drawing.\n";
#endif
        EndDrawing();
    }
    UnloadFont(font);
    CloseWindow();
    return 0;
}

