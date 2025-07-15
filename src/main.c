#include <ch32v00x.h>
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "encoder.h"
#include "gui.h"

static const char* const ITEMS[] = {
    "Oscilloscope", "INA226", "Snake game"
};

extern void oscilloscope(void);
extern void power(void);
extern void snake(void);

int main(void) {
    TWI_Init(400000);

    Encoder_Init(); // To prevent phantom button click, initialize before delay

    delay_ms(50);

    if (! ssd1306_begin()) {
        while (1) {}
    }

    while (1) {
        switch (gui_choose("Select mode", ITEMS, ARRAY_SIZE(ITEMS))) {
            case 0:
                oscilloscope();
                break;
            case 1:
                power();
                break;
            case 2:
                snake();
                break;
        }
    }
}
