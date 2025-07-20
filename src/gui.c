#include <stdlib.h>
#include "ssd1306.h"
#include "encoder.h"
#include "gui.h"

static void gui_draw(const char *title, const char* const items[], uint8_t count, uint8_t top, uint8_t selected) {
    uint8_t y;

    screen_clear();
    if (title) {
        y = font_charheight(true);
        screen_printstr_x2(title, 1, 0, 1);
        screen_hline(0, y - 1, SCREEN_WIDTH, 1);
    } else
        y = 0;
    while (top < count) {
        if (top == selected)
            screen_fillrect(0, y, font_strwidth(items[top], true) + 2, font_charheight(true), 1);
        screen_printstr_x2(items[top], 1, y, top != selected);
        y += font_charheight(true);
        if (y >= SCREEN_HEIGHT)
            break;
        ++top;
    }
    ssd1306_flush(false);
}

uint8_t gui_choose(const char *title, const char* const items[], uint8_t count) {
    uint8_t result = 0;
    uint8_t top = 0;
    uint8_t e;

    gui_draw(title, items, count, top, result);
    while ((e = Encoder_Read()) != ENC_BTNCLICK) {
        if ((e == ENC_CCW) || (e == ENC_CW)) {
            if (e == ENC_CCW) {
                if (result > 0)
                    --result;
                else
                    result = count - 1;
            } else { // e == ENC_CW
                if (result < count - 1)
                    ++result;
                else
                    result = 0;
            }
            if (result < top)
                top = result;
            else if (result > top + (SCREEN_HEIGHT / font_charheight(true)) - 1 - (uint8_t)(title != NULL))
                top = result - ((SCREEN_HEIGHT / font_charheight(true)) - 1 - (uint8_t)(title != NULL));
            gui_draw(title, items, count, top, result);
        }
    }
    return result;
}
