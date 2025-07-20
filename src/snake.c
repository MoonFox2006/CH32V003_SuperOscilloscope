#include <string.h>
#include <stdlib.h>
#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#endif
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "encoder.h"
#include "globals.h"

#define GAME_WIDTH  32
#define GAME_HEIGHT 16

#define FPS         8

enum { DIR_STOP, DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT };

static void game_pixel(uint8_t x, uint8_t y, bool color) {
    extern uint8_t _screen[SCREEN_WIDTH * (SCREEN_HEIGHT / 8)];

    uint16_t offset = 0; // SCREEN_WIDTH * (y / 8 * 4) + x * 4;
    uint8_t pattern;

    for (uint8_t i = 0; i < y >> 1; ++i)
        offset += SCREEN_WIDTH;
    offset += x << 2;
    pattern = y & 0x01 ? 0xF0 : 0x0F;
    if (! color)
        pattern = ~pattern;
    for (uint8_t i = 0; i < 4; ++i) {
        if (color)
            _screen[offset] |= pattern;
        else
            _screen[offset] &= pattern;
        ++offset;
    }
}

static void game_digit(uint8_t x, uint8_t y, uint8_t digit) {
    const uint8_t DIGITS[][3] = {
        { 0x1F, 0x11, 0x1F }, // 0
        { 0x11, 0x1F, 0x10 }, // 1
        { 0x1D, 0x15, 0x17 }, // 2
        { 0x15, 0x15, 0x0A }, // 3
        { 0x07, 0x04, 0x1F }, // 4
        { 0x17, 0x15, 0x1D }, // 5
        { 0x1F, 0x15, 0x1D }, // 6
        { 0x01, 0x01, 0x1F }, // 7
        { 0x1F, 0x15, 0x1F }, // 8
        { 0x17, 0x15, 0x1F }  // 9
    };

    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t bits = DIGITS[digit][i]; // digit * 3 + i

        for (uint8_t j = 0; j < 5; ++j) {
            game_pixel(x + i, y + j, (bits & (1 << j)) != 0);
        }
    }
}

static inline __attribute__((always_inline)) uint16_t encodeXY(uint8_t x, uint8_t y) {
    return (y << 5) + x; // y * GAME_WIDTH + x
}

static inline __attribute__((always_inline)) uint8_t decodeX(uint16_t xy) {
    return xy & (GAME_WIDTH - 1); // xy % GAME_WIDTH
}

static inline __attribute__((always_inline)) uint8_t decodeY(uint16_t xy) {
    return xy >> 5; // xy / GAME_WIDTH
}

static void incBCD(uint16_t *bcd) {
    for (uint8_t i = 0; i < 16; i += 4) {
        if (((*bcd >> i) & 0x0F) < 9) {
            *bcd += (1 << i);
            break;
        }
        *bcd &= ~(0x0F << i);
    }
}

static void initSnake() {
    vars.snake.snakeBody[0] = encodeXY(0, GAME_HEIGHT / 2); // GAME_HEIGHT >> 1
    vars.snake.snakeBody[1] = vars.snake.snakeBody[0] + 1; // encodeXY(1, GAME_HEIGHT / 2);
    vars.snake.snakeBody[2] = vars.snake.snakeBody[1] + 1; // encodeXY(2, GAME_HEIGHT / 2);
    vars.snake.snakeTail = 0;
    vars.snake.snakeLen = 3;
    vars.snake.foodX = 4;
    vars.snake.foodY = GAME_HEIGHT / 2; // GAME_HEIGHT >> 1
    vars.snake.direction = DIR_STOP;
    vars.snake.lastEaten = vars.snake.eaten;
    vars.snake.eaten = 0;
}

static bool inSnake(uint8_t x, uint8_t y, bool ignoreLast) {
    uint16_t xy = encodeXY(x, y);

    for (uint16_t i = ignoreLast; i < vars.snake.snakeLen; ++i) { // ... i = ignoreLast ? 1 : 0 ...
        if (vars.snake.snakeBody[(vars.snake.snakeTail + i) & (SNAKE_MAX - 1)] == xy) // % SNAKE_MAX
            return true;
    }
    return false;
}

static void moveSnake(uint8_t x, uint8_t y) {
    game_pixel(decodeX(vars.snake.snakeBody[vars.snake.snakeTail]), decodeY(vars.snake.snakeBody[vars.snake.snakeTail]), 0);
    if (++vars.snake.snakeTail >= SNAKE_MAX)
        vars.snake.snakeTail = 0;
    vars.snake.snakeBody[(vars.snake.snakeTail + vars.snake.snakeLen - 1) & (SNAKE_MAX - 1)] = encodeXY(x, y); // % SNAKE_MAX
    game_pixel(x, y, 1);
    vars.snake.updating = true;
}

