#include "twi.h"
#include "ssd1306.h"

#define SSD1306_ADDR    0x3C

extern uint8_t _screen[SCREEN_WIDTH * (SCREEN_HEIGHT / 8)];

static bool sendcommand(uint8_t cmd) {
    bool result;

    if ((result = TWI_Start(SSD1306_ADDR, false) == TWI_OK)) {
        result = TWI_Write(0x00) && TWI_Write(cmd);
        TWI_Stop();
    }
    return result;
}

static bool sendcommands(const uint8_t *cmds, uint8_t size) {
    bool result;

    if ((result = TWI_Start(SSD1306_ADDR, false) == TWI_OK)) {
        result = TWI_Write(0x00) && (TWI_Writes(cmds, size) == size);
        TWI_Stop();
    }
    return result;
}

static bool senddata(const uint8_t *data, uint16_t size, bool wait) {
    bool result;

    if ((result = TWI_Start(SSD1306_ADDR, false) == TWI_OK)) {
        if ((result = TWI_Write(0x40))) {
            if (wait) {
                result = TWI_Writes(data, size) == size;
            } else {
                TWI_WritesAsync(data, size, true);
                return true;
            }
        }
        TWI_Stop();
    }
    return result;
}

bool ssd1306_begin() {
    const uint8_t CMDS[] = {
        0xAE, // DISPLAYOFF
        0xD5, 0x80, // SETDISPLAYCLOCKDIV = 0x80
        0xA8, 0x3F, // SETMULTIPLEX
        0xD3, 0x00, // SETDISPLAYOFFSET = 0x00
        0x40 | 0, // SETSTARTLINE = 0
        0x8D, 0x14, // CHARGEPUMP = 0x14
        0xA0, // SEGREMAP = normal
        0xC0, // COMSCANINC
        0xDA, 0x12, // SETCOMPINS
        0xD9, 0x22, // SETPRECHARGE = 0x22
        0xDB, 0x40, // SETVCOMDETECT
        0x20, 0x00, // MEMORYMODE = HORIZONTAL_ADDRESSING_MODE
        0x21, 0x00, 0x7F, // SETCOLUMN
        0x22, 0x00, 0x07, // SETPAGE
        0x81, 0xCF, // SETCONTRAST
        0xA4, // DISPLAYALLON_RESUME
        0xA6, // NORMALDISPLAY
        0xAF // DISPLAYON
    };

    screen_clear();
    return sendcommands(CMDS, sizeof(CMDS));
}

bool ssd1306_flip(bool on) {
    const uint8_t CMDS[4] = {
        0xA0, // SEGREMAP = normal
        0xC0, // COMSCANINC
        0xA0 | 0x01, // SEGREMAP = reverse
        0xC8 // COMSCANDEC
    };

    return sendcommands(&CMDS[on ? 0 : 2], 2);
}

inline __attribute__((always_inline)) bool ssd1306_invert(bool on) {
    return sendcommand(0xA6 | on); // NORMALDISPLAY/INVERSEDISPLAY
}

inline __attribute__((always_inline)) bool ssd1306_power(bool on) {
    return sendcommand(0xAE | on); // DISPLAYON/DISPLAYOFF
}

bool ssd1306_contrast(uint8_t value) {
    uint8_t cmds[2];

    cmds[0] = 0x81; // SETCONTRAST
    cmds[1] = value;
    return sendcommands(cmds, sizeof(cmds));
}

inline __attribute__((always_inline)) bool ssd1306_flush(bool wait) {
    return senddata(_screen, sizeof(_screen), wait);
}
