# services/ — System services

**Charter (ARCHITECTURE.md §3):** the always-on system services that
orchestrate everything above the kernel: the plugin host, the worker
supervisor, the event bus fan-out, and the initial console. Services are
consumers of the system (ARCHITECTURE.md §4.4) — they never implement
kernel policy.

Scope:

- service registry and lifecycle (start order, restart policy),
- hosts workers spawned by `runtime/`,
- the system console / shell session plumbing for `cli/`.

Boundary: depends on all system crates, but is not depended upon by them
(dependency graph rule 5).
