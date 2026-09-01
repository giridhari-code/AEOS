# ADR Index

Statuses: **proposed** → **accepted** → **deprecated** → **superseded by ADR-NNNN**.

| ADR | Title | Status | Date |
|-----|-------|--------|------|
| [0001](0001-language-strategy.md) | Language Strategy & FFI Constitution | accepted | 2026-08-06 |
| [0002](0002-kernel-model.md) | Modular Monolithic Kernel with Capability Security | accepted | 2026-08-06 |
| 0003 | (reserved: plugin host) | — | — |
| 0004 | (reserved: learning substrate) | — | — |

## Process

Every decision that affects public behavior, safety properties, or crosses an
FFI boundary needs an ADR. Proposal steps (CONTRIBUTING §6):

1. Write the ADR following the template below.
2. Open a PR titled `adr(NNNN): ...`, request review from the Architecture
   Working Group (CODEOWNERS).
3. Apply during a two-week review window; the ADR becomes **accepted** only
   after merge, and its status table row is updated.
4. ADRs are immutable once accepted. Revisions are new ADRs that supersede
   the old one.

## Template

```markdown
# ADR-NNNN: <Title>

- Status: proposed
- Date: YYYY-MM-DD
- Deciders: <names/teams>

## Context

## Decision

## Consequences (positive / negative / neutral)

## Compliance

## References
```

## Compliance

Accepted ADRs are normative. A change that contradicts an accepted ADR is
rejected in review; enforcement is via the `boundary-checks` CI job and the
contract test suite (TESTING.md §3).
