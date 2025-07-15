#pragma once

#include <inttypes.h>

//#define USE_MILLIS

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(a[0]))

void UPDATE_REG16(volatile uint16_t *addr, uint16_t and_mask, uint16_t or_mask);
void UPDATE_REG32(volatile uint32_t *addr, uint32_t and_mask, uint32_t or_mask);

#ifdef USE_MILLIS
void init_systick(void);
uint32_t millis(void);
void delay(uint32_t ms);
#else
void delay_ms(uint32_t ms);
#endif
