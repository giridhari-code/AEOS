# logging/ — Structured logging

**Charter (ARCHITECTURE.md §3):** structured, leveled, correlation-id-aware
logging for all four language zones. Core crate (`logging/`) + Python
bindings (`logging/bindings/`).

Scope:

- tracing (Rust) / logging (Python) / syslog-ish (C) adapters with a
  common envelope (COMMUNICATION.md §3.3),
- `AEOS_LOG_LEVEL` control; serial in kernel, OTLP elsewhere,
- never logs secrets (gitleaks + code review; SECURITY.md §3).

Boundary: a system crate; consumed by every layer, importing nothing
domain-specific (dependency graph rule: observability crates sit under
everything).
