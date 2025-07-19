#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum { AVG1, AVG4, AVG16, AVG64, AVG128, AVG256, AVG512, AVG1024 } avgmode_t;
typedef enum { US140, US204, US332, US588, US1100, US2116, US4156, US8244 } convtime_t;

uint16_t ina226_maxMilliAmps(uint16_t shuntMilliOhms);
bool ina226_begin(uint16_t shuntMilliOhms);
bool ina226_setup(uint16_t shuntMilliOhms);
bool ina226_ready();
bool ina226_measure(bool continuous, avgmode_t avgMode, convtime_t vbusTime, convtime_t shuntTime);
uint16_t ina226_getMilliVolts();
int32_t ina226_getMicroAmps(uint16_t shuntMilliOhms);
uint32_t ina226_getMicroWatts(uint16_t shuntMilliOhms);
