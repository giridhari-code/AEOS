"""In-kernel AI engine (Q16.16 fixed-point MLP).

Faithful Python mirror of kernel/ai.c: same 3-8-3 network, same weights,
same perceive-decide-act cycle. Used by MockKernel for simulation-first
development and by the SDK for offline inference testing.
"""

from __future__ import annotations

from dataclasses import dataclass

FP_ONE = 65536
AI_INPUTS = 3
AI_HIDDEN = 8
AI_OUTPUTS = 3
AI_PERIOD_TICKS = 50
AI_HZ_MIN = 20
AI_HZ_MAX = 200
AI_HZ_STEP = 25
AI_HIST_LEN = 16

ACTION_SUSTAIN = 0
ACTION_BOOST = 1
ACTION_REST = 2
ACTION_NAMES = ("SUSTAIN", "BOOST", "REST")


def fp_mul(a: int, b: int) -> int:
    return (a * b) >> 16


def fp_from_ratio(num: int, den: int) -> int:
    if den == 0:
        return 0
    return (num << 16) // den


def fp_softsign(x: int) -> int:
    denom = FP_ONE + abs(x)
    return (x << 16) // denom


def fp_clamp(x: int, lo: int, hi: int) -> int:
    return max(lo, min(hi, x))


# Hidden layer weights [neuron][input], Q16.16
W1: list[list[int]] = [
    [       0,        0,  (2 << 16)],
    [       0,        0, -(2 << 16)],
    [(2 << 16),        0,        0 ],
    [       0, (2 << 16),        0 ],
    [(1 << 15), (1 << 15),        0 ],
    [-(1 << 16), -(1 << 16), -(1 << 16)],
    [(1 << 16),        0,  (1 << 16)],
    [       0,        0,        0 ],
]

B1: list[int] = [0, 0, -(1 << 16), -(1 << 16), 0, (1 << 15), 0, 0]

# Output layer weights [action][neuron], Q16.16
W2: list[list[int]] = [
    [-(1 << 14), -(1 << 14), -(1 << 14), -(1 << 14),
     (1 << 15),  (1 << 15),  -(1 << 14),  (1 << 15)],
    [(2 << 16), -(2 << 16),        0,        0,
     -(1 << 15), -(1 << 15), (2 << 16),        0],
    [       0,  (2 << 16),  (2 << 16),  (2 << 16),
     (1 << 15),        0, -(1 << 15),        0],
]

B2: list[int] = [0, 0, 0]


@dataclass(frozen=True)
class HistEntry:
    tick: int
    action: int


@dataclass
class AIDecision:
    inputs: tuple[int, int, int]
    hidden: tuple[int, ...]
    outputs: tuple[int, ...]
    action: int
    action_name: str
    hz: int


def perceive(
    heap_used: int,
    heap_cap: int,
    pages_free: int,
    pages_total: int,
    ticks_since_infer: int,
) -> tuple[int, int, int]:
    x0 = fp_from_ratio(heap_used, heap_cap)
    x1 = fp_from_ratio(pages_total - pages_free, pages_total)
    x2 = fp_clamp(
        fp_from_ratio(ticks_since_infer, AI_PERIOD_TICKS) - FP_ONE,
        -(2 * FP_ONE), 2 * FP_ONE,
    )
    return x0, x1, x2


def decide(x: tuple[int, int, int]) -> tuple[tuple[int, ...], int]:
    hidden = [0] * AI_HIDDEN
    for i in range(AI_HIDDEN):
        s = B1[i]
        s += fp_mul(W1[i][0], x[0])
        s += fp_mul(W1[i][1], x[1])
        s += fp_mul(W1[i][2], x[2])
        hidden[i] = fp_softsign(s)

    out = [0] * AI_OUTPUTS
    best = 0
    for j in range(AI_OUTPUTS):
        o = B2[j]
        for i in range(AI_HIDDEN):
            o += fp_mul(W2[j][i], hidden[i])
        out[j] = fp_clamp(o, -(4 * FP_ONE), 4 * FP_ONE)
        if out[j] > out[best]:
            best = j

    return tuple(hidden), best


def act(current_hz: int, action: int) -> int:
    if action == ACTION_BOOST:
        if current_hz + AI_HZ_STEP <= AI_HZ_MAX:
            return current_hz + AI_HZ_STEP
    elif action == ACTION_REST:
        if current_hz >= AI_HZ_MIN + AI_HZ_STEP:
            return current_hz - AI_HZ_STEP
    return current_hz
