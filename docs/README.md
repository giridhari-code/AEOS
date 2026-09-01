# docs/ — Documentation

**Charter (ARCHITECTURE.md §3):** all normative and how-to documentation.

| Path | Contents |
|------|----------|
| `architecture/` | master doc, communication, plugins, build (normative) |
| `adr/` | accepted and proposed ADRs (normative once accepted) |
| `guides/` | scenario-based how-tos |
| `api/` | generated reference (rustdoc/Doxygen/mkdocstrings) |

Rules: `make docs` builds strictly (`mkdocs build --strict`); docs changes
reviewed by the Architecture Working Group (CODEOWNERS); root-level
markdown is canonical and materialized into `docs/_root/` (gitignored) by
`scripts/docs.sh`.
