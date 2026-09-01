# API Reference

Auto-generated reference documentation lands here from build-time codegen:

| Source | Generated output | Tool |
|--------|------------------|------|
| Rust crate docs | `docs/api/rust/` | rustdoc (crate-level `//!` docs required by `missing_docs = deny`) |
| C headers (`hal`, `drivers`, `device`) | `docs/api/c/` | Doxygen |
| Python packages | `docs/api/python/` | mkdocstrings |

The `docs` Make target regenerates these before `mkdocs build --strict`.
Every public symbol must appear here; the CI `docs` job fails otherwise
(`missing_docs = deny` in Cargo.toml, Doxygen warnings-as-errors).
