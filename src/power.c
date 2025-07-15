#define USE_UART

#include <string.h>
#include <stdlib.h>
#include <ch32v00x.h>
#ifdef USE_UART
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_usart.h>
#endif
#endif
#include "utils.h"
#include "twi.h"
#include "ssd1306.h"
#include "ina226.h"
#include "strutils.h"

#ifdef USE_UART
#define UART_SPEED  115200

static void uartInit(void) {
#ifdef USE_SPL
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = UART_SPEED;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
#else
    uint32_t tmp, brr;

    RCC->APB2PCENR |= (RCC_USART1EN | RCC_IOPDEN);

    /* USART1 TX-->D5 */
    GPIOD->CFGLR &= ~((uint32_t)0x0F << (4 * 5));
    GPIOD->CFGLR |= ((uint32_t)0x0B << (4 * 5));

    tmp = (25 * FCPU) / (4 * UART_SPEED);
    brr = (tmp / 100) << 4;
    tmp -= 100 * (brr >> 4);
    brr |= (((tmp * 16) + 50) / 100) & 0x0F;

    USART1->BRR = brr;
    USART1->CTLR2 = 0;
    USART1->CTLR3 = 0;
    USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE;
#endif
}

static void uartWrite(char c) {
#ifdef USE_SPL
    while (! USART_GetFlagStatus(USART1, USART_FLAG_TXE)) {}
    USART_SendData(USART1, c);
#else
    while (! (USART1->STATR & USART_STATR_TXE)) {}
    USART1->DATAR = c;
#endif
}

static void uartPrint(const char *str) {
    while (*str) {
        uartWrite(*str++);
    }
}
#endif

static void normalizeU32(char *str, uint32_t value, const char *suffix) {
    char *s;
    bool milli;

    if (value < 1000000) {
        milli = true;
    } else {
        milli = false;
        value /= 1000;
    }
//    sprintf(str, "%u.%03u %s%s", (uint16_t)(value / 1000), (uint16_t)(value % 1000), prefix, suffix);
    s = u16str(str, value / 1000, 1);
    *s++ = '.';
    s = u16str(s, value % 1000, 100);
    *s++ = ' ';
    if (milli)
        *s++ = 'm';
    strcpy(s, suffix);
}

static void normalizeI32(char *str, int32_t value, const char *suffix) {
    char *s;
    bool milli;

    if (labs(value) < 1000000) {
        milli = true;
    } else {
        milli = false;
        value /= 1000;
    }
//    sprintf(str, "%d.%03u %s%s", (int16_t)(value / 1000), (uint16_t)(labs(value) % 1000), prefix, suffix);
    s = i16str(str, value / 1000, 1);
    *s++ = '.';
    s = u16str(s, labs(value) % 1000, 100);
    *s++ = ' ';
    if (milli)
        *s++ = 'm';
    strcpy(s, suffix);
}

void power(void) {
    if ((! ina226_begin()) || (! ina226_measure(true, AVG64, US8244, US8244))) {
        screen_clear();
        screen_printstr_x2("INA226", 0, 16, 1);
        screen_printstr_x2("not ready!", 0, 32, 1);
        ssd1306_flush(true);
//        while (1) {}
        delay_ms(5000); // 5 sec.
        return;
    } else {
        screen_clear();
        screen_printstr_x2("Wait...", 0, 24, 1);
        ssd1306_flush(true);
    }

    const char PROGRESS[4] = { '-', '\\', '|', '/' };

    uint64_t totalMicroAmps = 0;
    uint32_t totalTime = 0;

#ifdef USE_UART
    uartInit();
#endif

    while (1) {
        if (ina226_ready()) {
            char str[16];
            char *s;
            int32_t microAmps;
            uint32_t microWatts;
            uint16_t milliVolts;

            milliVolts = ina226_getMilliVolts();
            microAmps = ina226_getMicroAmps();
            microWatts = ina226_getMicroWatts();
            totalMicroAmps += labs(microAmps);
            ++totalTime;
            screen_clear();
            screen_printchar(PROGRESS[totalTime & 0x03], SCREEN_WIDTH - FONT_WIDTH, 0, 1);
//            snprintf(str, sizeof(str), "%u.%03u V", milliVolts / 1000, milliVolts % 1000);
            s = u16str(str, milliVolts / 1000, 1);
            *s++ = '.';
            s = u16str(s, milliVolts % 1000, 100);
            *s++ = ' ';
            *s++ = 'V';
            *s = '\0';
            screen_printstr_x2(str, 0, 0, 1);
            normalizeI32(str, microAmps, "A");
            screen_printstr_x2(str, 0, 16, 1);
            normalizeU32(str, microWatts, "W");
            screen_printstr_x2(str, 0, 32, 1);
            normalizeU32(str, (uint32_t)((totalMicroAmps * 3600000000ULL / 1055232) / totalTime), "A/h");
            screen_printstr_x2(str, 0, 48, 1);
            ssd1306_flush(true);
#ifdef USE_UART
            itoa(microAmps, str, 10);
            uartPrint(str);
            uartPrint("\r\n");
#endif
        }
    }
}
