
#include "OpenVGHelper.h"


void TextActualMid(VGfloat x, VGfloat y, char *s, Fontinfo f, int pointsize) {
    float text_height = TextHeight(f, pointsize);
    TextMid(x, y - text_height/2.0f, s, f, pointsize);
}

float get_max_text_size(char *str, Fontinfo f, float width) {
    float baseline_width = TextWidth(str, f, 1.0f);
    return width / baseline_width;
}
