/*
 * AEOS - AI Engine API (public functions, history, report)
 */
#include "ai_api.h"
#include "ai_core.h"
#include "ai_learn.h"
#include "ai_debug.h"
#include "memory.h"
#include "uart.h"
#include "cmt.h"

#define AI_MAGIC 0x41493130u

typedef struct {
    uint32_t tick;
    uint8_t  action;
} ai_hist_entry_t;

typedef struct {
    uint32_t       magic;
    uint32_t       ticks_total;
    uint32_t       ticks_since_infer;
    uint32_t       inferences;
    uint32_t       errors;
    uint32_t       hz;
    uint8_t        action;
    uint8_t        disabled;
    uint8_t        verbose;
    uint8_t        in_tick;
    ai_hist_entry_t hist[AI_HIST_LEN];
    uint32_t       hist_count;
} ai_state_t;

static ai_state_t ai;

static int ai_healthy(void) {
    if (ai.magic != AI_MAGIC || ai.disabled) {
        if (ai.magic == AI_MAGIC) ai.errors++;
        ai.disabled = 1;
        return 0;
    }
    return 1;
}

void ai_init(void) {
    ai.magic = AI_MAGIC;
    ai.ticks_total = 0;
    ai.ticks_since_infer = 0;
    ai.inferences = 0;
    ai.errors = 0;
    ai.hz = 100;
    ai.action = AI_ACT_SUSTAIN;
    ai.disabled = 0;
    ai.verbose = 0;
    ai.in_tick = 0;
    ai.hist_count = 0;
    
    /* Initialize learning engine */
    ai_learn_init();
    
    /* Initialize debugger */
    ai_debug_init();
    
    tick_subscribe(ai_on_tick);
}

void ai_on_tick(void) {
    int32_t x[AI_INPUTS];
    int action;

    if (!ai_healthy()) return;
    if (ai.in_tick) return;
    ai.in_tick = 1;

    ai.ticks_total++;
    ai.ticks_since_infer++;

    cmt_on_tick();

    if (ai.ticks_since_infer >= AI_PERIOD_TICKS) {
        ai.ticks_since_infer = 0;

        uint32_t hu = heap_get_used(), hc = heap_get_capacity();
        uint32_t pf = pmm_get_free_pages(), pt = pmm_get_total_pages();
        
        /* Division-by-zero guard */
        uint32_t heap_pct = (hc > 0) ? (hu * 100) / hc : 0;
        uint32_t page_pct = (pt > 0) ? ((pt - pf) * 100) / pt : 0;
        
        /* Get current state for learning */
        uint8_t state = ai_learn_get_state(heap_pct, page_pct, ai.errors);
        
        /* Learning: choose action (may explore) */
        action = ai_learn_choose_action(state);
        
        /* Get neural network decision for comparison — pass all 6 inputs */
        ai_perceive(x, ai.ticks_since_infer, hu, hc, pf, pt);
        int nn_action = ai_decide(x);
        if (action == AI_ACT_SUSTAIN && nn_action != AI_ACT_SUSTAIN) {
            /* Trust neural net if learning suggests sustain but NN disagrees */
            action = nn_action;
        }

        int cmt_level = cmt_consciousness_level();
        ai_act(action, cmt_level, &ai.hz);
        timer_set_hz(ai.hz);

        ai.action = (uint8_t)action;
        ai.inferences++;

        ai_hist_entry_t *e = &ai.hist[ai.hist_count % AI_HIST_LEN];
        e->tick = ai.ticks_total;
        e->action = (uint8_t)action;
        ai.hist_count++;

        /* Learning: calculate reward and store experience */
        int16_t reward = ai_learn_calculate_reward(heap_pct, page_pct, ai.errors, ai.inferences);
        uint8_t next_state = ai_learn_get_state(heap_pct, page_pct, ai.errors);
        ai_learn_store(state, action, reward, next_state);
        
        /* Learning: update Q-table periodically */
        if (ai.inferences % 10 == 0) {
            ai_learn_update();
        }
        
        /* Debugger: scan and auto-fix every 20 inferences */
        if (ai.inferences % 20 == 0) {
            ai_debug_scan();
            ai_debug_autofix();
        }

        if (ai.verbose) {
            uart_puts_nolf("[ai] t=");
            uart_dec(ai.ticks_total);
            uart_puts_nolf(" C=");
            uart_dec(cmt_level);
            uart_puts_nolf(" L=");
            uart_dec(action);
            uart_puts_nolf(" -> ");
            uart_puts(ai_core_action_name(action));
            uart_puts_nolf("[ai] policy ");
            uart_dec(ai.hz);
            uart_puts(" Hz");
        }
    }
    ai.in_tick = 0;
}

void ai_set_verbose(int enabled) { ai.verbose = enabled ? 1 : 0; }

void ai_reset(void) {
    uint8_t v = ai.verbose;
    ai_init();
    ai.verbose = v;
}

void ai_report(void) {
    uart_puts_nolf("  AI healthy: ");
    uart_puts(ai_is_healthy() ? "yes" : "NO (fail-safe latched)");
    uart_puts_nolf("  AI inferences: "); uart_dec(ai.inferences); uart_puts("");
    uart_puts_nolf("  AI errors: "); uart_dec(ai.errors); uart_puts("");
    uart_puts_nolf("  AI last action: "); uart_puts(ai_core_action_name(ai.action));
    uart_puts_nolf("  AI timer policy: "); uart_dec(ai.hz); uart_puts(" Hz");

    uint32_t n = (ai.hist_count < AI_HIST_LEN) ? ai.hist_count : AI_HIST_LEN;
    if (n > 0) {
        uint32_t start = ai.hist_count - n;
        uart_puts("  AI decisions:");
        for (uint32_t i = 0; i < n; i++) {
            uart_puts_nolf("    t=");
            uart_dec(ai.hist[(start + i) % AI_HIST_LEN].tick);
            uart_puts_nolf("  ");
            uart_puts(ai_core_action_name(ai.hist[(start + i) % AI_HIST_LEN].action));
        }
    }
    
    /* Learning report */
    ai_learn_report();
    
    /* Debug report */
    ai_debug_report();
}

uint32_t ai_inference_count(void) { return ai.inferences; }
uint32_t ai_error_count(void)     { return ai.errors; }
int      ai_last_action(void)     { return ai.action; }
const char *ai_action_name(int a) { return ai_core_action_name(a); }
uint32_t ai_current_hz(void)      { return ai.hz; }
int      ai_is_healthy(void)      { return ai.magic == AI_MAGIC && !ai.disabled; }

int ai_history_count(void) {
    uint32_t n = ai.hist_count;
    return (n < AI_HIST_LEN) ? (int)n : (int)AI_HIST_LEN;
}

uint32_t ai_history_tick(int idx) {
    int total = ai_history_count();
    if (idx < 0 || idx >= total) return 0;
    uint32_t start = (ai.hist_count < AI_HIST_LEN) ? 0 : ai.hist_count - AI_HIST_LEN;
    return ai.hist[(start + idx) % AI_HIST_LEN].tick;
}

int ai_history_action(int idx) {
    int total = ai_history_count();
    if (idx < 0 || idx >= total) return -1;
    uint32_t start = (ai.hist_count < AI_HIST_LEN) ? 0 : ai.hist_count - AI_HIST_LEN;
    return (int)ai.hist[(start + idx) % AI_HIST_LEN].action;
}

void ai_disable(void) { ai.disabled = 1; }
void ai_enable(void)  { ai.disabled = 0; ai.magic = AI_MAGIC; }
