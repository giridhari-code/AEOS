/*
 * AEOS - PL011 UART (shared console)
 * Architecture: ARM64 (AArch64)
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_set_base(uintptr_t base);   /* loader: DTB-discovered base */
uintptr_t uart_get_base(void);

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);       /* string + newline */
void uart_puts_nolf(const char *s);  /* raw string */
void uart_hex(unsigned long long v);
void uart_hex32(uint32_t v);
void uart_dec(unsigned long v);

/* Receive: 1 = got a byte in *out, 0 = RX FIFO empty. */
int  uart_getc_nonblock(char *out);

#endif /* UART_H */
