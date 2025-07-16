#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_usart.h>
#endif
#include "utils.h"
#include "uart.h"

void uartInit(void) {
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

void uartWrite(char c) {
#ifdef USE_SPL
    while (! USART_GetFlagStatus(USART1, USART_FLAG_TXE)) {}
    USART_SendData(USART1, c);
#else
    while (! (USART1->STATR & USART_STATR_TXE)) {}
    USART1->DATAR = c;
#endif
}

void uartPrint(const char *str) {
    while (*str) {
        uartWrite(*str++);
    }
}
