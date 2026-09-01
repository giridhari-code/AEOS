# runtime/ — Process & execution runtime

**Charter (ARCHITECTURE.md §3):** the user-facing execution model:
processes, threads, address spaces, worker isolation, and the supervisor
hooks that let `diagnostics/` watch over everything. Untrusted code
(plugins, AI workloads) runs here in isolated workers behind capabilities.

Scope:

- process/thread primitives over the scheduler + memory ports,
- worker lifecycle (spawn → execute → exit) and termination contracts,
- no scheduling policy itself (that is `scheduler/`), no syscalls (that is
  `kernel/`).

Boundary: depends on `scheduler/`, `memory/`, `ipc/`, `security/`; never on
Python. Consumed by `services/` and `plugins/`.
