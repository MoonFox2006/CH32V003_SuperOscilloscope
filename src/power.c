#define USE_EEPROM
#define USE_UART

#include <string.h>
#include <stdlib.h>
#include <ch32v00x.h>
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "encoder.h"
#include "ina226.h"
#ifdef USE_EEPROM
#include "eeprom.h"
#endif
#ifdef USE_UART
#include "uart.h"
#endif
#include "strutils.h"
#include "globals.h"

#define DEF_SHUNT   2 // R100

#define MIN_CORR_FACTOR 800 // 0.8
#define MAX_CORR_FACTOR 1200 // 1.2

static const uint16_t SHUNTS[] = {
    10, 50, 100, 150, 200
};

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
    const char PROGRESS[4] = { '-', '\\', '|', '/' };

    uint64_t totalMicroAmps = 0;
    uint32_t totalTime = 0;
    bool configChanged = false;

#ifdef USE_EEPROM
    EEPROM_Init(&vars.power, sizeof(vars.power));
    if ((vars.power.corrFactor < MIN_CORR_FACTOR) || (vars.power.corrFactor > MAX_CORR_FACTOR) || (vars.power.shunt >= ARRAY_SIZE(SHUNTS))) { // Wrong or empty EEPROM
        vars.power.corrFactor = 1000; // 1.0
        vars.power.shunt = DEF_SHUNT;
//        configChanged = true;
    }
#else
    vars.power.corrFactor = 1000; // 1.0
    vars.power.shunt = DEF_SHUNT;
#endif

    if ((! ina226_begin(SHUNTS[vars.power.shunt], vars.power.corrFactor)) || (! ina226_measure(true, AVG64, US8244, US8244))) {
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

#ifdef USE_UART
    uartInit();
#endif

    while (1) {
        uint8_t e = Encoder_Read();

        if (e != ENC_NONE) {
            switch (e) {
                case ENC_BTNCLICK: // Apply shunt and corrFactor and clear totals
                    if (configChanged) {
                        ina226_setup(SHUNTS[vars.power.shunt], vars.power.corrFactor);
#ifdef USE_EEPROM
                        EEPROM_Flush();
#endif
                        configChanged = false;
                    }
                    totalMicroAmps = 0;
                    totalTime = 0;
                    break;
                case ENC_CCW:
                    if (vars.power.shunt > 0)
                        --vars.power.shunt;
                    else
                        vars.power.shunt = ARRAY_SIZE(SHUNTS) - 1;
                    configChanged = true;
                    break;
                case ENC_CW:
                    if (vars.power.shunt < ARRAY_SIZE(SHUNTS) - 1)
                        ++vars.power.shunt;
                    else
                        vars.power.shunt = 0;
                    configChanged = true;
                    break;
                case ENC_BTNCCW:
                    if (vars.power.corrFactor > MIN_CORR_FACTOR) {
                        --vars.power.corrFactor;
                        configChanged = true;
                    }
                    break;
                case ENC_BTNCW:
                    if (vars.power.corrFactor < MAX_CORR_FACTOR) {
                        ++vars.power.corrFactor;
                        configChanged = true;
                    }
                    break;
            }
        }

        if (ina226_ready()) {
            char str[16];
            char *s;
            int32_t microAmps;
            uint32_t microWatts;
            uint16_t milliVolts;

            milliVolts = ina226_getMilliVolts();
            microAmps = ina226_getMicroAmps(SHUNTS[vars.power.shunt]);
            microWatts = ina226_getMicroWatts(SHUNTS[vars.power.shunt]);
            totalMicroAmps += labs(microAmps);
            ++totalTime;

            screen_clear();
            s = str;
            if (configChanged)
                *s++ = '!';
            *s++ = 'R';
            s = u16str(s, SHUNTS[vars.power.shunt], 100);
            *s++ = ' ';
            *s++ = PROGRESS[totalTime & 0x03];
            *s = '\0';
            screen_printstr(str, SCREEN_WIDTH - font_strwidth(str, false), 0, 1);
            s = str;
            *s++ = 'x';
            s = u16str(s, vars.power.corrFactor / 1000, 1);
            *s++ = '.';
            u16str(s, vars.power.corrFactor % 1000, 100);
            screen_printstr(str, SCREEN_WIDTH - font_strwidth(str, false), FONT_HEIGHT, 1);
//            snprintf(str, sizeof(str), "%u.%03u V", milliVolts / 1000, milliVolts % 1000);
            s = u16str(str, milliVolts / 1000, 1);
            *s++ = '.';
            s = u16str(s, milliVolts % 1000, 100);
            *s++ = ' ';
            *s++ = 'V';
            *s = '\0';
            screen_printstr_x2(str, 0, 0, 1);
            normalizeI32(str, microAmps, "A");
            screen_printstr_x2(str, 0, FONT_HEIGHT * 2, 1);
            normalizeU32(str, microWatts, "W");
            screen_printstr_x2(str, 0, FONT_HEIGHT * 4, 1);
            normalizeU32(str, (uint32_t)((totalMicroAmps * 3600000000ULL / 1055232) / totalTime), "A/h");
            screen_printstr_x2(str, 0, FONT_HEIGHT * 6, 1);
            ssd1306_flush(true);

#ifdef USE_UART
            itoa(microAmps, str, 10);
            uartPrint(str);
            uartPrint("\r\n");
#endif
        }
    }
}
