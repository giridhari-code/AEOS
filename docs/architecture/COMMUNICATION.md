# AEOS Internal Communication Model

> **Status:** normative · Part of the Master Architecture Document
> (deliverable 5). Everything here is designed so the same fabric works
> in-process, between processes, and — later — across machines.

## 1. Principles

1. **Subsystems observe, never couple.** A subsystem that needs data from
   another either subscribes to events or calls a port — it never reaches into
   internals.
2. **Messages are typed and versioned.** Every message and event has a schema
   with a version. Old consumers tolerate new fields; breaking changes bump the
   schema version, never silently.
3. **Commands and queries are separated (CQRS).** Control paths are commands
   (verbs, validated, audited); telemetry and state reads are queries (read-only,
   cacheable). They have different shapes, rates, and guarantees — and never
   share handlers.
4. **Zero-copy where it matters.** Sensor frames and large payloads travel over
   shared-memory rings; metadata and control travel as small messages.
5. **Everything is observable.** Every message carries a correlation ID; every
   publish/consume can be traced and replayed (research mandate).

## 2. The four transport mechanisms

| # | Mechanism | Used for | Guarantees | Zero-copy |
|---|---|---|---|---|
| 1 | **Syscall ABI** | User → kernel (open, spawn, map, send, ...) | Synchronous, per-process | no |
| 2 | **Message channels** (`ipc::channel`) | Commands, small payloads, service RPC | Bounded queue, FIFO per channel, backpressure | no |
| 3 | **Shared-memory rings** (`ipc::shared_ring`) | High-rate data: camera frames, IMU streams, joint states | Single-writer/single-reader, wait-free | yes |
| 4 | **System event bus** (`ipc::event_bus`) | System-wide state changes, pub/sub | At-least-once, idempotent handlers, dead-letter | no |

All four are exposed through one protocol schema and one client library
(`sdk/bindings/`), so the caller does not care which mechanism carries the
message.

## 3. Message and event contracts

- Wire format: length-prefixed, schema-versioned envelopes; payload serialized
  with a stable, schema-first encoding (protobuf for cross-language, mirror
  types in Rust/C/Python from one schema source).
- Every envelope: `{ schema_id, schema_version, correlation_id, causation_id,
  source, timestamp, payload }`.
- Event naming: past tense (`DriverAttached`, `PlanReady`, `JointStateUpdated`).
- Command naming: verb (`spawn_task`, `move_to`, `set_mode`).
- Query naming: noun ( `get_state`, `list_services` ).

### 3.1 Event catalog (normative core; extensions need ADR + registry entry)

| Event | Publisher | Consumers |
|---|---|---|
| `DeviceAttached` / `DeviceDetached` | `device/` | `kernel`, `hal`, `robotics/`, `telemetry` |
| `DriverFault` | `drivers/` | `kernel` (health), `diagnostics`, `security` (audit) |
| `ThreadStateChanged` | `scheduler/` | `kernel`, `diagnostics` |
| `ProcessSpawned` / `ProcessExited` | `kernel/` | `services/`, `security` (audit) |
| `MemoryPressure` | `memory/` | `scheduler`, `services` (OOM policy), `telemetry` |
| `PerceptBundleReady` | `perception/` | `planning/`, `reasoning/`, `memory` (SDK side) |
| `PlanReady` / `PlanRejected` | `planning/` | `robotics/`, `reasoning/` |
| `TrajectoryDispatched` / `TrajectoryCompleted` / `TrajectoryFailed` | `robotics/` | `planning/`, `telemetry` |
| `SafetyInterlockTriggered` | `robotics/` / `security/` | everything (highest priority) |
| `ConfigChanged` | `config/` | all subscribers |
| `ServiceHealthChanged` | `services/` | `cli`, `telemetry`, `diagnostics` |
| `PluginLoaded` / `PluginUnloaded` / `PluginQuarantined` | `plugins/` | `security` (audit), `services` |
| `NetworkInterfaceChanged` | `network/` | `communication/`, `telemetry` |

## 4. Communication between the layers

```
 Python subsystem ──(ports/events)──▶ sdk/ ──(client lib)──▶ ipc/ ──▶ kernel
 kernel ──(FFI)──▶ hal/ ──▶ drivers ──▶ hardware
 kernel ──(events)──▶ services/ ──▶ runtime/ ──▶ user programs
```

- Python → kernel: only via `sdk/` async clients (`aeos_sdk`), which speak the
  protocol schema over the syscall ABI and channels.
- Kernel → Python: kernel never calls Python. It publishes events; the SDK
  subscribes on behalf of the Python layer.
- C (drivers) → rest of OS: drivers report through `device/` (registration +
  events) and `hal/` callbacks. Drivers never publish domain events directly;
  `device/` translates device state into system events.

## 5. CQRS applied

| Path | Mechanism | Validation | Audit |
|---|---|---|---|
| Commands (control) | Message channels, synchronous reply | Schema + capability check at the port | Yes (audit sink) |
| Queries (state) | Read-only channels / direct VFS-like reads | Schema + capability check | No (except security queries) |
| Events (fact streams) | Event bus pub/sub | Schema | Only security/safety events |

Rationale: control latency must not be degraded by telemetry volume; telemetry
reads must never block command paths. Two mechanisms, one schema, no shared
state.

## 6. Reliability and observability

- **Backpressure:** channels are bounded; senders block or fail by declared
  policy (never silently drop control messages).
- **Idempotency:** event handlers must be idempotent; delivery is at-least-once.
- **Dead-letter:** failing handlers route to a dead-letter queue after N
  attempts (bounded retries with backoff).
- **Tracing:** every envelope carries `correlation_id`; the SDK and Rust core
  emit OTel spans (`telemetry/`).
- **Replay:** `ipc/` can record and replay the event stream per run
  (`diagnostics/`), enabling deterministic research runs and post-mortems.

## 7. Future: distributed transport

The protocol schema and envelope are transport-agnostic by design. A
`transport` adapter in `sdk/` will carry the same envelopes over the network
stack (gRPC/DDS-style), so a Python subsystem can run on another machine
without changing a line of its code. Distribution is an adapter swap —
migration rule: a subsystem is "distributed-ready" only after its ports are
exercised by two adapters in CI.
