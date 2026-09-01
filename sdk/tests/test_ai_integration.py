"""Tests for AI integration in MockKernel."""

import pytest

from aeos_sdk.mock import MockKernel
from aeos_sdk.types import SystemVitals


@pytest.fixture
def kernel() -> MockKernel:
    return MockKernel(total_pages=1000, reserved_pages=100, timer_hz=100)


class TestMockKernelAIInference:
    def test_inference_runs_on_tick(self, kernel: MockKernel) -> None:
        """AI inference fires after AI_PERIOD_TICKS (50) ticks."""
        from aeos_sdk.ai_core import AI_PERIOD_TICKS

        for _ in range(AI_PERIOD_TICKS):
            kernel.timer_info()
        assert kernel.ai_inference_count() == 1

    def test_inference_runs_multiple_cycles(self, kernel: MockKernel) -> None:
        from aeos_sdk.ai_core import AI_PERIOD_TICKS

        for _ in range(AI_PERIOD_TICKS * 3):
            kernel.timer_info()
        assert kernel.ai_inference_count() == 3

    def test_force_inference_bypasses_period(self, kernel: MockKernel) -> None:
        kernel.ai_force_inference()
        assert kernel.ai_inference_count() == 1

    def test_force_inference_returns_decision(self, kernel: MockKernel) -> None:
        from aeos_sdk.ai_core import AIDecision

        decision = kernel.ai_force_inference()
        assert isinstance(decision, AIDecision)
        assert decision.action_name in ("SUSTAIN", "BOOST", "REST")
        assert len(decision.inputs) == 3
        assert len(decision.hidden) == 8

    def test_last_action_updates(self, kernel: MockKernel) -> None:
        kernel.ai_force_inference()
        assert kernel.ai_last_action() in ("SUSTAIN", "BOOST", "REST")


class TestMockKernelAIHistory:
    def test_history_starts_empty(self, kernel: MockKernel) -> None:
        assert kernel.ai_get_history() == []

    def test_history_grows(self, kernel: MockKernel) -> None:
        kernel.ai_force_inference()
        kernel.ai_force_inference()
        history = kernel.ai_get_history()
        assert len(history) == 2

    def test_history_ring_buffer(self, kernel: MockKernel) -> None:
        from aeos_sdk.ai_core import AI_HIST_LEN

        for _ in range(AI_HIST_LEN + 5):
            kernel.ai_force_inference()
        assert len(kernel.ai_get_history()) == AI_HIST_LEN

    def test_history_entries_have_tick_and_action(self, kernel: MockKernel) -> None:
        kernel.ai_force_inference()
        entry = kernel.ai_get_history()[0]
        assert hasattr(entry, "tick")
        assert hasattr(entry, "action")
        assert entry.action in (0, 1, 2)


class TestMockKernelAIHealth:
    def test_starts_healthy(self, kernel: MockKernel) -> None:
        assert kernel.ai_is_healthy() is True

    def test_disable_makes_unhealthy(self, kernel: MockKernel) -> None:
        kernel.ai_disable()
        assert kernel.ai_is_healthy() is False

    def test_enable_restores_health(self, kernel: MockKernel) -> None:
        kernel.ai_disable()
        kernel.ai_enable()
        assert kernel.ai_is_healthy() is True

    def test_disabled_skips_inference(self, kernel: MockKernel) -> None:
        kernel.ai_disable()
        from aeos_sdk.ai_core import AI_PERIOD_TICKS
        for _ in range(AI_PERIOD_TICKS * 2):
            kernel.timer_info()
        assert kernel.ai_inference_count() == 0


class TestMockKernelAIWeights:
    def test_get_weights_returns_dict(self, kernel: MockKernel) -> None:
        w = kernel.ai_get_weights()
        assert "w1" in w and "b1" in w and "w2" in w and "b2" in w

    def test_weights_match_kernel(self, kernel: MockKernel) -> None:
        from aeos_sdk.ai_core import B1, B2, W1, W2
        w = kernel.ai_get_weights()
        assert w["w1"] == W1
        assert w["b1"] == B1
        assert w["w2"] == W2
        assert w["b2"] == B2

    def test_set_weights_updates(self, kernel: MockKernel) -> None:
        new_w1 = [[0] * 3 for _ in range(8)]
        kernel.ai_set_weights(w1=new_w1)
        assert kernel.ai_get_weights()["w1"] == new_w1
        # Restore
        from aeos_sdk.ai_core import W1
        kernel.ai_set_weights(w1=W1)

    def test_set_weights_validates_shape(self, kernel: MockKernel) -> None:
        with pytest.raises(ValueError, match="w1 must be 8x3"):
            kernel.ai_set_weights(w1=[[0] * 2 for _ in range(8)])
        with pytest.raises(ValueError, match="b1 must have 8"):
            kernel.ai_set_weights(b1=[0] * 5)


class TestMockKernelAIInputs:
    def test_get_inputs_returns_tuple(self, kernel: MockKernel) -> None:
        inputs = kernel.ai_get_inputs()
        assert isinstance(inputs, tuple)
        assert len(inputs) == 3


class TestMockKernelHeapSimulation:
    def test_set_heap_used(self, kernel: MockKernel) -> None:
        kernel.set_heap_used(1024 * 1024)
        # Force inference to see if it uses heap info
        decision = kernel.ai_force_inference()
        assert isinstance(decision.action, int)
