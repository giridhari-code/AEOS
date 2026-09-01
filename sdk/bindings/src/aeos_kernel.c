/*
 * AEOS - Native kernel client library (host-side)
 *
 * Implements the symbol contract consumed by sdk/py/src/aeos_sdk/ffi.py.
 * Mirrors the on-target kernel state model: PMM page accounting, the
 * generic timer, and the fixed-point MLP policy from boot/aarch64/ai.c
 * so decisions match what the real kernel computes.
 *
 * Build: make  (produces libaeos_kernel.so)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ============================================================
 * Kernel state mirror
 * ============================================================ */

#define PAGE_SIZE        4096u
#define RAM_SIZE         (128u * 1024u * 1024u)
#define DEFAULT_HZ       100u
#define HZ_MIN           10u
#define HZ_MAX           500u
#define AI_PERIOD_TICKS  50u

static uint32_t k_total_pages = RAM_SIZE / PAGE_SIZE;   /* 32768 */
static uint32_t k_free_pages  = 32768 - 4608;           /* reserved */
static uint64_t k_ticks;
static uint32_t k_hz = DEFAULT_HZ;
static uint32_t k_inferences;
static uint32_t k_reads_since_inference;
static const char *k_last_action = "SUSTAIN";

/* ============================================================
 * Fixed-point helpers (Q16.16) - identical to boot/aarch64/ai.c
 * ============================================================ */

#define FP_ONE   ((int32_t)65536)

