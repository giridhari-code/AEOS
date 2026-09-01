"""Tests for the native kernel backend (libaeos_kernel).

Skipped automatically when the library has not been built.
Build it with: make -C sdk/bindings
"""

import os

import pytest

from aeos_sdk.ffi import load_native
from aeos_sdk.ports import KernelPort

pytestmark = pytest.mark.skipif(
    os.environ.get("AEOS_KERNEL_LIB") is None and load_native() is None,
    reason="libaeos_kernel.so not built (run: make -C sdk/bindings)",
)


@pytest.fixture
def native_kernel():
    from aeos_sdk.ffi import NativeKernel

    lib = load_native()
    assert lib is not None, "expected native library under test"
    return NativeKernel(lib)


def test_native_satisfies_kernel_port(native_kernel) -> None:
    assert isinstance(native_kernel, KernelPort)


def test_native_memory_accounting(native_kernel) -> None:
    stats = native_kernel.memory_stats()
    assert stats.total_pages == 32768
    assert 0 < stats.free_pages <= stats.total_pages


def test_native_timer_ticks_advance(native_kernel) -> None:
    first = native_kernel.timer_info()
    second = native_kernel.timer_info()
    assert second.ticks > first.ticks
    assert second.hz == first.hz


def test_native_set_timer_hz_validates_then_clamps(native_kernel) -> None:
    import pytest

    with pytest.raises(ValueError, match="within"):
        native_kernel.set_timer_hz(999_999)
    with pytest.raises(ValueError, match="within"):
        native_kernel.set_timer_hz(1)
    native_kernel.set_timer_hz(250)
    assert native_kernel.timer_info().hz == 250
    native_kernel.set_timer_hz(100)


def test_native_ai_cycle_fires_every_period(native_kernel) -> None:
    before = native_kernel.ai_inference_count()
    for _ in range(50):
        native_kernel.timer_info()
    assert native_kernel.ai_inference_count() == before + 1
    assert native_kernel.ai_last_action() in {"SUSTAIN", "BOOST", "REST"}


def test_native_vitals_aggregate(native_kernel) -> None:
    vitals = native_kernel.vitals()
    assert vitals.memory.total_pages == 32768
    assert vitals.ai_last_action in {"SUSTAIN", "BOOST", "REST"}
