/* AEOS x86_64 - UART API (see uart.c) */
#ifndef AEOS_X86_UART_H
#define AEOS_X86_UART_H

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
void uart_puts_nolf(const char *s);
void uart_hex(unsigned long long v);
void uart_hex64(unsigned long v);
void uart_dec(unsigned long v);
int  uart_getc_nonblock(char *out);

#endif
