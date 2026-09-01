"""Kernel port - the protocol every kernel backend must satisfy.

This is the hexagonal boundary: AI subsystems depend on this
Protocol only, never on a concrete transport.
"""

from __future__ import annotations

from typing import Protocol, runtime_checkable

from aeos_sdk.types import MemoryStats, SystemVitals, TimerInfo


@runtime_checkable
class KernelPort(Protocol):
    """Operations the kernel exposes to the AI runtime."""

    def memory_stats(self) -> MemoryStats:
        """Snapshot of physical memory accounting."""
        ...

    def timer_info(self) -> TimerInfo:
        """Current timer tick count and policy rate."""
        ...

    def ai_inference_count(self) -> int:
        """How many times the in-kernel AI core has inferred."""
        ...

    def ai_last_action(self) -> str:
        """Name of the last action chosen by the AI core."""
        ...

    def set_timer_hz(self, hz: int) -> None:
        """Request a timer policy change (capability-checked)."""
        ...

    def vitals(self) -> SystemVitals:
        """Convenience aggregate of all vitals."""
        ...

    # -- AI control (new) -------------------------------------------

    def ai_force_inference(self) -> object:
        """Force an immediate AI inference cycle (bypass period)."""
        ...

    def ai_get_history(self) -> list:
        """Return the decision history ring buffer."""
        ...

    def ai_get_weights(self) -> dict:
        """Return current model weights."""
        ...

    def ai_set_weights(
        self,
        w1: list | None = None,
        b1: list | None = None,
        w2: list | None = None,
        b2: list | None = None,
    ) -> None:
        """Update model weights (runtime reconfiguration)."""
        ...

    def ai_get_inputs(self) -> tuple:
        """Return current perceive inputs."""
        ...

    def ai_is_healthy(self) -> bool:
        """Whether the AI engine is operational."""
        ...
