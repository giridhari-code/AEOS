/*
 * AEOS x86_64 - COM1 16550 UART driver
 * Same API as the ARM PL011 driver so shell/kernel code is portable.
 */

#include "memory.h"
#include <stdint.h>

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* Line status register bits */
#define LSR_THRE 0x20   /* transmit holding empty */
#define LSR_DATA 0x01   /* data ready */

void uart_init(void)
{
    outb(COM1 + 1, 0x00);    /* disable interrupts */
    outb(COM1 + 3, 0x80);    /* DLAB on */
    outb(COM1 + 0, 0x01);    /* divisor low: 115200 */
    outb(COM1 + 1, 0x00);    /* divisor high */
    outb(COM1 + 3, 0x03);    /* 8N1, DLAB off */
    outb(COM1 + 2, 0xC7);    /* FIFO on, clear */
    outb(COM1 + 4, 0x0B);    /* RTS/DSR */
}

void uart_putc(char c)
{
    extern void vgacon_putc(char c);

#ifdef AEOS_VGA_MIRROR
    vgacon_putc(c);              /* optional: needs preempt-race fix */
#endif
    if (c == '\r')
        return;
    if (c == '\n')
        uart_putc('\r');
    while (!(inb(COM1 + 5) & LSR_THRE))
        ;
    outb(COM1, (uint8_t)c);
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
    uart_putc('\n');
}

void uart_puts_nolf(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

void uart_hex(unsigned long long v)
{
    const char *hex = "0123456789abcdef";
    char buf[17];
    int i;

    uart_puts_nolf("0x");
    if (v == 0) {
        uart_putc('0');
        return;
    }
    for (i = 15; i >= 0; i--) {
        buf[i] = hex[v & 0xF];
        v >>= 4;
    }
    buf[16] = '\0';
    i = 0;
    while (buf[i] == '0')
        i++;
    while (buf[i])
        uart_putc(buf[i++]);
}

void uart_dec(unsigned long v)
{
    char buf[21];
    int i = 20;

    buf[20] = '\0';
    if (v == 0) {
        uart_putc('0');
        return;
    }
    while (v > 0) {
        buf[--i] = '0' + (char)(v % 10);
        v /= 10;
    }
    while (buf[i])
        uart_putc(buf[i++]);
}

int uart_getc_nonblock(char *out)
{
    if (!(inb(COM1 + 5) & LSR_DATA))
        return 0;
    *out = (char)inb(COM1);
    return 1;
}

void uart_hex64(unsigned long v)
{
    int i;
    char hex[] = "0123456789abcdef";

    uart_puts_nolf("0x");
    for (i = 15; i >= 0; i--)
        uart_putc(hex[(v >> (i * 4)) & 0xF]);
}
