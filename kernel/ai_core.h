/*
 * AEOS - AI Engine Core Header
 */
#ifndef AI_CORE_H
#define AI_CORE_H

#include "fp.h"
#include "ai.h"

void     ai_perceive(int32_t *x, uint32_t ticks_since, uint32_t heap_used, uint32_t heap_cap, uint32_t pages_free, uint32_t pages_total);
int      ai_decide(const int32_t *x);
int      ai_act(int action, int cmt_level, uint32_t *hz);
const char *ai_core_action_name(int action);

#endif /* AI_CORE_H */
