#include <ch32v00x.h>

#include "encoder.h"
#include "gui.h"
#include "ssd1306.h"
#include "twi.h"
#include "utils.h"

static const char* const ITEMS[] = {"Oscilloscope", "Voltage", "Snake game",
                                    "INA226", "Brick Breaker"};

extern void oscilloscope(void);
extern void voltage(void);
extern void snake(void);
extern void power(void);
extern void brick_breaker(void);

int main(void) {
    TWI_Init(400000);

    Encoder_Init();  // To prevent phantom button click, initialize before
                     // delay

    delay_ms(50);

    if (!ssd1306_begin()) {
        while (1);
    }

    while (1) {
        switch (gui_choose("Select mode", ITEMS, ARRAY_SIZE(ITEMS))) {
            case 0:
                oscilloscope();
                break;
            case 1:
                voltage();
                break;
            case 2:
                snake();
                break;
            case 3:
                power();
                break;
            case 4:
                brick_breaker();
                break;
        }
    }
}
