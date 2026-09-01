/*
 * AEOS - Fixed-point helpers (Q16.16)
 * Shared by AI and CMT engines
 */
#ifndef FP_H
#define FP_H

#include <stdint.h>

#define FP_ONE ((int32_t)65536)

static inline int32_t fp_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static inline int32_t fp_from_ratio(uint32_t num, uint32_t den) {
    if (den == 0) return 0;
    return (int32_t)(((int64_t)num << 16) / den);
}

static inline int32_t fp_softsign(int32_t x) {
    /* Handle INT32_MIN safely: use unsigned to avoid UB on negation */
    uint32_t abs_x = (x < 0) ? (uint32_t)(-(x + 1)) + 1u : (uint32_t)x;
    int32_t d = FP_ONE + (int32_t)abs_x;
    return (int32_t)(((int64_t)x << 16) / d);
}

static inline int32_t fp_clamp(int32_t x, int32_t lo, int32_t hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline int32_t fp_abs(int32_t x) {
    /* Handle INT32_MIN safely: use unsigned to avoid UB on negation */
    if (x == INT32_MIN) return INT32_MAX;  /* saturate */
    return x < 0 ? -x : x;
}

#endif /* FP_H */
