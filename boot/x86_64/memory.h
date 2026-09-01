/*
 * AEOS x86_64 - compat memory header
 * Same API surface the portable kernel code expects.
 */

#ifndef AEOS_X86_MEMORY_H
#define AEOS_X86_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define RAM_START       0x0000000000100000ULL   /* 1MB */
#define RAM_SIZE        (128u * 1024u * 1024u)  /* 128MB phase-1 default */
#define RAM_END         (RAM_START + RAM_SIZE)
#define KERNEL_BASE     0x100000

/* UART (COM1) - same names as the ARM driver */
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);          /* string + newline */
void uart_puts_nolf(const char *s);     /* raw string */
void uart_hex(unsigned long long v);
void uart_dec(unsigned long v);
int  uart_getc_nonblock(char *out);

/* IRQ / timer - same names as the ARM side */
void irq_init(void);
int  tick_subscribe(void (*fn)(void));
void irq_enable(void);
void irq_disable(void);
uint64_t timer_get_ticks(void);
uint64_t timer_get_ms(void);
void timer_set_hz(uint32_t hz);

/* Memory shims (mm.c) - counters for shell 'mem' command */
uint32_t pmm_get_total_pages(void);
uint32_t pmm_get_free_pages(void);
uint32_t heap_get_used(void);
uint32_t heap_get_free(void);
size_t   heap_get_capacity(void);

/* Master header: pull in portable subsystem APIs so the kernel
 * sources compile unchanged, same as the ARM build. */
#include "ai_api.h"
#include "ai_core.h"

/* Arch idle wait: scheduler park loops use this (hlt on x86). */
#ifndef AEOS_IDLE_WAIT
#define AEOS_IDLE_WAIT() __asm__ volatile ("hlt")
#endif

#endif /* AEOS_X86_MEMORY_H */
