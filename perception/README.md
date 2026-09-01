# perception/ — Perceptual foundations (Python)

**Charter (ARCHITECTURE.md §3):** the shared sensing stack the vision and
audio modules build on: sensor data acquisition, synchronization of
multimodal streams, and the `PerceptBundleReady` producers.

Scope:

- stream/synchronization primitives over `sdk/` shared-memory rings,
- sensor models used by simulation and real hardware alike
  (`simulation/` can replay its recordings here),
- never touches hardware directly (FFI constitution rule 1).

Boundary: Python; imports `aeos_sdk` only. Contract tests against
recorded fixtures (`tests/ai/`).
