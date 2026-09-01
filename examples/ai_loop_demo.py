#!/usr/bin/env python3
"""Demo: AI integration with kernel - full perceive-decide-act loop.

Shows:
  1. Real MLP inference (same as kernel/ai.c)
  2. Decision history tracking
  3. Weight inspection
  4. Runtime inference forcing
  5. Health monitoring

Usage:
    make -C sdk/bindings
    PYTHONPATH=sdk/py/src python3 examples/ai_loop_demo.py
"""

from aeos_sdk.ffi import open_kernel
from aeos_sdk.mock import MockKernel


def main() -> None:
    kernel = open_kernel()
    print(f"Backend: {type(kernel).__name__}")
    print()

    # Show initial state
    print("=== Initial State ===")
    v = kernel.vitals()
    print(f"  Memory: {v.memory.free_pages}/{v.memory.total_pages} pages free")
    print(f"  Timer: {v.timer.hz} Hz")
    print(f"  AI: {v.ai_inferences} inferences, last={v.ai_last_action}")
    print(f"  Healthy: {kernel.ai_is_healthy()}")
    print()

    # Show model weights
    print("=== Model Weights (3-8-3 MLP) ===")
    weights = kernel.ai_get_weights()
    print(f"  W1: {len(weights['w1'])}x{len(weights['w1'][0])} hidden layer")
    print(f"  W2: {len(weights['w2'])}x{len(weights['w2'][0])} output layer")
    print()

    # Run inference cycles
    print("=== AI Inference Cycles ===")
    for cycle in range(1, 6):
        # Simulate some memory pressure
        if isinstance(kernel, MockKernel):
            kernel.allocate_pages(100 * cycle)

        decision = kernel.ai_force_inference()
        v = kernel.vitals()
        print(
            f"  Cycle {cycle}: action={v.ai_last_action:8s} "
            f"hz={v.ai_policy_hz:3d} "
            f"pressure={v.memory.pressure:.1%} "
            f"inferences={v.ai_inferences}"
        )
    print()

    # Show decision history
    print("=== Decision History ===")
    history = kernel.ai_get_history()
    for i, entry in enumerate(history):
        from aeos_sdk.ai_core import ACTION_NAMES
        print(f"  [{i:2d}] tick={entry.tick:5d} action={ACTION_NAMES[entry.action]}")
    print()

    # Final vitals
    print("=== Final Vitals ===")
    v = kernel.vitals()
    print(f"  Memory: {v.memory.free_pages}/{v.memory.total_pages} pages "
          f"({v.memory.pressure:.1%} pressure)")
    print(f"  Timer: {v.timer.hz} Hz, {v.timer.ticks} ticks")
    print(f"  AI: {v.ai_inferences} inferences, last={v.ai_last_action}")
    print(f"  History: {len(history)} decisions tracked")
    print(f"  Healthy: {kernel.ai_is_healthy()}")


if __name__ == "__main__":
    main()
