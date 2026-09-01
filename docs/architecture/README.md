# Architecture Documentation

This directory holds the normative architecture documents for AEOS. The
master document is [ARCHITECTURE.md](ARCHITECTURE.md); everything else is a
specialization of it. Accepted ADRs in [docs/adr](../adr/README.md) are
normative and supersede earlier wording in these documents.

| Document | Scope | Audience |
|----------|-------|----------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Master: principles, language strategy, directory tree, dependency graph, subsystem responsibilities | Everyone |
| [COMMUNICATION.md](COMMUNICATION.md) | IPC transports, message contracts, event catalog, reliability | All subsystem authors |
| [PLUGINS.md](PLUGINS.md) | Plugin model: manifest, ABI versioning, permission model, lifecycle | Plugin and host authors |
| [BUILD.md](BUILD.md) | Toolchains, workspaces, build targets, reproducibility, CI | Everyone who builds |

## Status

All four documents are **draft-1** (2026-08-06), authored together with
ADRs 0001–0002. They become stable at the end of Phase 0 review.

## Maintenance

- Update the master document when the directory tree or dependency graph
  changes (CODEOWNERS: `@aeos/architecture-wg`).
- Every public event, message, error code, or ABI change must be reflected
  here *in the same PR* that changes the code (CONTRIBUTING §6).
- Cross-references are checked in `make docs` (mkdocs `--strict`).
