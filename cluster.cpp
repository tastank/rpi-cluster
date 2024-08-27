#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include <raylib.h>

#include <zmqpp/zmqpp.hpp>

#include "Label.h"
#include "DigitalGauge.h"
#include "RectGauge.h"
#include "RoundGauge.h"
#include "RaylibHelper.h"

// https://github.com/mcmtroffaes/inipp
#include "inipp.h"
// TODO this looks like it might be better https://github.com/SSARCandy/ini-cpp

//#define DEBUG
//#define DEBUG_FPS

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

    inipp::Ini<char> ini;
    std::ifstream is("rpi-cluster.conf");
    ini.parse(is);

    /////////// DEFINE GAUGES /////////////////////////////////////////////
    std::vector<Gauge*> gauges;
    std::map<std::string, float> parameter_values;

    std::string gauge_section;
    int gauge_index = 0;

    while (true) {
        gauge_section = std::string("GAUGE_") + std::to_string(gauge_index);
        if (ini.sections.find(gauge_section) == ini.sections.end()) {
            break;
        }
        // these values are common to all gauges
        std::string type;
        std::string display_name;
        std::string parameter_name;
        float center_x = 0.0f;
        float center_y = 0.0f;
        std::string thresholds_str;
        std::string states_str;

        inipp::get_value(ini.sections[gauge_section], "type", type);
        inipp::get_value(ini.sections[gauge_section], "display_name", display_name);
        inipp::get_value(ini.sections[gauge_section], "parameter_name", parameter_name);
        // initialize the parameter
        parameter_values[parameter_name] = 0.0f;
        inipp::get_value(ini.sections[gauge_section], "center_x", center_x);
        inipp::get_value(ini.sections[gauge_section], "center_y", center_y);
        inipp::get_value(ini.sections[gauge_section], "thresholds", thresholds_str);
        inipp::get_value(ini.sections[gauge_section], "states", states_str);

        std::vector<float> thresholds;
        // TODO use a template function to do this with arbitrary types and not have to repeat the code for the states list.
        //  function signature would be template<typename T> std::vector<T> csv_to_vector(std::string csv)
        if (thresholds_str.size() > 0) {
            // from https://stackoverflow.com/a/10861816
            std::stringstream thresholds_stream(thresholds_str);
            // TODO allow different types
            while (thresholds_stream.good()) {
                std::string substr;
                std::getline(thresholds_stream, substr, ',');
                float threshold = std::stof(substr);
                thresholds.push_back(threshold);
            }
        }
        // TODO this should be a vector of States, not std::strings
        std::vector<State> states;
        if (states_str.size() > 0) {
            // from https://stackoverflow.com/a/10861816
            std::stringstream states_stream(states_str);
            while (states_stream.good()) {
                std::string state_str;
                std::getline(states_stream, state_str, ',');
                State state = str_to_state(state_str);
                states.push_back(state);
            }
        }
        if (type == "round") {
            float size = 0.0f;
            int display_digits = -1;
            int decimal_digits = 0;

            inipp::get_value(ini.sections[gauge_section], "size", size);
            inipp::get_value(ini.sections[gauge_section], "display_digits", display_digits);
            inipp::get_value(ini.sections[gauge_section], "decimal", decimal_digits);

            RoundGauge gauge(
                display_name,
                center_x, center_y,
                size,
                display_digits,
                thresholds,
                states,
                font
            );
            gauges.push_back(&gauge);

        } else if (type == "rect") {
            float size_x = 0.0f;
            float size_y = 0.0f;
            int display_digits;
            std::string orientation_str;
            // TODO display_digits is not actually supported now; add display option to the gauge rather than having to define a separate digital gauge
            inipp::get_value(ini.sections[gauge_section], "size_x", size_x);
            inipp::get_value(ini.sections[gauge_section], "size_y", size_y);
            inipp::get_value(ini.sections[gauge_section], "display_digits", display_digits);
            inipp::get_value(ini.sections[gauge_section], "orientation", orientation_str);

            RectGauge gauge(
                display_name, 
                center_x,
                center_y,
                (Vector2) {size_x, size_y},
                str_to_orientation(orientation_str),
                display_digits,
                thresholds,
                states
            );
            gauges.push_back(&gauge);
        } else if (type == "digital") {
            float size = 0.0f;
            int display_digits = -1;
            int decimal_digits = -1;

            inipp::get_value(ini.sections[gauge_section], "size", size);
            inipp::get_value(ini.sections[gauge_section], "digits", display_digits);
            inipp::get_value(ini.sections[gauge_section], "decimal", decimal_digits);

            DigitalGauge gauge(
                display_name,
                center_x,
                center_y,
                size,
                display_digits,
                decimal_digits,
                thresholds,
                states,
                font
            );
            gauges.push_back(&gauge);
        }

        gauge_index++;
    }

    /////////// DEFINE LABELS /////////////////////////////////////////////
    std::vector<Label*> labels;

    std::string label_section;
    int label_index = 0;

    while (true) {
        label_section = std::string("TEXT_") + std::to_string(gauge_index);
        if (ini.sections.find(label_section) == ini.sections.end()) {
            break;
        }
        std::string text;
        float center_x = 0.0f;
        float center_y = 0.0f;
        float size = 0.0f;

        inipp::get_value(ini.sections[label_section], "text", text);
        inipp::get_value(ini.sections[label_section], "center_x", center_x);
        inipp::get_value(ini.sections[label_section], "center_y", center_y);
        inipp::get_value(ini.sections[label_section], "size", size);

        Label label(text, center_x, center_y, size, font);

        labels.push_back(&label);

        label_index++;
    }





#ifdef DEBUG
    std::cout << "Pushing gauges to vector...\n";
#endif


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

            // special cases
            if (param_name == "FLASH") {
                warn_flash_on = (param_value > 0.9f);
            } else if (param_name == "TIME") {
                // TODO treat this like a string ("H:MM"), not a float.
                // this is a hack to show H.MM
                int hours = (int) (param_value / 60.0f);
                int minutes = (int) param_value % 60;
                param_value = (float) hours + minutes/100.0f;
            }
            if (parameter_values.find(param_name) != parameter_values.end()) {
                parameter_values[param_name] = param_value;
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

        // TODO parameterize this in the conf file
        Vector2 crit_label_pos = {532, 420};
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

#ifdef DEBUG
        std::cout << "Drawing gauges...\n";
#endif
        for (Gauge *gauge : gauges) {
            gauge->draw();
        }
        for (Label *label : labels) {
            label->draw();
        }

#ifdef DEBUG
        std::cout << "Finished drawing.\n";
#endif
        EndDrawing();
    }
    UnloadFont(font);
    CloseWindow();
    return 0;
}

