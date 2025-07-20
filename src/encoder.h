#pragma once

#include <stdbool.h>
#include <ch32v00x.h>

#ifdef USE_SPL
#define ENC_RCC     RCC_APB2Periph_GPIOC
#else
#define ENC_RCC     RCC_IOPCEN
#endif
#define ENC_GPIO    GPIOC
#define ENC_CLK     6
#define ENC_DT      7
#define ENC_BTN     3

enum { ENC_NONE, ENC_BTNCLICK, ENC_CCW, ENC_BTNCCW, ENC_CW, ENC_BTNCW };

void Encoder_Init();
uint8_t Encoder_Read();
