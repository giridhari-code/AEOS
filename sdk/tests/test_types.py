"""Tests for SDK value types."""

import pytest

from aeos_sdk.types import MemoryStats, TimerInfo


def test_memory_pressure_zero_division_guard() -> None:
    empty = MemoryStats(total_pages=0, free_pages=0)
    assert empty.pressure == 0.0


def test_timer_seconds_zero_hz_guard() -> None:
    stalled = TimerInfo(ticks=100, hz=0)
    assert stalled.seconds == 0.0


def test_types_are_immutable() -> None:
    stats = MemoryStats(total_pages=10, free_pages=5)
    with pytest.raises(AttributeError):
        stats.total_pages = 99  # type: ignore[misc]
