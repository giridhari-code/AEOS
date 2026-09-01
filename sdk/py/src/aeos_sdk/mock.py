"""In-process mock kernel with real AI simulation.

Simulation is a first-class hardware provider: agent code is written
against KernelPort and runs unchanged against this mock today and the
native kernel tomorrow.

The mock now runs the same 3-8-3 MLP as the in-kernel AI engine,
providing faithful simulation-first development.
"""

from __future__ import annotations

import itertools
from dataclasses import dataclass, field

from aeos_sdk.ai_core import (
    AI_HIST_LEN,
    AI_HZ_MAX,
    AI_HZ_MIN,
    AI_HZ_STEP,
    AI_PERIOD_TICKS,
    ACTION_NAMES,
    AIDecision,
    HistEntry,
    act,
    decide,
    perceive,
)
from aeos_sdk.types import MemoryStats, SystemVitals, TimerInfo


class MockKernel:
    """Deterministic in-process KernelPort with real AI simulation."""

    def __init__(
        self,
        total_pages: int = 32768,
        reserved_pages: int = 4608,
        timer_hz: int = 100,
    ) -> None:
        if total_pages <= 0:
            raise ValueError("total_pages must be positive")
        if not (10 <= timer_hz <= 500):
            raise ValueError("timer_hz must be within [10, 500]")
        self._total = total_pages
        self._free = max(0, total_pages - reserved_pages)
        self._hz = timer_hz
        self._ticks = 0
        self._inferences = 0
        self._ticks_since_infer = 0
        self._last_action = "SUSTAIN"
        self._last_action_id = 0
        self._tick_source = itertools.count(step=1)

        # AI engine state
        self._ai_disabled = False
        self._ai_errors = 0
        self._ai_history: list[HistEntry] = []
        self._ai_last_decision: AIDecision | None = None

        # Simulation knobs
        self._heap_used = 0
        self._heap_cap = 16 * 1024 * 1024  # 16MB default

    # -- KernelPort -------------------------------------------------

    def memory_stats(self) -> MemoryStats:
        return MemoryStats(total_pages=self._total, free_pages=self._free)

    def timer_info(self) -> TimerInfo:
        self._ticks = next(self._tick_source)
        self._tick()
        return TimerInfo(ticks=self._ticks, hz=self._hz)

    def ai_inference_count(self) -> int:
        return self._inferences

    def ai_last_action(self) -> str:
        return self._last_action

    def set_timer_hz(self, hz: int) -> None:
        if not 10 <= hz <= 500:
            raise ValueError("timer rate must be within [10, 500] Hz")
        self._hz = hz

    def vitals(self) -> SystemVitals:
        return SystemVitals(
            memory=self.memory_stats(),
            timer=self.timer_info(),
            ai_inferences=self._inferences,
            ai_last_action=self._last_action,
            ai_policy_hz=self._hz,
        )

    # -- AI engine --------------------------------------------------

    def _tick(self) -> None:
        """Simulate one timer tick: run AI inference every period."""
        if self._ai_disabled:
            return
        self._ticks_since_infer += 1
        if self._ticks_since_infer >= AI_PERIOD_TICKS:
            self._ticks_since_infer = 0
            self._run_inference()

    def _run_inference(self) -> AIDecision:
        """Run one perceive-decide-act cycle (same as kernel/ai.c)."""
        x = perceive(
            heap_used=self._heap_used,
            heap_cap=self._heap_cap,
            pages_free=self._free,
            pages_total=self._total,
            ticks_since_infer=self._ticks_since_infer,
        )
        hidden, action_id = decide(x)
        old_hz = self._hz
        self._hz = act(self._hz, action_id)

        decision = AIDecision(
            inputs=x,
            hidden=hidden,
            outputs=(),  # computed inside decide()
            action=action_id,
            action_name=ACTION_NAMES[action_id],
            hz=self._hz,
        )

        self._last_action = ACTION_NAMES[action_id]
        self._last_action_id = action_id
        self._inferences += 1
        self._ai_last_decision = decision
        self._ai_history.append(HistEntry(tick=self._ticks, action=action_id))
        if len(self._ai_history) > AI_HIST_LEN:
            self._ai_history = self._ai_history[-AI_HIST_LEN:]

        return decision

    # -- AI control (new SDK interface) ----------------------------

    def ai_force_inference(self) -> AIDecision:
        """Force an immediate inference cycle (bypass period)."""
        self._ticks_since_infer = AI_PERIOD_TICKS
        return self._run_inference()

    def ai_get_history(self) -> list[HistEntry]:
        """Return the decision history ring buffer."""
        return list(self._ai_history)

    def ai_get_weights(self) -> dict:
        """Return current model weights (read-only snapshot)."""
        from aeos_sdk.ai_core import B1, B2, W1, W2
        return {"w1": W1, "b1": B1, "w2": W2, "b2": B2}

    def ai_set_weights(
        self,
        w1: list[list[int]] | None = None,
        b1: list[int] | None = None,
        w2: list[list[int]] | None = None,
        b2: list[int] | None = None,
    ) -> None:
        """Update model weights (runtime reconfiguration).

        Validates shapes match the 3-8-3 architecture.
        """
        import aeos_sdk.ai_core as ai_mod

        if w1 is not None:
            if len(w1) != 8 or any(len(row) != 3 for row in w1):
                raise ValueError("w1 must be 8x3")
            ai_mod.W1 = [row[:] for row in w1]
        if b1 is not None:
            if len(b1) != 8:
                raise ValueError("b1 must have 8 elements")
            ai_mod.B1 = list(b1)
        if w2 is not None:
            if len(w2) != 3 or any(len(row) != 8 for row in w2):
                raise ValueError("w2 must be 3x8")
            ai_mod.W2 = [row[:] for row in w2]
        if b2 is not None:
            if len(b2) != 3:
                raise ValueError("b2 must have 3 elements")
            ai_mod.B2 = list(b2)

    def ai_get_inputs(self) -> tuple[int, int, int]:
        """Return current perceive inputs (for debugging)."""
        return perceive(
            heap_used=self._heap_used,
            heap_cap=self._heap_cap,
            pages_free=self._free,
            pages_total=self._total,
            ticks_since_infer=self._ticks_since_infer,
        )

    def ai_is_healthy(self) -> bool:
        return not self._ai_disabled

    def ai_disable(self) -> None:
        """Latch the AI engine into disabled state."""
        self._ai_disabled = True

    def ai_enable(self) -> None:
        """Re-enable a disabled AI engine."""
        self._ai_disabled = False

    # -- Test/simulation hooks --------------------------------------

    def allocate_pages(self, count: int) -> None:
        """Simulate page consumption (raises when exhausted)."""
        if count < 0 or count > self._free:
            raise ValueError("cannot allocate requested pages")
        self._free -= count

    def set_heap_used(self, used: int) -> None:
        """Set simulated heap usage in bytes."""
        self._heap_used = max(0, used)

    def fire_ai_inference(self, action: str) -> None:
        """Simulate one AI decision cycle (manual override)."""
        self._inferences += 1
        self._last_action = action
