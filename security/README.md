# security/ — Capabilities, crypto, audit

**Charter (ARCHITECTURE.md §3):** the capability-based authorization model
(ADR-0002), keyring, and audit trail. Ports: `CapabilityManager`,
`AuditSink`, `Keyring` (ARCHITECTURE.md §5.2).

Scope:

- capability grant/revoke/audit for every subsystem, worker, and plugin
  (PLUGINS.md §7, SECURITY.md §3),
- keyring for encrypted storage and attestation (Phase 3),
- audit records for every grant, revocation, and security-relevant event.

Boundary: does not implement protocol crypto itself where it belongs to
`network/`; the capability checker is a formal-verification pilot target
(ROADMAP Phase 4).

Safety: unauthorized callers receive `AEOS-SEC-NO_CAPABILITY`
(SECURITY.md §3); never grants `motion.request` without a deliberate,
human-audited grant.
