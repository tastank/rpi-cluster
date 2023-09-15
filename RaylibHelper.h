
#include <raylib.h>

typedef enum HAlign {LEFT, CENTER, RIGHT} HAlign;
typedef enum VAlign {BOTTOM, MIDDLE, TOP} VAlign;


void DrawTextExAlign(Font font, const char *text, Vector2 pos, float fontSize, float spacing, Color color, HAlign h_align, VAlign v_align);
float get_max_text_size(char *str, Font font, float width);

