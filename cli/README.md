# cli/ — Command-line interface

**Charter (ARCHITECTURE.md §3):** the developer/operator shell for AEOS:
querying state, issuing capabilities, watching events, flashing and
debugging. The CLI is a consumer (dependency graph rule 5).

Scope:

- read-only inspection by default; privileged actions require
  capabilities and write audit records,
- `aeos console`, `aeos flash`, `aeos run-iso` conveniences wrapping
  `tools/`,
- contract tests in `cli/tests/` run headless (no TTY assumptions).

Boundary: Rust consumer of `services/` + `sdk/bindings/`; never part of
the kernel image.
