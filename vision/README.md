# vision/ — Vision processing (Python)

**Charter (ARCHITECTURE.md §3):** camera pipelines: calibration, detection,
depth, and the frame-serving path to planning. Everything here is a
consumer of `perception/` frames.

Scope:

- pipeline stages with fixed-fixture regression suites (`tests/ai/`),
- hot-path stages move to `sdk/bindings/` or plugins when benchmarks
  demand (RISKS R-03),
- no motion authority: emits observations, never commands.

Boundary: Python; consumes `aeos_sdk` + `aeos_perception`.
