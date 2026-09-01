# scheduler/ — CPU scheduling

**Charter (ARCHITECTURE.md §3):** time to run, for whom, and for how long —
including the real-time tier that the motion control loop depends on.
Ports: `SchedulerPolicy`, `ThreadQueue` (ARCHITECTURE.md §5.2).

Scope:

- Phase 0: single-CPU preemptive round-robin + priority,
- Phase 1: fixed-priority preemptive RT tier with budget enforcement
  (ROADMAP), syscall latency < 100 µs benchmarks in `benchmarks/kernel/`,
- Phase 2: SMP per-CPU queues, migration, IRQ affinity.

Boundary: policy lives here; mechanism (context switch, trap entry) lives
in `kernel/src/arch/`. Consumes `memory/` budgets, `security/` capabilities.

Safety: `ThreadStateChanged` events; the RT tier is never starved by AI
workloads (RISKS R-03).
