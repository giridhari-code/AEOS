/*
 * AEOS - AI Engine Core (MLP + Perceive-Decide-Act)
 */
#include "ai_core.h"
#include "memory.h"
#include "cmt.h"

static const char *ai_action_names[AI_OUTPUTS] = {
    "SUSTAIN", "BOOST", "REST"
};

/* Hidden layer weights [neuron][input], Q16.16 */
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

/* Output layer weights [action][neuron], Q16.16 */
static const int32_t w2[AI_OUTPUTS][AI_HIDDEN] = {
    { -(1 << 14), -(1 << 14), -(1 << 14), -(1 << 14),
      (1 << 15),  (1 << 15), -(1 << 14),  (1 << 15) },
    {  (2 << 16), -(2 << 16),        0,        0,
      -(1 << 15), -(1 << 15),  (2 << 16),        0 },
    {        0,  (2 << 16),  (2 << 16),  (2 << 16),
      (1 << 15),        0, -(1 << 15),        0 },
};
static const int32_t b2[AI_OUTPUTS] = { 0, 0, 0 };

void ai_perceive(int32_t *x, uint32_t ticks_since, uint32_t heap_used, uint32_t heap_cap, uint32_t pages_free, uint32_t pages_total) {
    x[0] = fp_from_ratio(heap_used, heap_cap);
    x[1] = fp_from_ratio(pages_total - pages_free, pages_total);
    x[2] = fp_clamp(fp_from_ratio(ticks_since, AI_PERIOD_TICKS) - FP_ONE, -(2*FP_ONE), 2*FP_ONE);
}

int ai_decide(const int32_t *x) {
    int32_t hidden[AI_HIDDEN], out[AI_OUTPUTS];
    int i, j, best = 0;

    for (i = 0; i < AI_HIDDEN; i++) {
        int32_t s = b1[i];
        for (j = 0; j < AI_INPUTS; j++)
            s += fp_mul(w1[i][j], x[j]);
        hidden[i] = fp_softsign(s);
    }

    for (j = 0; j < AI_OUTPUTS; j++) {
        out[j] = b2[j];
        for (i = 0; i < AI_HIDDEN; i++)
            out[j] += fp_mul(w2[j][i], hidden[i]);
        out[j] = fp_clamp(out[j], -(4*FP_ONE), 4*FP_ONE);
        if (out[j] > out[best]) best = j;
    }
    return best;
}

int ai_act(int action, int cmt_level, uint32_t *hz) {
    uint32_t hz_step = (uint32_t)AI_HZ_STEP;
    uint32_t hz_min = (uint32_t)AI_HZ_MIN;
    uint32_t hz_max = (uint32_t)AI_HZ_MAX;

    if (cmt_level <= 1) {
        hz_step = (uint32_t)(AI_HZ_STEP / 2);
        if (hz_step < 1) hz_step = 1;
        hz_min = 80; hz_max = 120;
    } else if (cmt_level >= 3) {
        hz_step = (uint32_t)(AI_HZ_STEP * 2);
        hz_min = 50; hz_max = 200;
    }

    switch (action) {
    case AI_ACT_BOOST:
        if (*hz + hz_step <= hz_max) *hz += hz_step;
        break;
    case AI_ACT_REST:
        if (*hz >= hz_min + hz_step) *hz -= hz_step;
        break;
    default: break;
    }
    return 0;
}

const char *ai_core_action_name(int action) {
    if (action >= 0 && action < AI_OUTPUTS) return ai_action_names[action];
    return "UNKNOWN";
}
