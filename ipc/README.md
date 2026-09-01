# ipc/ — Inter-process communication

**Charter (ARCHITECTURE.md §3):** the four transports of COMMUNICATION.md
§2: syscall ABI, message channels, shared-memory rings, system event bus.
Ports: `Channel`, `SharedRing`, `EventBus` (ARCHITECTURE.md §5.2).

Scope:

- channel semantics (bounded, flow-controlled, cancellation-safe),
- zero-copy rings with memory-budget integration (`memory/`),
- event bus implementing the normative catalog (COMMUNICATION.md §3),
- envelope contract: `schema_id`, `schema_version`, `correlation_id`,
  `causation_id`, `source`, `timestamp`, `payload`.

Boundary: the only sanctioned way for subsystems to talk
(ADR-0002 §4). Fuzzed nightly (`tests/fuzz/`); every public message has a
schema in `config/schemas/`.
