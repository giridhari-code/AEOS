/*
 * AEOS - CMT Engine Core (MLP + Perceive-Transform-Experience)
 */
#include "cmt.h"
#include "memory.h"
#include "uart.h"
#include "fp.h"

#define CMT_MAGIC 0x434D5430u

static const char *cmt_action_names[CMT_OUTPUTS] = { "SUSTAIN", "BOOST", "REST" };

cmt_engine_t cmt;

/* 6-12-3 Network Weights */
static const int32_t w1[CMT_HIDDEN][CMT_INPUTS] = {
    { (2<<16), (1<<15),        0,        0,        0,        0 },
    {-(2<<16),-(1<<15),        0,        0,        0,        0 },
    { (1<<15), (2<<16),        0,        0,        0,        0 },
    {        0,        0, (2<<16),        0,        0,        0 },
    {        0,        0,        0, (2<<16),        0,        0 },
    {        0,        0,        0,        0, (2<<16),        0 },
    {        0,        0,        0,        0,        0, (2<<16) },
    { (1<<15), (1<<15),        0,        0,        0,        0 },
    {        0,        0, (1<<15), (1<<15),        0,        0 },
    {        0,        0,        0,        0, (1<<15), (1<<15) },
    {-(1<<16),-(1<<16),-(1<<16),        0,        0,        0 },
    {        0,        0,        0,        0,        0,        0 },
};
static const int32_t b1[CMT_HIDDEN] = {
    0, 0, -(1<<16), 0, 0, 0, 0, (1<<15), 0, 0, (1<<15), 0
};
static const int32_t w2[CMT_OUTPUTS][CMT_HIDDEN] = {
    { -(1<<14),-(1<<14),-(1<<14),-(1<<14),-(1<<14),-(1<<14),
      -(1<<14), (1<<15), (1<<15), (1<<15), (1<<15), (1<<15) },
    {  (2<<16),-(2<<16),       0, (2<<16),       0,       0,
             0,-(1<<15),-(1<<15),       0,-(1<<16),       0 },
    {        0, (2<<16), (2<<16),-(2<<16),-(2<<16),-(2<<16),
      -(2<<16),       0,       0,-(1<<15), (2<<16),       0 },
};
static const int32_t b2[CMT_OUTPUTS] = { 0, 0, 0 };

void cmt_perceive(cmt_input_t *in) {
    uint32_t hu = heap_get_used(), hc = heap_get_capacity();
    uint32_t pf = pmm_get_free_pages(), pt = pmm_get_total_pages();

    int32_t ph = fp_from_ratio(hu, hc);
    int32_t pp = fp_from_ratio(pt - pf, pt);
    in->p = (ph + pp) / 2;

    in->m = fp_clamp(FP_ONE - fp_from_ratio(pf, pt), 0, FP_ONE);

    in->b = fp_clamp(
        fp_from_ratio(cmt.ticks_since_infer, CMT_PERIOD) - FP_ONE,
        -(2*FP_ONE), 2*FP_ONE
    );

    uint32_t ai_tot = ai_inference_count() + ai_error_count();
    in->s = (ai_tot > 0) ? fp_from_ratio(ai_inference_count(), ai_tot) : FP_ONE;

    in->a = (int32_t)(ai_last_action()) << 16;

    in->x = fp_clamp(fp_from_ratio(cmt.state.uptime % 1000, 1000), 0, FP_ONE);
}

int cmt_transform(const cmt_input_t *in, cmt_experience_t *exp) {
    int32_t hidden[CMT_HIDDEN], out[CMT_OUTPUTS];
    int i, j, best = 0;
    int32_t te = 0;
    int32_t inp[CMT_INPUTS] = { in->p, in->m, in->b, in->s, in->a, in->x };

    for (i = 0; i < CMT_HIDDEN; i++) {
        int32_t s = b1[i];
        for (j = 0; j < CMT_INPUTS; j++)
            s += fp_mul(w1[i][j], inp[j]);
        hidden[i] = fp_softsign(s);
        te += fp_abs(hidden[i]);
    }

    for (j = 0; j < CMT_OUTPUTS; j++) {
        out[j] = b2[j];
        for (i = 0; i < CMT_HIDDEN; i++)
            out[j] += fp_mul(w2[j][i], hidden[i]);
        if (out[j] > out[best]) best = j;
    }

    for (i = 0; i < CMT_HIDDEN; i++)
        exp->dims[i] = hidden[i];
    exp->energy = te / CMT_HIDDEN;
    exp->coherence = (te > 0)
        ? fp_clamp(te / (fp_abs(hidden[0]) + 1), 0, FP_ONE)
        : 0;

    return best;
}

uint8_t cmt_derive_level(const cmt_experience_t *exp) {
    if (exp->energy <= 0) return 0;
    if (exp->energy < (1 << 15)) return 1;
    if (exp->energy < (1 << 16) || exp->coherence < (1 << 15)) return 2;
    return 3;
}

void cmt_record(uint32_t tick, int action, uint8_t level, int32_t energy) {
    if (cmt.hist_count < CMT_HIST_LEN) {
        cmt.history[cmt.hist_count].tick = tick;
        cmt.history[cmt.hist_count].action = (uint8_t)action;
        cmt.history[cmt.hist_count].level = level;
        cmt.history[cmt.hist_count].energy = energy;
        cmt.hist_count++;
    }
}

void cmt_on_tick(void) {
    if (cmt.state.disabled) return;

    cmt.state.uptime++;
    cmt.ticks_total++;
    cmt.ticks_since_infer++;

    if (cmt.ticks_since_infer < CMT_PERIOD) return;

    cmt.ticks_since_infer = 0;
    cmt.state.inferences++;

    cmt_input_t in;
    cmt_perceive(&in);
    cmt.last_input = in;

    int action = cmt_transform(&in, &cmt.experience);

    cmt.state.level = cmt_derive_level(&cmt.experience);
    cmt.state.action = (uint8_t)action;

    cmt_record(cmt.ticks_total, action, cmt.state.level, cmt.experience.energy);

    if (cmt.state.verbose) {
        uart_puts_nolf("  CMT: C=");
        uart_dec(cmt.state.level);
        uart_puts_nolf(" E=");
        uart_dec(cmt.experience.energy);
        uart_puts_nolf(" A=");
        uart_puts_nolf(cmt_action_names[action]);
        uart_puts_nolf(" H=");
        uart_dec(cmt.experience.coherence);
        uart_puts("");
    }
}

void cmt_force_inference(void) {
    cmt.ticks_since_infer = CMT_PERIOD;
}

const char *cmt_core_action_name(int action) {
    if (action >= 0 && action < CMT_OUTPUTS) return cmt_action_names[action];
    return "UNKNOWN";
}
