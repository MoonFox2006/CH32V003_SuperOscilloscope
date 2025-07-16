#define USE_UART

#include <string.h>
#include <stdlib.h>
#include <ch32v00x.h>
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "encoder.h"
#include "ina226.h"
#include "strutils.h"
#ifdef USE_UART
#include "uart.h"
#endif

static void normalizeU32(char *str, uint32_t value, const char *suffix) {
    char *s;
    bool milli;

    if (value < 1000000) {
        milli = true;
    } else {
        milli = false;
        value /= 1000;
    }
//    sprintf(str, "%u.%03u %s%s", (uint16_t)(value / 1000), (uint16_t)(value % 1000), prefix, suffix);
    s = u16str(str, value / 1000, 1);
    *s++ = '.';
    s = u16str(s, value % 1000, 100);
    *s++ = ' ';
    if (milli)
        *s++ = 'm';
    strcpy(s, suffix);
}

static void normalizeI32(char *str, int32_t value, const char *suffix) {
    char *s;
    bool milli;

    if (labs(value) < 1000000) {
        milli = true;
    } else {
        milli = false;
        value /= 1000;
    }
//    sprintf(str, "%d.%03u %s%s", (int16_t)(value / 1000), (uint16_t)(labs(value) % 1000), prefix, suffix);
    s = i16str(str, value / 1000, 1);
    *s++ = '.';
    s = u16str(s, labs(value) % 1000, 100);
    *s++ = ' ';
    if (milli)
        *s++ = 'm';
    strcpy(s, suffix);
}

void power(void) {
    if ((! ina226_begin()) || (! ina226_measure(true, AVG64, US8244, US8244))) {
        screen_clear();
        screen_printstr_x2("INA226", 0, 16, 1);
        screen_printstr_x2("not ready!", 0, 32, 1);
        ssd1306_flush(true);
//        while (1) {}
        delay_ms(5000); // 5 sec.
        return;
    } else {
        screen_clear();
        screen_printstr_x2("Wait...", 0, 24, 1);
        ssd1306_flush(true);
    }

    const char PROGRESS[4] = { '-', '\\', '|', '/' };

    uint64_t totalMicroAmps = 0;
    uint32_t totalTime = 0;

#ifdef USE_UART
    uartInit();
#endif

    while (1) {
        if (Encoder_Button()) { // Clear totals
            while (Encoder_Button()) {} // Wait for button release

            totalMicroAmps = 0;
            totalTime = 0;
        }

        if (ina226_ready()) {
            char str[16];
            char *s;
            int32_t microAmps;
            uint32_t microWatts;
            uint16_t milliVolts;

            milliVolts = ina226_getMilliVolts();
            microAmps = ina226_getMicroAmps();
            microWatts = ina226_getMicroWatts();
            totalMicroAmps += labs(microAmps);
            ++totalTime;

            screen_clear();
            screen_printchar(PROGRESS[totalTime & 0x03], SCREEN_WIDTH - FONT_WIDTH, 0, 1);
//            snprintf(str, sizeof(str), "%u.%03u V", milliVolts / 1000, milliVolts % 1000);
            s = u16str(str, milliVolts / 1000, 1);
            *s++ = '.';
            s = u16str(s, milliVolts % 1000, 100);
            *s++ = ' ';
            *s++ = 'V';
            *s = '\0';
            screen_printstr_x2(str, 0, 0, 1);
            normalizeI32(str, microAmps, "A");
            screen_printstr_x2(str, 0, 16, 1);
            normalizeU32(str, microWatts, "W");
            screen_printstr_x2(str, 0, 32, 1);
            normalizeU32(str, (uint32_t)((totalMicroAmps * 3600000000ULL / 1055232) / totalTime), "A/h");
            screen_printstr_x2(str, 0, 48, 1);
            ssd1306_flush(true);

#ifdef USE_UART
            itoa(microAmps, str, 10);
            uartPrint(str);
            uartPrint("\r\n");
#endif
        }
    }
}
