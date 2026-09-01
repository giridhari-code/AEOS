/*
 * AEOS - PL011 UART driver
 * Architecture: ARM64 (AArch64)
 * Target: QEMU virt machine
 */

#include "uart.h"
#include "memory.h"

/*
 * Runtime MMIO base. Defaults to the classic PL011 address; the
 * loader overrides it from the firmware DTB so the SAME binary
 * runs on any board whose UART is discoverable - no recompile,
 * no emulator tie-in.
 */
static uintptr_t uart_base = UART_BASE;

#define UART_DR      (uart_base + 0x000)
#define UART_FR      (uart_base + 0x018)
#define UART_IBRD    (uart_base + 0x024)
#define UART_FBRD    (uart_base + 0x028)
#define UART_LCRH    (uart_base + 0x02C)
#define UART_CR      (uart_base + 0x030)
#define UART_IMSC    (uart_base + 0x038)
#define UART_FR_TXFF (1 << 5)
#define UART_FR_RXFE (1 << 4)

void uart_set_base(uintptr_t base)
{
    if (base)
        uart_base = base;
}

uintptr_t uart_get_base(void)
{
    return uart_base;
}

static void uart_putc_raw(char c)
{
    while (*(volatile unsigned int *)UART_FR & UART_FR_TXFF)
        ;
    *(volatile unsigned char *)UART_DR = (unsigned char)c;
}

void uart_init(void)
{
    *(volatile unsigned int *)UART_CR = 0;          /* disable */
    *(volatile unsigned int *)UART_IMSC = 0x7FF;    /* mask all IRQs */
    *(volatile unsigned int *)UART_IBRD = 13;       /* 115200 @ 24MHz */
    *(volatile unsigned int *)UART_FBRD = 1;
    *(volatile unsigned int *)UART_LCRH = (3 << 5) | (1 << 4); /* 8N1, FIFO */
    *(volatile unsigned int *)UART_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putc(char c)
{
    if (c == '\r')
        return;
    if (c == '\n')
        uart_putc_raw('\r');
    uart_putc_raw(c);
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

void uart_hex32(uint32_t v)
{
    const char *hex = "0123456789abcdef";
    char buf[9];
    int i;

    uart_puts_nolf("0x");
    if (v == 0) {
        uart_putc('0');
        return;
    }
    for (i = 7; i >= 0; i--) {
        buf[i] = hex[v & 0xF];
        v >>= 4;
    }
    buf[8] = '\0';
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
    if (*(volatile unsigned int *)UART_FR & UART_FR_RXFE)
        return 0;                        /* RX FIFO empty */
    *out = (char)(*(volatile unsigned char *)UART_DR & 0xFF);
    return 1;
}
