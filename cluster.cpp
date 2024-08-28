#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
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

// std::stof is overloaded and the compiler won't know which definition to use if it is used as a callback, so use this wrapper to help it
float stof(std::string str) {
    return std::stof(str);
}

// it'd be nice to move this and any other potential utility functions to their own source file, but doing that with a generic template will lead to a linker error.
template<typename T> std::vector<T> csv_to_vector(std::string csv, T (*conversion_function)(std::string)) {
    std::vector<T> ret;
    if (csv.size() > 0) {
        // from https://stackoverflow.com/a/10861816
        std::stringstream csv_stream(csv);
        while (csv_stream.good()) {
            std::string substr;
            std::getline(csv_stream, substr, ',');
            T val = conversion_function(substr);
            ret.push_back(val);
        }
    }
    return ret;
}

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

    // TODO add a limit to ensure this loop exits
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

        std::vector<float> thresholds = csv_to_vector<float>(thresholds_str, stof);
        std::vector<State> states = csv_to_vector<State>(states_str, str_to_state);
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
            Vector2 size;
            int display_digits;
            std::string orientation_str;
            // TODO display_digits is not actually supported now; add display option to the gauge rather than having to define a separate digital gauge
            inipp::get_value(ini.sections[gauge_section], "size_x", size.x);
            inipp::get_value(ini.sections[gauge_section], "size_y", size.y);
            inipp::get_value(ini.sections[gauge_section], "display_digits", display_digits);
            inipp::get_value(ini.sections[gauge_section], "orientation", orientation_str);

            RectGauge *gauge = new RectGauge(
                display_name, 
                parameter_name,
                center_x,
                center_y,
                size,
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

    // TODO add a limit to ensure this loop exits
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
        std::map<State, std::vector<Gauge*>> state_to_gauge_map;
        for (Gauge *gauge : gauges) {
            state_to_gauge_map[gauge->get_state()].push_back(gauge);
        }

        if (state_to_gauge_map[CRIT].size() > 0 && warn_flash_on) {
            ClearBackground(MAROON);
            black_text = true;
        }

        // TODO parameterize this in the conf file
        Vector2 crit_label_pos = {532, 420};
        // TODO there's definitely a better way to do this
        std::vector<Gauge*> crit_or_stale_gauges;
        crit_or_stale_gauges.insert(crit_or_stale_gauges.end(), state_to_gauge_map[CRIT].begin(), state_to_gauge_map[CRIT].end());
        crit_or_stale_gauges.insert(crit_or_stale_gauges.end(), state_to_gauge_map[STALE].begin(), state_to_gauge_map[STALE].end());
        for (Gauge *gauge : crit_or_stale_gauges) {
            if (black_text) {
                text_color = BLACK;
            } else {
                text_color = gauge->get_color(CRIT);
            }
            //TODO set font size more intelligently
            DrawTextEx(font, gauge->get_name().c_str(), crit_label_pos, 36.0f, 0, text_color);
            crit_label_pos.y += 40.0f;
        }
        // Print anything in WARN state after anything in CRIT state
        for (Gauge *gauge : state_to_gauge_map[WARN]) {
            DrawTextEx(font, gauge->get_name().c_str(), crit_label_pos, 36.0f, 0, gauge->get_color(WARN));
            crit_label_pos.y += 40.0f;
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

    // deallocate memory
    for (Gauge *gauge : gauges) {
        delete gauge;
    }
    for (Label *label : labels) {
        delete label;
    }

    return 0;
}

