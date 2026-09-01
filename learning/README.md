# learning/ — Learning systems (Python)

**Charter (ARCHITECTURE.md §3):** model training, adaptation, and online
learning — with versioning and rollback as a hard contract from day one
(ROADMAP Phase 2, EXPANSION.md §3).

Scope:

- model registry with signed versions and rollback,
- replay buffers for online learning (safety-constrained by design),
- evaluation harnesses against `tests/ai/` fixtures and `simulation/`
  scenarios.

Boundary: Python; learned components never receive motion authority
directly — they refine planning/reasoning models that remain
safety-checked.
