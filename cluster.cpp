#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include <raylib.h>

#include <zmqpp/zmqpp.hpp>

#include "logging.h"
#include "Label.h"
#include "DigitalGauge.h"
#include "RectGauge.h"
#include "RoundGauge.h"
#include "RaylibHelper.h"

// https://github.com/mcmtroffaes/inipp
#include "inipp.h"
// TODO this looks like it might be better https://github.com/SSARCandy/ini-cpp

//#define DEBUG_CLUSTER
//#define DEBUG_CLUSTER_FPS

int main() {

    print_debug("Initializing zmqpp...\n");

    const std::string endpoint = "tcp://*:9961";
    zmqpp::context context;
    zmqpp::socket_type type = zmqpp::socket_type::pull;
    zmqpp::socket socket(context, type);

    socket.bind(endpoint);

    const int SCREEN_WIDTH = 1024;
    const int SCREEN_HEIGHT = 600;

    print_debug("Initializing Raylib window...\n");

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "");

    print_debug("Finished Raylib initialization.\n");


    print_debug("Loading font...\n");
    Font font = LoadFontEx("/home/pi/rpi-cluster/resources/fonts/SimplyMono-Bold.ttf", 144.0f, NULL, 0);
    //Font font = LoadFont("resources/fonts/SimplyMono-Bold.ttf");
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    print_debug("Setting target FPS...\n");
    SetTargetFPS(30);
    Color text_color = WHITE;

    print_debug("Defining gauges...\n");

    inipp::Ini<char> ini;
    std::ifstream is("/home/pi/rpi-cluster/rpi-cluster.conf");
    ini.parse(is);

    /////////// DEFINE GAUGES /////////////////////////////////////////////
    std::vector<Gauge*> gauges;

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

            RoundGauge *gauge = new RoundGauge(
                display_name,
                parameter_name,
                center_x, center_y,
                size,
                display_digits,
                thresholds,
                states,
                font
            );
            gauges.push_back(gauge);

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

            RectGauge *gauge = new RectGauge(
                display_name, 
                parameter_name,
                center_x,
                center_y,
                (Vector2) {size_x, size_y},
                str_to_orientation(orientation_str),
                display_digits,
                thresholds,
                states
            );
            gauges.push_back(gauge);

        } else if (type == "digital") {
            float size = 0.0f;
            int display_digits = 0;
            int decimal_digits = 0;

            inipp::get_value(ini.sections[gauge_section], "size", size);
            inipp::get_value(ini.sections[gauge_section], "digits", display_digits);
            inipp::get_value(ini.sections[gauge_section], "decimal", decimal_digits);

            DigitalGauge *gauge = new DigitalGauge(
                display_name,
                parameter_name,
                center_x,
                center_y,
                size,
                display_digits,
                decimal_digits,
                thresholds,
                states,
                font
            );
            gauges.push_back(gauge);

        }

        gauge_index++;
    }

    /////////// DEFINE LABELS /////////////////////////////////////////////
    std::vector<Label*> labels;

    std::string label_section;
    int label_index = 0;

    while (true) {
        label_section = std::string("TEXT_") + std::to_string(label_index);
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

        Label *label = new Label(text, center_x, center_y, size, font);

        labels.push_back(label);

        label_index++;
    }

    bool warn_flash_on = true;

    time_t start_time = std::time(NULL);

    while (!WindowShouldClose()) {
        print_debug("Starting loop.\n");

        zmqpp::message message;

        // TODO initializing this here prevents recognition of unrecognized values based on their absence in the map; either store a list of expected values when the gauges are initialized from the conf file or initialize the map and use a different algorithm to draw them so stale data can be recognized.
        std::map<std::string, float> parameter_values;
        int message_count = 0;
        while (socket.receive(message, true)) {
            std::string text;
            message >> text;
            if (text == "STOP") {
                return 0;
            }
            print_debug(text);
            print_debug("\n");

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
            // see above comment about the initialization of parameter_values
            // if (parameter_values.find(param_name) != parameter_values.end()) {
                parameter_values[param_name] = param_value;
            //} else {
            //    print_debug("Unknown parameter: ");
            //    print_debug(param_name);
            //    print_debug("\n");
            //}

        }
        if (message_count == 0) {
            print_debug("No message received.\n");
        } else {
            print_debug("Finished reading data.\n");
        }


        BeginDrawing();
        ClearBackground(BLACK);
#ifdef DEBUG_CLUSTER_FPS
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
                DrawTextEx(font, gauge->get_name().c_str(), crit_label_pos, 36.0f, 0, text_color);
                crit_label_pos.y += 40.0f;
            }
        }
        // Print anything in WARN state after anything in CRIT state
        for (Gauge *gauge : gauges) {
            State state = gauge->get_state();
            if (gauge->get_state() == WARN) {
                DrawTextEx(font, gauge->get_name().c_str(), crit_label_pos, 36.0f, 0, gauge->get_color(WARN));
                crit_label_pos.y += 40.0f;
            }
        }

        print_debug("Drawing labels...\n");
        for (Label *label : labels) {
            label->draw();
        }

        print_debug("Drawing gauges...\n");
        for (Gauge *gauge : gauges) {
            if (parameter_values.find(gauge->get_parameter_name()) != parameter_values.end()) {
                gauge->set_value(parameter_values[gauge->get_parameter_name()]);
            }
            gauge->draw();
        }

        EndDrawing();
    }
    UnloadFont(font);
    CloseWindow();
    return 0;
}

