"""Tests for the AI core module (Q16.16 fixed-point MLP)."""

import pytest

from aeos_sdk.ai_core import (
    AI_HIST_LEN,
    AI_HZ_MAX,
    AI_HZ_MIN,
    AI_HZ_STEP,
    ACTION_BOOST,
    ACTION_REST,
    ACTION_SUSTAIN,
    FP_ONE,
    AIDecision,
    act,
    decide,
    fp_clamp,
    fp_from_ratio,
    fp_mul,
    fp_softsign,
    perceive,
)


class TestFixedPoint:
    def test_fp_mul_identity(self) -> None:
        assert 1 * FP_ONE == FP_ONE

    def test_fp_mul_half(self) -> None:
        assert fp_mul(FP_ONE, FP_ONE) == FP_ONE

    def test_fp_from_ratio_half(self) -> None:
        assert fp_from_ratio(1, 2) == FP_ONE // 2

    def test_fp_from_ratio_zero_denom(self) -> None:
        assert fp_from_ratio(100, 0) == 0

    def test_fp_softsign_bounded(self) -> None:
        val = fp_softsign(100 * FP_ONE)
        assert -FP_ONE < val < FP_ONE

    def test_fp_clamp(self) -> None:
        assert fp_clamp(5, 0, 10) == 5
        assert fp_clamp(-1, 0, 10) == 0
        assert fp_clamp(15, 0, 10) == 10


class TestPerceive:
    def test_perceive_zero_pressure(self) -> None:
        x = perceive(0, 100, 100, 100, 0)
        assert x[0] == 0  # no heap pressure
        assert x[1] == 0  # no page pressure

    def test_perceive_full_pressure(self) -> None:
        x = perceive(100, 100, 0, 100, 0)
        assert x[0] == FP_ONE  # full heap pressure
        assert x[1] == FP_ONE  # full page pressure

    def test_perceive_activity_trend(self) -> None:
        x = perceive(0, 100, 100, 100, 0)
        # ticks_since_infer=0, period=50 -> ratio=0, minus FP_ONE = -FP_ONE
        assert x[2] == -FP_ONE


class TestDecide:
    def test_decide_returns_valid_action(self) -> None:
        _, best = decide((0, 0, 0))
        assert best in (ACTION_SUSTAIN, ACTION_BOOST, ACTION_REST)

    def test_decide_hidden_size(self) -> None:
        hidden, _ = decide((FP_ONE // 2, FP_ONE // 2, 0))
        assert len(hidden) == 8

    def test_decide_deterministic(self) -> None:
        h1, a1 = decide((FP_ONE // 4, FP_ONE // 4, FP_ONE // 2))
        h2, a2 = decide((FP_ONE // 4, FP_ONE // 4, FP_ONE // 2))
        assert a1 == a2
        assert h1 == h2


class TestAct:
    def test_boost_increases_hz(self) -> None:
        assert act(100, ACTION_BOOST) == 100 + AI_HZ_STEP

    def test_rest_decreases_hz(self) -> None:
        assert act(100, ACTION_REST) == 100 - AI_HZ_STEP

    def test_sustain_no_change(self) -> None:
        assert act(100, ACTION_SUSTAIN) == 100

    def test_boost_clamps_at_max(self) -> None:
        assert act(AI_HZ_MAX, ACTION_BOOST) == AI_HZ_MAX

    def test_rest_clamps_at_min(self) -> None:
        assert act(AI_HZ_MIN, ACTION_REST) == AI_HZ_MIN
