#include <ch32v00x.h>
#include "utils.h"

#ifdef USE_MILLIS
static volatile uint32_t _ms = 0;
#endif

void UPDATE_REG16(volatile uint16_t *addr, uint16_t and_mask, uint16_t or_mask) {
    and_mask &= *addr;
    and_mask |= or_mask;
    *addr = and_mask;
}

void UPDATE_REG32(volatile uint32_t *addr, uint32_t and_mask, uint32_t or_mask) {
    and_mask &= *addr;
    and_mask |= or_mask;
    *addr = and_mask;
}

#ifndef USE_MILLIS
void delay_ms(uint32_t ms) {
    SysTick->SR = 0;
    SysTick->CMP = (FCPU / 8000) * ms;
    SysTick->CNT = 0;
    SysTick->CTLR = 1 << 0;

    while (! (SysTick->SR & 0x01)) {}
    SysTick->CTLR = 0;
}

#else
void init_systick(void) {
    NVIC_EnableIRQ(SysTicK_IRQn);
    SysTick->SR = 0;
    SysTick->CMP = FCPU / 1000;
    SysTick->CNT = 0;
    SysTick->CTLR = 0x0F;
}

inline __attribute__((always_inline)) uint32_t millis(void) {
    return _ms;
}

void delay(uint32_t ms) {
    uint32_t start = millis();

    while (millis() - start < ms) {}
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void SysTick_Handler(void) {
    ++_ms;
    SysTick->SR = 0;
}
#endif
