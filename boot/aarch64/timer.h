/*
 * AEOS - ARM Generic Timer (public API)
 * Architecture: ARM64 (AArch64)
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void     timer_init(void);
void     timer_handler(void);
void     timer_set_hz(uint32_t hz);
uint32_t timer_get_hz(void);
uint64_t timer_get_ticks(void);
uint64_t timer_get_ms(void);
void     timer_set_callback(void (*cb)(void));

#endif /* TIMER_H */
