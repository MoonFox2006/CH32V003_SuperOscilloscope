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

#define MAXMILLIAMPS        500U // 0.5 A
#define SHUNTMILLIOHMS      100U // R100
#ifdef PRECISSION
#define CURRENT_LSB         (1000000UL * MAXMILLIAMPS / 32768U)
#else
#define CURRENT_LSB         (1000UL * MAXMILLIAMPS / 32768U)
#endif

//#define CORR_FACTOR         980U // 0.98

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

bool ina226_begin() {
    return (ina226_read16(INA226_MANID_REG) == 0x5449) &&
        ina226_write16(INA226_CONF_REG, 0x8000) && // Reset INA226
#ifdef PRECISSION
#ifdef CORR_FACTOR
        ina226_write16(INA226_CALIB_REG, 5120000000ULL * CORR_FACTOR / ((uint64_t)CURRENT_LSB * SHUNTMILLIOHMS * 1000)) &&
#else
        ina226_write16(INA226_CALIB_REG, 5120000000ULL / ((uint64_t)CURRENT_LSB * SHUNTMILLIOHMS)) &&
#endif
#else
#ifdef CORR_FACTOR
        ina226_write16(INA226_CALIB_REG, 5120000UL * CORR_FACTOR / ((uint32_t)CURRENT_LSB * SHUNTMILLIOHMS * 1000)) &&
#else
        ina226_write16(INA226_CALIB_REG, 5120000UL / ((uint32_t)CURRENT_LSB * SHUNTMILLIOHMS)) &&
#endif
#endif
        ina226_write16(INA226_MASK_REG, 0x0400); // CNVR
}

bool ina226_measure(bool continuous, avgmode_t avgMode, convtime_t vbusTime, convtime_t shuntTime) {
    return ina226_write16(INA226_CONF_REG, 0x4003 | ((uint16_t)avgMode << 9) | ((uint16_t)vbusTime << 6) | ((uint16_t)shuntTime << 3) | ((uint16_t)continuous << 2));
}

uint16_t ina226_getMilliVolts() {
    return (uint32_t)ina226_read16(INA226_BUSV_REG) * 125 / 100;
}

int32_t ina226_getMicroAmps() {
#ifdef PRECISSION
    return (int64_t)ina226_read16(INA226_CURRENT_REG) * CURRENT_LSB / 1000;
#else
    return (int32_t)ina226_read16(INA226_CURRENT_REG) * CURRENT_LSB;
#endif
}

uint32_t ina226_getMicroWatts() {
#ifdef PRECISSION
    return (uint64_t)ina226_read16(INA226_POWER_REG) * CURRENT_LSB / 40;
#else
    return (uint32_t)ina226_read16(INA226_POWER_REG) * CURRENT_LSB * 25;
#endif
}