static void growSnake(uint8_t x, uint8_t y) {
    if (vars.snake.snakeLen < SNAKE_MAX) {
        ++vars.snake.snakeLen;
    } else {
        game_pixel(decodeX(vars.snake.snakeBody[vars.snake.snakeTail]), decodeY(vars.snake.snakeBody[vars.snake.snakeTail]), 0);
        if (++vars.snake.snakeTail >= SNAKE_MAX)
            vars.snake.snakeTail = 0;
    }
    vars.snake.snakeBody[(vars.snake.snakeTail + vars.snake.snakeLen - 1) & (SNAKE_MAX - 1)] = encodeXY(x, y); // % SNAKE_MAX
    game_pixel(x, y, 1);
    vars.snake.updating = true;
    incBCD(&vars.snake.eaten);
}

static void drawSnake() {
    for (uint16_t i = 0; i < vars.snake.snakeLen; ++i) {
        uint16_t index = (vars.snake.snakeTail + i) & (SNAKE_MAX - 1); // % SNAKE_MAX

        game_pixel(decodeX(vars.snake.snakeBody[index]), decodeY(vars.snake.snakeBody[index]), 1);
    }
    vars.snake.updating = true;
}

static void drawFood() {
    game_pixel(vars.snake.foodX, vars.snake.foodY, 1);
    vars.snake.updating = true;
}

static void newFood() {
    do {
        vars.snake.foodX = rand() & (GAME_WIDTH - 1); // % GAME_WIDTH
        vars.snake.foodY = rand() & (GAME_HEIGHT - 1); // % GAME_HEIGHT
    } while (inSnake(vars.snake.foodX, vars.snake.foodY, false));
}

static void drawBCD(uint16_t bcd) {
    uint8_t x;
    int8_t d = 12;

    while ((d > 0) && (((bcd >> d) & 0x0F) == 0))
        d -= 4;
    if (d < 4)
        x = (GAME_WIDTH - 3) / 2; // ... >> 1
    else if (d < 8)
        x = (GAME_WIDTH - 7) / 2;
    else if (d < 12)
        x = (GAME_WIDTH - 11) / 2;
    else
        x = (GAME_WIDTH - 15) / 2;
    while (d >= 0) {
        game_digit(x, 0, (bcd >> d) & 0x0F);
        d -= 4;
        x += 4;
    }
    vars.snake.updating = true;
}

static void startScreen() {
    screen_clear();
    drawSnake();
    drawFood();
    drawBCD(vars.snake.lastEaten);
//    vars.snake.updating = true;
}

void snake(void) {
    bool pause = false;

    vars.snake.eaten = 0;
    vars.snake.updating = false;

    initSnake();
    startScreen();

    SysTick->SR = 0;
    SysTick->CMP = (FCPU / 8000) * (1000 / FPS);
    SysTick->CNT = 0;
    SysTick->CTLR = (1 << 3) | (1 << 0); // STRE | STE

    while (1) {
        uint8_t e;

        while (! (SysTick->SR & 0x01)) {}
        SysTick->SR = 0;

        e = Encoder_Read();
        if (e != ENC_NONE) {
            if (vars.snake.direction != DIR_STOP) {
                if (e == ENC_BTNCLICK) {
                    pause = ! pause;
                } else if (! pause) {
                    if (e == ENC_CCW) { // Counterclockwise
                        if (vars.snake.direction == DIR_UP)
                            vars.snake.direction = DIR_LEFT;
                        else
                            --vars.snake.direction;
                    } else if (e == ENC_CW) { // Clockwise
                        if (vars.snake.direction == DIR_LEFT)
                            vars.snake.direction = DIR_UP;
                        else
                            ++vars.snake.direction;
                    }
                }
            } else {
                if (e == ENC_BTNCLICK) { // Start
                    vars.snake.direction = DIR_RIGHT;
                    screen_clear();
                    drawSnake();
                    drawFood();
                    pause = false;
                }
            }
        }

        if ((vars.snake.direction != DIR_STOP) && (! pause)) {
            int16_t x, y;

            y = (vars.snake.snakeTail + vars.snake.snakeLen - 1) & (SNAKE_MAX - 1); // % SNAKE_MAX
            x = decodeX(vars.snake.snakeBody[y]);
            y = decodeY(vars.snake.snakeBody[y]);
            switch (vars.snake.direction) {
                case DIR_UP:
                    --y;
                    break;
                case DIR_RIGHT:
                    ++x;
                    break;
                case DIR_DOWN:
                    ++y;
                    break;
                case DIR_LEFT:
                    --x;
                    break;
                default:
                    break;
            }
            if ((x < 0) || (x > GAME_WIDTH - 1) || (y < 0) || (y > GAME_HEIGHT - 1) || inSnake(x, y, true)) {
                initSnake();
                startScreen();
            } else {
                if ((x == vars.snake.foodX) && (y == vars.snake.foodY)) {
                    growSnake(x, y);
                    newFood();
                    drawFood();
                } else {
                    moveSnake(x, y);
                }
            }
        }
        if (vars.snake.updating) {
            ssd1306_flush(false);
            vars.snake.updating = false;
        }
    }
}
