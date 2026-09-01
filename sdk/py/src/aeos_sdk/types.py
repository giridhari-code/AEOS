"""Value types crossing the kernel boundary."""

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class MemoryStats:
    """Physical memory accounting, in 4KiB pages."""

    total_pages: int
    free_pages: int

    @property
    def used_pages(self) -> int:
        return self.total_pages - self.free_pages

    @property
    def pressure(self) -> float:
        """Used fraction in [0.0, 1.0]."""
        if self.total_pages == 0:
            return 0.0
        return self.used_pages / self.total_pages


@dataclass(frozen=True, slots=True)
class TimerInfo:
    """Kernel timer state."""

    ticks: int
    hz: int

    @property
    def seconds(self) -> float:
        return self.ticks / self.hz if self.hz else 0.0


@dataclass(frozen=True, slots=True)
class SystemVitals:
    """Aggregate vitals as perceived by the in-kernel AI core."""

    memory: MemoryStats
    timer: TimerInfo
    ai_inferences: int
    ai_last_action: str
    ai_policy_hz: int
