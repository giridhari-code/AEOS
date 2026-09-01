# simulation/ — Simulation & scenario testing

**Charter (ARCHITECTURE.md §3):** the digital twin: physics/environment
models, sensor simulators feeding `perception/`, and the scenario library
that drives e2e and nightly testing (`tests/e2e/ --scenarios all`).

Scope:

- `scenarios/`: versioned, deterministic scenario definitions,
- world models shared between sim and test harnesses — sim is a first-class
  consumer of the same contracts as hardware (HAL-portable),
- never trusted for safety proofs; hardware (HIL) remains the gate
  (TESTING.md §4.2).

Boundary: Python; consumes `aeos_sdk`; owned with `tests/e2e/`.
