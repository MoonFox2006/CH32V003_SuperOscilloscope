#include "strutils.h"

char *u16str(char *s, uint16_t value, uint16_t dec) {
    uint16_t divider;

    for (divider = 10000; divider > dec; divider /= 10) {
        if (value / divider)
            break;
    }
    for (; divider > 0; divider /= 10) {
        *s++ = '0' + value / divider;
        value %= divider;
    }
    *s = '\0';
    return s;
}

char *i16str(char *s, int16_t value, uint16_t dec) {
    uint16_t divider;

    if (value < 0) {
        *s++ = '-';
        value = -value;
    }
    for (divider = 10000; divider > dec; divider /= 10) {
        if (value / divider)
            break;
    }
    for (; divider > 0; divider /= 10) {
        *s++ = '0' + value / divider;
        value %= divider;
    }
    *s = '\0';
    return s;
}
