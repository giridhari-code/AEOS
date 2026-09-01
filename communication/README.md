# communication/ — Agent-to-agent communication (Python)

**Charter (ARCHITECTURE.md §3):** communication between AEOS agents — local
agents now, distributed fleets later (EXPANSION.md §6) — over the network
transport contracts, with content defined by the event catalog.

Scope:

- message framing + routing atop `network/` capabilities (`network.socket`),
- offline-first: a single agent functions fully without the fleet layer,
- remote plans/messages arrive as first-class events
  (`schema_version`-checked, COMMUNICATION.md §3).

Boundary: Python; consumes `aeos_sdk` only; fuzz-safe parsing of all
inbound bytes (`tests/fuzz/`).
