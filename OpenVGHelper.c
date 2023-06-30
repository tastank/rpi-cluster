
#include "OpenVGHelper.h"


void TextActualMid(VGfloat x, VGfloat y, char *s, Fontinfo f, int pointsize) {
    float text_height = TextHeight(f, pointsize);
    TextMid(x, y - text_height/2.0f, s, f, pointsize);
}