static int32_t fp_mul(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static int32_t fp_from_ratio(uint32_t num, uint32_t den)
{
    if (den == 0)
        return 0;
    return (int32_t)(((int64_t)num << 16) / den);
}

static int32_t fp_softsign(int32_t x)
{
    int32_t denom = FP_ONE + (x < 0 ? -x : x);
    return (int32_t)(((int64_t)x << 16) / denom);
}

static int32_t fp_clamp(int32_t x, int32_t lo, int32_t hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* ============================================================
 * Policy network (3-8-3 MLP) - same weights as ai.c
 * ============================================================ */

#define AI_INPUTS  3
#define AI_HIDDEN  8
#define AI_OUTPUTS 3

enum { ACT_SUSTAIN = 0, ACT_BOOST = 1, ACT_REST = 2 };

static const char *ai_action_names[AI_OUTPUTS] = {
    "SUSTAIN", "BOOST", "REST"
};

static const int32_t w1[AI_HIDDEN][AI_INPUTS] = {
    {        0,        0,  (2 << 16) },
    {        0,        0, -(2 << 16) },
    { (2 << 16),        0,        0 },
    {        0, (2 << 16),        0 },
    { (1 << 15), (1 << 15),        0 },
    { -(1 << 16), -(1 << 16), -(1 << 16) },
    { (1 << 16),        0,  (1 << 16) },
    {        0,        0,        0 },
};

static const int32_t b1[AI_HIDDEN] = {
    0, 0, -(1 << 16), -(1 << 16), 0, (1 << 15), 0, 0
};

static const int32_t w2[AI_OUTPUTS][AI_HIDDEN] = {
    { -(1 << 14), -(1 << 14), -(1 << 14), -(1 << 14),
       (1 << 15),  (1 << 15), -(1 << 14),  (1 << 15) },
    {  (2 << 16), -(2 << 16),        0,        0,
      -(1 << 15), -(1 << 15),  (2 << 16),        0 },
    {        0,  (2 << 16),  (2 << 16),  (2 << 16),
       (1 << 15),        0, -(1 << 15),        0 },
};

static const int32_t b2[AI_OUTPUTS] = { 0, 0, 0 };

static void ai_perceive(int32_t *x)
{
    uint32_t window = AI_PERIOD_TICKS;

    x[0] = fp_from_ratio(k_total_pages - k_free_pages, k_total_pages);
    x[1] = fp_from_ratio(k_total_pages - k_free_pages, k_total_pages);
    x[2] = fp_clamp(fp_from_ratio(window, AI_PERIOD_TICKS) - FP_ONE,
                    -(2 * FP_ONE), 2 * FP_ONE);
}

static int ai_decide(const int32_t *x)
{
    int32_t hidden[AI_HIDDEN];
    int32_t out[AI_OUTPUTS];
    int i, j, best = 0;

    for (i = 0; i < AI_HIDDEN; i++) {
        int32_t s = b1[i];
        s += fp_mul(w1[i][0], x[0]);
        s += fp_mul(w1[i][1], x[1]);
        s += fp_mul(w1[i][2], x[2]);
        hidden[i] = fp_softsign(s);
    }

    for (j = 0; j < AI_OUTPUTS; j++) {
        out[j] = b2[j];
        for (i = 0; i < AI_HIDDEN; i++)
            out[j] += fp_mul(w2[j][i], hidden[i]);
        /* Clamp outputs to prevent extreme values (matches kernel behavior) */
        out[j] = fp_clamp(out[j], -(4 * FP_ONE), 4 * FP_ONE);
        if (out[j] > out[best])
            best = j;
    }
    return best;
}

static void ai_act(int action)
{
    switch (action) {
    case ACT_BOOST:
        if (k_hz + 25u <= HZ_MAX)
            k_hz += 25u;
        break;
    case ACT_REST:
        if (k_hz >= 25u + HZ_MIN)
            k_hz -= 25u;
        break;
    default:
        break;
    }
}

/* Run one inference cycle (called every AI_PERIOD_TICKS reads). */
static void ai_step(void)
{
    int32_t x[AI_INPUTS];
    int action;

    ai_perceive(x);
    action = ai_decide(x);
    ai_act(action);
    k_last_action = ai_action_names[action];
    k_inferences++;
}

/* ============================================================
 * Exported symbol contract (see sdk/py/src/aeos_sdk/ffi.py)
 * ============================================================ */

uint32_t aeos_memory_total_pages(void)
{
    return k_total_pages;
}

uint32_t aeos_memory_free_pages(void)
{
    return k_free_pages;
}

uint64_t aeos_timer_ticks(void)
{
    /* Each read advances simulated time by one policy period;
     * the AI core infers every AI_PERIOD_TICKS reads, mirroring
     * the on-target 50-tick cadence. */
    k_ticks += k_hz;
    if (++k_reads_since_inference >= AI_PERIOD_TICKS) {
        k_reads_since_inference = 0;
        ai_step();
    }
    return k_ticks;
}

uint32_t aeos_timer_hz(void)
{
    return k_hz;
}

uint32_t aeos_ai_inference_count(void)
{
    return k_inferences;
}

const char *aeos_ai_last_action(void)
{
    return k_last_action;
}

void aeos_set_timer_hz(uint32_t hz)
{
    if (hz < HZ_MIN)
        hz = HZ_MIN;
    if (hz > HZ_MAX)
        hz = HZ_MAX;
    k_hz = hz;
}

/* Test/simulation hooks (not part of the Python contract). */

void aeos_sim_allocate_pages(uint32_t count)
{
    if (count <= k_free_pages)
        k_free_pages -= count;
}

void aeos_sim_reset(void)
{
    k_free_pages = k_total_pages - 4608;
    k_ticks = 0;
    k_hz = DEFAULT_HZ;
    k_inferences = 0;
    k_reads_since_inference = 0;
    k_last_action = "SUSTAIN";
}

/* ============================================================
 * AI control API (new SDK integration)
 * ============================================================ */

/* Decision history ring buffer (mirrors kernel/ai.c) */
#define AI_HIST_LEN 16

typedef struct {
    uint64_t tick;
    uint8_t  action;
} aeos_hist_entry_t;

static aeos_hist_entry_t ai_history[AI_HIST_LEN];
static uint32_t ai_hist_count;

static int ai_disabled_flag;

void aeos_ai_force_inference(void)
{
    if (ai_disabled_flag)
        return;
    /* Force a perception cycle by feeding extreme inputs */
    int32_t x[AI_INPUTS];
    x[0] = fp_from_ratio(k_total_pages - k_free_pages, k_total_pages);
    x[1] = fp_from_ratio(k_total_pages - k_free_pages, k_total_pages);
    x[2] = (int32_t)(2 * FP_ONE);  /* high activity */
    int action = ai_decide(x);
    ai_act(action);
    k_last_action = ai_action_names[action];
    k_inferences++;

    /* Record in history */
    ai_history[ai_hist_count % AI_HIST_LEN].tick = k_ticks;
    ai_history[ai_hist_count % AI_HIST_LEN].action = (uint8_t)action;
    ai_hist_count++;
}

int aeos_ai_get_history_count(void)
{
    uint32_t n = ai_hist_count;
    return (n < AI_HIST_LEN) ? (int)n : AI_HIST_LEN;
}

uint64_t aeos_ai_get_history_tick(int idx)
{
    int total = aeos_ai_get_history_count();
    if (idx < 0 || idx >= total)
        return 0;
    uint32_t start = (ai_hist_count < AI_HIST_LEN)
                     ? 0 : ai_hist_count - AI_HIST_LEN;
    return ai_history[(start + idx) % AI_HIST_LEN].tick;
}

int aeos_ai_get_history_action(int idx)
{
    int total = aeos_ai_get_history_count();
    if (idx < 0 || idx >= total)
        return -1;
    uint32_t start = (ai_hist_count < AI_HIST_LEN)
                     ? 0 : ai_hist_count - AI_HIST_LEN;
    return (int)ai_history[(start + idx) % AI_HIST_LEN].action;
}

int aeos_ai_is_healthy(void)
{
    return !ai_disabled_flag;
}

void aeos_ai_disable(void)
{
    ai_disabled_flag = 1;
}

void aeos_ai_enable(void)
{
    ai_disabled_flag = 0;
}
