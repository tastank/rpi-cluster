
#include "Label.h"

#include "RaylibHelper.h"

#include <string>

#include <raylib.h>

Label::Label(std::string text, float center_x, float center_y, float size, Font font) {
    this->text = text;
    this->center_x = center_x;
    this->center_y = center_y;
    this->size = size;
    this->font = font;
}

void Label::draw() {
    DrawTextExAlign(font, text, {center_x, center_y}, size, 0, WHITE, CENTER, MIDDLE);
}

