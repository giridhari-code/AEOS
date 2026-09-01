# reasoning/ — Symbolic & contextual reasoning (Python)

**Charter (ARCHITECTURE.md §3):** higher-order reasoning over the
perception state: scene semantics, constraints, intent — the input to
planning's cost functions, never a motion authority itself.

Scope:

- constraint/context services consumed by `planning/`,
- explicit, testable logic first; learned components only behind
  `learning/` versioning + rollback (ROADMAP Phase 2).

Boundary: Python; consumes `aeos_sdk`, `aeos_perception`, `aeos_vision`.
