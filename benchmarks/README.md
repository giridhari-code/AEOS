# benchmarks/ — Cross-cutting benchmarks

**Charter (ARCHITECTURE.md §3):** benchmark suites with regression
thresholds (perf > 5% regression blocks merge, TESTING.md §4.3).

| Path | Scope |
|------|-------|
| `kernel/` | syscall latency, IPC throughput, context-switch cost |
| `hal/` | C boundary overhead, driver throughput |
| `sdk/` | Python↔bindings overhead |
| `ai/` | pipeline latency, inference budgets (pytest-benchmark) |

Rules: baselines are saved and committed (`--save-baseline nightly` in
nightly.yaml); benchmarks must be runnable headless in CI and on HIL
hardware for release gates.
