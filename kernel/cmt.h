/*
 * AEOS - Consciousness Manifold Theory (CMT) Engine
 *
 * Core Formula: C = E_f
 *
 * Implements the CMT model as a kernel subsystem:
 *   Inputs (P,M,B,S,A,...) -> IM -> T -> E_f -> C
 *
 * The engine runs on timer ticks, same as the AI engine.
 * It provides a richer input model than the basic AI:
 *   P - Perception (heap + page pressure)
 *   M - Memory    (allocation history, fragmentation)
 *   B - Body      (timer activity, tick rate)
 *   S - Self      (health, error count, inference count)
 *   A - Action    (last action taken, policy Hz)
 *   X - Context   (uptime, task count, watchdog state)
 *
 * All computation is Q16.16 fixed-point. No floating point.
 */

#ifndef AEOS_CMT_H
#define AEOS_CMT_H

#include <stdint.h>

/* ============================================================
 * Constants
 * ============================================================ */

#define CMT_INPUTS      6       /* P, M, B, S, A, X */
#define CMT_HIDDEN      12      /* wider than AI's 8 for richer manifold */
#define CMT_OUTPUTS     3       /* SUSTAIN, BOOST, REST */
#define CMT_PERIOD      50      /* inference every N ticks */
#define CMT_HIST_LEN    32      /* decision history depth */

/* Input indices */
#define CMT_IN_PERCEPTION   0   /* P */
#define CMT_IN_MEMORY       1   /* M */
#define CMT_IN_BODY         2   /* B */
#define CMT_IN_SELF         3   /* S */
#define CMT_IN_ACTION       4   /* A */
#define CMT_IN_CONTEXT      5   /* X */

/* Actions */
#define CMT_ACT_SUSTAIN  0
#define CMT_ACT_BOOST    1
#define CMT_ACT_REST     2

/* ============================================================
 * Types
 * ============================================================ */

/* Input manifold point (6D) */
typedef struct {
    int32_t p;      /* Perception: heap + page pressure */
    int32_t m;      /* Memory: allocation pattern */
    int32_t b;      /* Body: timer activity */
    int32_t s;      /* Self: health status */
    int32_t a;      /* Action: last decision */
    int32_t x;      /* Context: system state */
} cmt_input_t;

/* Embedded experience (compressed manifold representation) */
typedef struct {
    int32_t dims[CMT_HIDDEN];   /* hidden layer activations */
    int32_t energy;             /* total activation energy */
    int32_t coherence;          /* inter-neuron agreement */
} cmt_experience_t;

/* Consciousness state */
typedef struct {
    uint8_t  level;             /* 0=unconscious, 1=minimal, 2=aware, 3=focus */
    uint8_t  action;            /* last chosen action */
    uint32_t inferences;        /* total inference count */
    uint32_t errors;            /* total error count */
    uint32_t hz;                /* current policy rate */
    uint32_t uptime;            /* ticks since init */
    uint8_t  disabled;          /* fail-safe latch */
    uint8_t  verbose;           /* UART logging */
} cmt_state_t;

/* Decision history entry */
typedef struct {
    uint32_t tick;
    uint8_t  action;
    uint8_t  level;
    int32_t  energy;
} cmt_hist_entry_t;

/* Full CMT engine context */
typedef struct {
    uint32_t        magic;          /* magic word for corruption detection */
    cmt_state_t     state;
    cmt_experience_t experience;    /* last embedded experience */
    cmt_input_t     last_input;     /* last perceived inputs */
    cmt_hist_entry_t history[CMT_HIST_LEN];
    uint32_t        hist_count;
    uint32_t        ticks_since_infer;
    uint32_t        ticks_total;
} cmt_engine_t;

/* ============================================================
 * API
 * ============================================================ */

/* Initialize the CMT engine */
void cmt_init(void);

/* Tick entry point (called from timer IRQ via tick_subscribe) */
void cmt_on_tick(void);

/* Force an immediate inference cycle */
void cmt_force_inference(void);

/* Enable/disable verbose UART logging */
void cmt_set_verbose(int enabled);

/* Print engine state + history */
void cmt_report(void);

/* Health check */
int  cmt_is_healthy(void);
void cmt_disable(void);
void cmt_enable(void);

/* Accessors */
uint32_t     cmt_inference_count(void);
uint32_t     cmt_error_count(void);
int          cmt_last_action(void);
const char  *cmt_action_name(int action);
uint32_t     cmt_current_hz(void);
int          cmt_consciousness_level(void);

/* History ring buffer */
int      cmt_history_count(void);
uint32_t cmt_history_tick(int idx);
int      cmt_history_action(int idx);
int      cmt_history_level(int idx);
int32_t  cmt_history_energy(int idx);

/* Current state access */
const cmt_experience_t *cmt_get_experience(void);
const cmt_input_t      *cmt_get_inputs(void);
const cmt_state_t      *cmt_get_state(void);

#endif /* AEOS_CMT_H */
