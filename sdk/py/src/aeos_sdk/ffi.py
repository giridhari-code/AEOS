"""Native FFI binding loader.

Looks for the kernel client library (libaeos_kernel) and exposes it
as a KernelPort. When no native library is present - the normal
state during Phase 0 simulation-first development - callers fall
back to the in-process mock so agent code runs unchanged.

This module is the ONLY place in the Python tree allowed to touch
ctypes/C symbols.
"""

from __future__ import annotations

import ctypes
import os
from pathlib import Path
from typing import Protocol

from aeos_sdk.mock import MockKernel
from aeos_sdk.types import MemoryStats, SystemVitals, TimerInfo

LIB_NAME = "libaeos_kernel"


class _NativeVtable(Protocol):
    """Symbol contract of libaeos_kernel (see sdk/bindings)."""

    def aeos_memory_total_pages(self) -> int: ...
    def aeos_memory_free_pages(self) -> int: ...
    def aeos_timer_ticks(self) -> int: ...
    def aeos_timer_hz(self) -> int: ...
    def aeos_ai_inference_count(self) -> int: ...
    def aeos_ai_last_action(self) -> str: ...
    def aeos_set_timer_hz(self, hz: int) -> None: ...
    def aeos_ai_force_inference(self) -> None: ...
    def aeos_ai_get_history_count(self) -> int: ...
    def aeos_ai_is_healthy(self) -> int: ...
    def aeos_ai_disable(self) -> None: ...
    def aeos_ai_enable(self) -> None: ...


def _candidate_paths() -> list[Path]:
    env = os.environ.get("AEOS_KERNEL_LIB")
    if env:
        return [Path(env)]
    return [
        Path("sdk/bindings/src") / f"lib{LIB_NAME}.so",
        Path("/usr/local/lib") / f"{LIB_NAME}.so",
    ]


def load_native() -> ctypes.CDLL | None:
    """Return the native kernel client, or None when absent."""
    for path in _candidate_paths():
        if path.is_file():
            return ctypes.CDLL(str(path))
    return None


class NativeKernel:
    """KernelPort backed by libaeos_kernel via ctypes."""

    def __init__(self, lib: ctypes.CDLL) -> None:
        self._lib = lib
        self._bind()

    def _bind(self) -> None:
        """Declare explicit return/arg types for the C contract."""
        lib = self._lib
        u64 = ctypes.c_uint64
        u32 = ctypes.c_uint32
        i32 = ctypes.c_int32

        lib.aeos_memory_total_pages.restype = u32
        lib.aeos_memory_free_pages.restype = u32
        lib.aeos_timer_ticks.restype = u64
        lib.aeos_timer_hz.restype = u32
        lib.aeos_ai_inference_count.restype = u32
        lib.aeos_ai_last_action.restype = ctypes.c_char_p
        lib.aeos_set_timer_hz.restype = None
        lib.aeos_set_timer_hz.argtypes = [u32]

        # New AI control bindings
        lib.aeos_ai_force_inference.restype = None
        lib.aeos_ai_get_history_count.restype = i32
        lib.aeos_ai_get_history_tick.argtypes = [i32]
        lib.aeos_ai_get_history_tick.restype = u64
        lib.aeos_ai_get_history_action.argtypes = [i32]
        lib.aeos_ai_get_history_action.restype = i32
        lib.aeos_ai_is_healthy.restype = i32
        lib.aeos_ai_disable.restype = None
        lib.aeos_ai_enable.restype = None

    def memory_stats(self) -> MemoryStats:
        return MemoryStats(
            total_pages=int(self._lib.aeos_memory_total_pages()),
            free_pages=int(self._lib.aeos_memory_free_pages()),
        )

    def timer_info(self) -> TimerInfo:
        return TimerInfo(
            ticks=int(self._lib.aeos_timer_ticks()),
            hz=int(self._lib.aeos_timer_hz()),
        )

    def ai_inference_count(self) -> int:
        return int(self._lib.aeos_ai_inference_count())

    def ai_last_action(self) -> str:
        raw = self._lib.aeos_ai_last_action()
        return raw.decode("ascii") if isinstance(raw, bytes) else str(raw)

    def set_timer_hz(self, hz: int) -> None:
        if not 10 <= hz <= 500:
            raise ValueError("timer rate must be within [10, 500] Hz")
        self._lib.aeos_set_timer_hz(int(hz))

    def vitals(self) -> SystemVitals:
        return SystemVitals(
            memory=self.memory_stats(),
            timer=self.timer_info(),
            ai_inferences=self.ai_inference_count(),
            ai_last_action=self.ai_last_action(),
            ai_policy_hz=self.timer_info().hz,
        )

    # -- AI control ------------------------------------------------

    def ai_force_inference(self) -> None:
        self._lib.aeos_ai_force_inference()

    def ai_get_history(self) -> list:
        from aeos_sdk.ai_core import HistEntry

        count = int(self._lib.aeos_ai_get_history_count())
        result = []
        for i in range(count):
            tick = int(self._lib.aeos_ai_get_history_tick(i))
            action = int(self._lib.aeos_ai_get_history_action(i))
            result.append(HistEntry(tick=tick, action=action))
        return result

    def ai_get_weights(self) -> dict:
        from aeos_sdk.ai_core import B1, B2, W1, W2
        return {"w1": W1, "b1": B1, "w2": W2, "b2": B2}

    def ai_set_weights(
        self,
        w1: list | None = None,
        b1: list | None = None,
        w2: list | None = None,
        b2: list | None = None,
    ) -> None:
        # Native kernel uses static const weights; weight updates
        # require a kernel reload. Raise for now.
        raise NotImplementedError(
            "Weight update not supported on native backend yet"
        )

    def ai_get_inputs(self) -> tuple:
        # Not exposed via C ABI in Phase 0; return placeholder
        return (0, 0, 0)

    def ai_is_healthy(self) -> bool:
        return bool(self._lib.aeos_ai_is_healthy())


def open_kernel() -> NativeKernel | MockKernel:
    """Open the best available kernel backend.

    Prefers the native library; falls back to the in-process mock.
    """
    lib = load_native()
    if lib is not None:
        return NativeKernel(lib)
    return MockKernel()
