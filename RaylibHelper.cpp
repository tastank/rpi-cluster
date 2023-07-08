
#include "RaylibHelper.h"

#include <raylib.h>

void DrawTextExAlign(Font font, const char *text, Vector2 pos, float size, float spacing, Color color, HAlign h_align, VAlign v_align) {
    Vector2 text_size = MeasureTextEx(font, text, size, 0);
    Vector2 offset;
    if (h_align == LEFT) {
        offset.x = 0;
    } else if (h_align == CENTER) {
        offset.x = -text_size.x / 2.0f;
    } else {
        offset.x = -text_size.x;
    }
    // TODO I don't actually know how text is vertically aligned by default
    if (v_align == TOP) {
        offset.y = 0;
    } else if (v_align == MIDDLE) {
        offset.y = -text_size.y / 2.0f;
    } else {
        offset.y = -text_size.y;
    }
    pos.x += offset.x;
    pos.y += offset.y;
    DrawTextEx(font, text, pos, size, spacing, color);
}

float get_max_text_size(char *str, Font font, float width) {
    float baseline_width = MeasureTextEx(font, str, 1, 0).x;
    return width / baseline_width;
}
