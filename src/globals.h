#pragma once

#include <inttypes.h>
#include <stdbool.h>

#define ADC_DATA_SIZE   120
#define ADC_TRIG_SIZE   (ADC_DATA_SIZE / 2)

#define SNAKE_MAX   64

typedef union {
    struct {
        uint16_t adc_data[ADC_DATA_SIZE + ADC_TRIG_SIZE];
        char timestr[3 + 8];
        uint16_t vcc; // 500 or 330
        uint16_t vref; // ~1.2 V internal voltage reference
        volatile uint8_t _adc_flags;
        uint8_t trig;
    } oscilloscope;
    struct {
        uint16_t snakeBody[SNAKE_MAX];
        uint16_t snakeTail, snakeLen;
        uint16_t eaten, lastEaten;
        uint8_t direction;
        uint8_t foodX, foodY;
        bool updating;
    } snake;
    struct {
        uint16_t corrFactor;
        uint8_t shunt;
    } power;
} vars_t;

extern vars_t vars;
