# audio/ — Audio pipelines (Python)

**Charter (ARCHITECTURE.md §3):** microphone/audio processing: VAD,
localization, speech intent. Same consumption pattern as `vision/`:
observations in, never motion commands out.

Scope:

- pipeline stages with fixed fixtures,
- latency budget tracked against the RT tier via `telemetry/`.

Boundary: Python; consumes `aeos_sdk` + `aeos_perception`.
