/*
 * AEOS - In-Kernel AI Engine (public API)
 *
 * A self-contained kernel subsystem implementing a perceive-decide-act
 * control loop on top of a tiny fixed-point neural policy.
 *
 * Design contract:
 *   - Fixed-point only (Q16.16). No floating point anywhere.
 *   - ISR-safe: ai_on_tick() is called from the timer interrupt.
 *     It uses only static state, never blocks, never allocates,
 *     and never returns an error to its caller.
 *   - Fail-safe: any internal invariant violation latches the engine
 *     into a disabled state instead of propagating a fault. The
 *     kernel keeps running without AI; ai_reset() re-enables it.
 *   - Zero dynamic memory. All weights are compile-time constants.
 *
 * NOTE: this is the Phase-0 C embodiment of the AI subsystem. When the
 * Rust kernel core lands, this logic migrates behind the same tick
 * subscription interface.
 */

#ifndef AEOS_AI_H
#define AEOS_AI_H

#include <stdint.h>

/* Policy actions */
#define AI_ACT_SUSTAIN 0
#define AI_ACT_BOOST   1
#define AI_ACT_REST    2

/* Network architecture */
#define AI_INPUTS  3
#define AI_HIDDEN  8
#define AI_OUTPUTS 3

/* Timer policy bounds (Hz) enforced by the Act stage */
#define AI_HZ_MIN      20u
#define AI_HZ_MAX      200u
#define AI_HZ_STEP     25u

/* Run one inference every N timer ticks */
#define AI_PERIOD_TICKS 50u

/* Decision history depth (ring buffer) */
#define AI_HIST_LEN    16u

/*
 * Initialize the engine and subscribe it to timer ticks.
 * Safe to call before interrupts are enabled: the subscription is
 * inert until the timer IRQ path starts dispatching.
 */
void ai_init(void);

/*
 * Tick entry point. Invoked by the IRQ dispatcher for every timer
 * interrupt. Accumulates ticks and runs one perceive-decide-act
 * cycle every AI_PERIOD_TICKS. Guaranteed bounded-time: a fixed
 * number of fixed-point multiply-accumulates, nothing else.
 */
void ai_on_tick(void);

/* Enable/disable live UART logging of each decision (default off). */
void ai_set_verbose(int enabled);

/* Print engine statistics plus the decision history ring buffer. */
void ai_report(void);

/* Clear the fail-safe latch and reset counters (keeps weights). */
void ai_reset(void);

/* Accessors */
uint32_t ai_inference_count(void);
uint32_t ai_error_count(void);
int      ai_last_action(void);   /* one of AI_ACT_* */
const char *ai_action_name(int action);
uint32_t ai_current_hz(void);
int      ai_is_healthy(void);    /* 0 once the fail-safe has latched */

/* History ring buffer (matches SDK contract) */
int      ai_history_count(void);
uint32_t ai_history_tick(int idx);
int      ai_history_action(int idx);

/* Enable/disable (matches SDK contract) */
void     ai_disable(void);
void     ai_enable(void);

#endif /* AEOS_AI_H */
