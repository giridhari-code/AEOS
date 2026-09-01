"""Contract tests for the mock kernel's KernelPort implementation."""

import pytest

from aeos_sdk.mock import MockKernel
from aeos_sdk.ports import KernelPort
from aeos_sdk.types import MemoryStats, SystemVitals


@pytest.fixture
def kernel() -> MockKernel:
    return MockKernel(total_pages=1000, reserved_pages=100, timer_hz=100)


def test_mock_satisfies_kernel_port(kernel: MockKernel) -> None:
    assert isinstance(kernel, KernelPort)


def test_memory_stats_accounting(kernel: MockKernel) -> None:
    stats = kernel.memory_stats()
    assert stats == MemoryStats(total_pages=1000, free_pages=900)
    assert stats.used_pages == 100
    assert stats.pressure == pytest.approx(0.1)


def test_allocate_pages_updates_pressure(kernel: MockKernel) -> None:
    kernel.allocate_pages(400)
    assert kernel.memory_stats().pressure == pytest.approx(0.5)


def test_allocate_pages_rejects_exhaustion(kernel: MockKernel) -> None:
    with pytest.raises(ValueError, match="cannot allocate"):
        kernel.allocate_pages(901)


def test_timer_ticks_advance_monotonically(kernel: MockKernel) -> None:
    first = kernel.timer_info()
    second = kernel.timer_info()
    assert second.ticks == first.ticks + 1
    assert second.hz == 100
    assert second.seconds == pytest.approx(second.ticks / 100)


@pytest.mark.parametrize("bad_hz", [0, 9, 501, 10_000])
def test_set_timer_hz_validates_range(kernel: MockKernel, bad_hz: int) -> None:
    with pytest.raises(ValueError, match="within"):
        kernel.set_timer_hz(bad_hz)


def test_set_timer_hz_accepts_valid_range(kernel: MockKernel) -> None:
    kernel.set_timer_hz(250)
    assert kernel.timer_info().hz == 250


def test_ai_inference_tracking(kernel: MockKernel) -> None:
    assert kernel.ai_inference_count() == 0
    kernel.fire_ai_inference("BOOST")
    kernel.fire_ai_inference("REST")
    assert kernel.ai_inference_count() == 2
    assert kernel.ai_last_action() == "REST"


def test_vitals_aggregates_everything(kernel: MockKernel) -> None:
    kernel.allocate_pages(500)
    kernel.set_timer_hz(200)
    kernel.fire_ai_inference("BOOST")
    vitals = kernel.vitals()
    assert isinstance(vitals, SystemVitals)
    assert vitals.memory.free_pages == 400
    assert vitals.ai_policy_hz == 200
    assert vitals.ai_last_action == "BOOST"
