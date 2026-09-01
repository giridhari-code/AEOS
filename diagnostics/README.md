# diagnostics/ — Supervision, watchdogs, crash recovery

**Charter (ARCHITECTURE.md §3):** the system's self-watch: watchdogs,
health signals, watchdog-reset state, and crash artifacts. Core crate +
`bindings/` (Python).

Scope:

- supervisory loops over `ServiceHealthChanged` events,
- watchdog wiring into the RT tier (fail-operational posture,
  ROADMAP Phase 4),
- crash dump capture + restart bookkeeping (`runs/` artifacts).

Boundary: reads everything, writes policy nowhere. It does not schedule
and it does not grant capabilities; it escalates (events, log, audit).
