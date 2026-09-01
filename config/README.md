# config/ — Configuration schemas & profiles

**Charter (ARCHITECTURE.md §3):** the schemas and profile files that define
AEOS's configuration surface — including the error-code taxonomy
(`AEOS-<MOD>-<NNN>`, config/schemas/) and the event/message proto schemas
the whole system compiles against (COMMUNICATION.md §3.3).

Scope:

- `schemas/`: protobuf/JSON-schema sources; codegen feeds Rust, C, Python,
- `profiles/`: per-target configs (QEMU, HIL lab, board bring-up),
  consumed by `tests/hil/` and `tools/`.

Boundary: schema changes are additive; any breaking change follows the
deprecation window (PLUGINS.md §5 semantics) and requires an ADR
(CONTRIBUTING §6).
