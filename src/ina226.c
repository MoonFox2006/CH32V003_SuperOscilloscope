#include "twi.h"
#include "ina226.h"

#define PRECISSION

#define INA226_ADDRESS      0x40

#define INA226_CONF_REG     0x00
#define INA226_SHUNTV_REG   0x01
#define INA226_BUSV_REG     0x02
#define INA226_POWER_REG    0x03
#define INA226_CURRENT_REG  0x04
#define INA226_CALIB_REG    0x05
#define INA226_MASK_REG     0x06
#define INA226_ALERT_REG    0x07
#define INA226_MANID_REG    0xFE
#define INA226_DIEID_REG    0xFF

uint16_t ina226_maxMilliAmps(uint16_t shuntMilliOhms) {
    uint16_t result;

    result = 81920UL / shuntMilliOhms;
    if (result >= 1000)
        result = (result / 1000) * 1000;
    else if (result >= 100)
        result = (result / 100) * 100;
    else // ???
        result = (result / 10) * 10;
    return result;
}

#ifdef PRECISSION
static uint32_t ina226_currentLSB(uint16_t shuntMilliOhms) {
    return 1000000ULL * ina226_maxMilliAmps(shuntMilliOhms) / 32768U;
#else
static uint16_t ina226_currentLSB(uint16_t shuntMilliOhms) {
    return 1000UL * ina226_maxMilliAmps(shuntMilliOhms) / 32768U;
#endif
}

static int16_t ina226_read16(uint8_t regAddr) {
    int16_t result = -1;

    if (TWI_Start(INA226_ADDRESS, false) == TWI_OK) {
        if (TWI_Write(regAddr)) {
            TWI_Stop();
            if (TWI_Start(INA226_ADDRESS, true) == TWI_OK) {
                result = TWI_Read(false) << 8;
                result |= TWI_Read(true);
            }
        }
        TWI_Stop();
    }
    return result;
}

static bool ina226_write16(uint8_t regAddr, uint16_t regValue) {
    bool result = false;

    if (TWI_Start(INA226_ADDRESS, false) == TWI_OK) {
        result = TWI_Write(regAddr) && TWI_Write(regValue >> 8) && TWI_Write(regValue & 0xFF);
        TWI_Stop();
    }
    return result;
}

bool ina226_ready() {
    int16_t mask = ina226_read16(INA226_MASK_REG);

    return (mask != -1) && ((mask & 0x08) != 0);
}

bool ina226_setup(uint16_t shuntMilliOhms, uint16_t corrFactor) {
#ifdef PRECISSION
    if (corrFactor != 1000)
        return ina226_write16(INA226_CALIB_REG, 5120000000ULL * corrFactor / ((uint64_t)ina226_currentLSB(shuntMilliOhms) * shuntMilliOhms * 1000));
    else
        return ina226_write16(INA226_CALIB_REG, 5120000000ULL / ((uint64_t)ina226_currentLSB(shuntMilliOhms) * shuntMilliOhms));
#else
    if (corrFactor != 1000)
        return ina226_write16(INA226_CALIB_REG, 5120000UL * corrFactor / ((uint32_t)ina226_currentLSB(shuntMilliOhms) * shuntMilliOhms * 1000));
    else
        return ina226_write16(INA226_CALIB_REG, 5120000UL / ((uint32_t)ina226_currentLSB(shuntMilliOhms) * shuntMilliOhms));
#endif
}

bool ina226_begin(uint16_t shuntMilliOhms, uint16_t corrFactor) {
    return (ina226_read16(INA226_MANID_REG) == 0x5449) &&
        ina226_write16(INA226_CONF_REG, 0x8000) && // Reset INA226
        ina226_setup(shuntMilliOhms, corrFactor) &&
        ina226_write16(INA226_MASK_REG, 0x0400); // CNVR
}

bool ina226_measure(bool continuous, avgmode_t avgMode, convtime_t vbusTime, convtime_t shuntTime) {
    return ina226_write16(INA226_CONF_REG, 0x4003 | ((uint16_t)avgMode << 9) | ((uint16_t)vbusTime << 6) | ((uint16_t)shuntTime << 3) | ((uint16_t)continuous << 2));
}

uint16_t ina226_getMilliVolts() {
    return (uint32_t)ina226_read16(INA226_BUSV_REG) * 125 / 100;
}

int32_t ina226_getMicroAmps(uint16_t shuntMilliOhms) {
#ifdef PRECISSION
    return (int64_t)ina226_read16(INA226_CURRENT_REG) * ina226_currentLSB(shuntMilliOhms) / 1000;
#else
    return (int32_t)ina226_read16(INA226_CURRENT_REG) * ina226_currentLSB(shuntMilliOhms);
#endif
}

uint32_t ina226_getMicroWatts(uint16_t shuntMilliOhms) {
#ifdef PRECISSION
    return (uint64_t)ina226_read16(INA226_POWER_REG) * ina226_currentLSB(shuntMilliOhms) / 40;
#else
    return (uint32_t)ina226_read16(INA226_POWER_REG) * ina226_currentLSB(shuntMilliOhms) * 25;
#endif
}
