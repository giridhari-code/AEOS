# memory/ — Memory management

**Charter (ARCHITECTURE.md §3):** physical frame allocation, virtual memory,
memory budgets, and the out-of-memory policy that protects the control
loop. Ports: `FrameAllocator`, `Vmm`, `MemoryBudget` (ARCHITECTURE.md §5.2).

Scope:

- allocator + paging (MVP in Phase 0, hardening per ROADMAP),
- per-worker memory budgets and overcommit supervision,
- zero-copy shared-memory rings for `ipc/` (COMMUNICATION.md §2.3).

Boundary: never touches hardware directly (goes through `hal/` via
`kernel/ffi`); formal-verification pilot target (ROADMAP Phase 4, RISKS
R-01).

Safety: `MemoryPressure` events feed the OOM supervisor
(COMMUNICATION.md §3); a misstep here can crash the kernel — contract
tests + fuzzing are mandatory (`tests/fuzz/`).
