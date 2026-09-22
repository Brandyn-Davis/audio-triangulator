#ifndef UART0_H_
#define UART0_H_

#include <stdint.h>
#include <stdbool.h>

void initUart0();
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc);
void putcUart0(char c);
void putsUart0(char* str);
char getcUart0();
bool kbhitUart0();

#endif
