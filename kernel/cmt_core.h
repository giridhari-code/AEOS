/*
 * AEOS - CMT Engine Core Header
 */
#ifndef CMT_CORE_H
#define CMT_CORE_H

#include "cmt.h"
#include "fp.h"

extern cmt_engine_t cmt;

void     cmt_perceive(cmt_input_t *in);
int      cmt_transform(const cmt_input_t *in, cmt_experience_t *exp);
uint8_t  cmt_derive_level(const cmt_experience_t *exp);
void     cmt_record(uint32_t tick, int action, uint8_t level, int32_t energy);
const char *cmt_core_action_name(int action);

#endif /* CMT_CORE_H */
