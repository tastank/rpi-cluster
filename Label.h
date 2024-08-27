#include <string>

#include <raylib.h>

#ifndef LABEL_H
#define LABEL_H

class Label {
protected:
    std::string text;
    float center_x, center_y, size;
    Font font;
public:
    Label(std::string text, float center_x, float center_y, float size, Font font);
    void draw();
};

#endif

