#include <ch32v00x.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_SPL
#include <ch32v00x_gpio.h>
#include <ch32v00x_rcc.h>
#endif
#include "encoder.h"
#include "globals.h"
#include "ssd1306.h"
#include "twi.h"
#include "utils.h"

#define BLACK 0
#define WHITE 1

uint8_t ball_x = 64, ball_y = 20;
uint8_t ball_dir_x = 1, ball_dir_y = -1;

uint8_t player_x = 52;
const uint8_t PLAYER_Y = 60;
const uint8_t PADDLE_WIDTH = 32;
bool bricks[2][16];

void reset_bricks() {
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 16; col++) {
            bricks[row][col] = true;
        }
    }
}

bool allBricksCleared() {
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 16; col++) {
            if (bricks[row][col]) return false;
        }
    }
    return true;
}

void drawGame() {
    screen_clear();

    // Draw bricks
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 16; col++) {
            if (bricks[row][col]) {
                screen_fillrect(col * 8, row * 6, 7, 4, WHITE);
            }
        }
    }

    // Draw paddle
    screen_fillrect(player_x, PLAYER_Y, PADDLE_WIDTH, 2, WHITE);

    // Draw ball
    screen_fillrect(ball_x, ball_y, 2, 2, WHITE);

    delay_ms(10);
    ssd1306_flush(0);
}

void reset_game() {
    reset_bricks();
    ball_x = 64;
    ball_y = 20;
    ball_dir_x = 1;
    ball_dir_y = -1;
}

void brick_breaker(void) {
    reset_bricks();

    while (1) {
        int8_t paddle_direction = Encoder_Read();

        if (paddle_direction < 0 && player_x > 0)
            player_x -= 4;
        else if (paddle_direction > 0 && player_x + PADDLE_WIDTH < SCREEN_WIDTH)
            player_x += 4;

        ball_x += ball_dir_x;
        ball_y += ball_dir_y;

        // Wall collision
        if (ball_x <= 0 || ball_x >= SCREEN_WIDTH - 4) ball_dir_x = -ball_dir_x;
        if (ball_y <= 0) ball_dir_y = -ball_dir_y;

        if (ball_y > PLAYER_Y) reset_game();

        // Paddle collision
        if (ball_y >= PLAYER_Y - 2 && ball_x >= player_x &&
            ball_x <= player_x + PADDLE_WIDTH) {
            ball_dir_y = -ball_dir_y;
        }

        // Brick collision
        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 16; col++) {
                if (bricks[row][col]) {
                    int bx = col * 8;
                    int by = row * 6;

                    if (ball_x >= bx && ball_x <= bx + 7 && ball_y >= by &&
                        ball_y <= by + 4) {
                        bricks[row][col] = false;
                        ball_dir_y = -ball_dir_y;
                    }
                }
            }
        }

        // Refill bricks if all destroyed
        if (allBricksCleared()) {
            reset_game();
        }

        drawGame();
    }
}