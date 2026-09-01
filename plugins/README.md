# plugins/ — The plugin system

**Charter (ARCHITECTURE.md §3):** extension points for AEOS: native (Rust
host) and Python host (Phase 1), WASM sandbox (Phase 2, ADR-0003
reserved). The full model is normative in
[docs/architecture/PLUGINS.md](../docs/architecture/PLUGINS.md).

Scope:

- `api/`: the ABI surfaces hosts expose (manifest validation, capability
  grant, lifecycle calls),
- `runtime/`: host implementations (Rust),
- `tests/`: contract + compatibility tests,
- manifest `aeos-plugin.toml`: name/version/abi/entry/permissions/
  dependencies/safety_class/signature.

Boundary: plugins are untrusted. Capabilities are minimum-scope, audited,
and quarantine is sticky (PLUGINS.md §7). A plugin can never acquire
motion capability without a human-audited grant.
