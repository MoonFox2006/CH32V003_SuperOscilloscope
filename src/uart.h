#pragma once

#define UART_SPEED  115200

void uartInit(void);
void uartWrite(char c);
void uartPrint(const char *str);
