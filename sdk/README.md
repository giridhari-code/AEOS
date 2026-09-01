# sdk/ — Software Development Kit

**Charter (ARCHITECTURE.md §3):** the only sanctioned way Python (and
external programs) reach AEOS — `sdk/py` (the Python SDK) and
`sdk/bindings/` (Rust crates exposing kernel services, PyO3 for hot
paths). The FFI constitution's rule 1 ends here.

Scope:

- `sdk/py/aeos_sdk*`: events, channels, capabilities, motion requests —
  API stability promised under SemVer (DEVELOPMENT.md §5),
- `sdk/bindings/`: generated bindings + performance-critical native
  paths (RISKS R-03 mitigation),
- contract tests in `sdk/tests/` and `tests/sdk/` run on every layer
  change (TESTING.md §2.4).

Boundary: nothing above the SDK may import native layers directly
(`make boundary-check` enforces).
