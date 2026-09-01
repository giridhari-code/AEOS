"""AEOS SDK - the only sanctioned Python-to-kernel boundary.

Python code never imports Rust or C symbols directly; all kernel
access goes through the ports defined here, backed either by the
native FFI binding (when a kernel is present) or by an in-process
mock (simulation-first development).
"""

from aeos_sdk.ffi import open_kernel
from aeos_sdk.mock import MockKernel
from aeos_sdk.ports import KernelPort
from aeos_sdk.types import MemoryStats, SystemVitals, TimerInfo

__all__ = [
    "KernelPort",
    "MemoryStats",
    "MockKernel",
    "SystemVitals",
    "TimerInfo",
    "open_kernel",
]

__version__ = "0.2.0"
