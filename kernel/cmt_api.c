/*
 * AEOS - CMT Engine API (init, history, report)
 */
#include "cmt.h"
#include "cmt_core.h"
#include "memory.h"
#include "uart.h"

#define CMT_MAGIC 0x434D5430u
extern cmt_engine_t cmt;

void cmt_init(void) {
    uint32_t i;

    cmt.magic = CMT_MAGIC;

    cmt.state.level = 0;
    cmt.state.action = 0;
    cmt.state.inferences = 0;
    cmt.state.errors = 0;
    cmt.state.hz = 0;
    cmt.state.uptime = 0;
    cmt.state.disabled = 0;
    cmt.state.verbose = 0;

    for (i = 0; i < CMT_HIDDEN; i++)
        cmt.experience.dims[i] = 0;
    cmt.experience.energy = 0;
    cmt.experience.coherence = 0;

    cmt.last_input.p = 0;
    cmt.last_input.m = 0;
    cmt.last_input.b = 0;
    cmt.last_input.s = 0;
    cmt.last_input.a = 0;
    cmt.last_input.x = 0;

    for (i = 0; i < CMT_HIST_LEN; i++) {
        cmt.history[i].tick = 0;
        cmt.history[i].action = 0;
        cmt.history[i].level = 0;
        cmt.history[i].energy = 0;
    }
    cmt.hist_count = 0;
    cmt.ticks_since_infer = 0;
    cmt.ticks_total = 0;

    uart_puts("  CMT engine initialized");
}

void cmt_set_verbose(int enabled) { cmt.state.verbose = enabled; }
int  cmt_is_healthy(void) { return (cmt.magic == CMT_MAGIC && !cmt.state.disabled); }
void cmt_disable(void) { cmt.state.disabled = 1; }
void cmt_enable(void)  { cmt.state.disabled = 0; }

uint32_t cmt_inference_count(void) { return cmt.state.inferences; }
uint32_t cmt_error_count(void)     { return cmt.state.errors; }
int      cmt_last_action(void)     { return (int)cmt.state.action; }
const char *cmt_action_name(int a) { return cmt_core_action_name(a); }
uint32_t cmt_current_hz(void)      { return cmt.state.hz; }
int      cmt_consciousness_level(void) { return (int)cmt.state.level; }

int cmt_history_count(void) { return (int)cmt.hist_count; }

uint32_t cmt_history_tick(int idx) {
    if (idx < 0 || idx >= (int)cmt.hist_count) return 0;
    return cmt.history[idx].tick;
}
int cmt_history_action(int idx) {
    if (idx < 0 || idx >= (int)cmt.hist_count) return -1;
    return (int)cmt.history[idx].action;
}
int cmt_history_level(int idx) {
    if (idx < 0 || idx >= (int)cmt.hist_count) return -1;
    return (int)cmt.history[idx].level;
}
int32_t cmt_history_energy(int idx) {
    if (idx < 0 || idx >= (int)cmt.hist_count) return 0;
    return cmt.history[idx].energy;
}

const cmt_experience_t *cmt_get_experience(void) { return &cmt.experience; }
const cmt_input_t      *cmt_get_inputs(void)     { return &cmt.last_input; }
const cmt_state_t      *cmt_get_state(void)      { return &cmt.state; }

static const char *cmt_level_names[] = { "unconscious", "minimal", "aware", "focused" };

void cmt_report(void) {
    uart_puts("=== CMT Engine Report ===");
    uart_puts_nolf("  Consciousness: Level ");
    uart_dec(cmt.state.level);
    uart_puts_nolf(" (");
    /* Bounds check to prevent out-of-bounds array access */
    uint8_t level = cmt.state.level;
    if (level >= 4) level = 3;
    uart_puts_nolf(cmt_level_names[level]);
    uart_puts(")");
    uart_puts_nolf("  Action: ");
    uart_puts(cmt_core_action_name(cmt.state.action));
    uart_puts_nolf("  Energy: ");
    uart_dec(cmt.experience.energy);
    uart_puts_nolf("  Coherence: ");
    uart_dec(cmt.experience.coherence);
    uart_puts("");
    uart_puts_nolf("  Inferences: ");
    uart_dec(cmt.state.inferences);
    uart_puts_nolf("  History: ");
    uart_dec(cmt.hist_count);
    uart_puts_nolf("/");
    uart_dec(CMT_HIST_LEN);
    uart_puts("");
    uart_puts_nolf("  Uptime: ");
    uart_dec(cmt.state.uptime);
    uart_puts(" ticks");
}
