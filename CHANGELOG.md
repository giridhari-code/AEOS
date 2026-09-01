# Changelog

All notable changes follow [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the versioning policy in DEVELOPMENT.md §5 (SemVer 2.0.0).

## [Unreleased]

### Added

- Phase 0 repository foundation (2026-08-06):
  - Directory tree and workspaces: Rust (Cargo workspace, 16 crates),
    C (hal/drivers/device CMake), Python (uv workspace, 10 packages).
  - Normative architecture documents: ARCHITECTURE.md,
    COMMUNICATION.md (IPC transports + event catalog),
    PLUGINS.md (plugin model + permission scheme), BUILD.md.
  - CODE_STYLE.md, SECURITY.md (threat model + capability authz).
  - ADR-0001 (language strategy & FFI constitution),
    ADR-0002 (modular monolithic kernel with capability security).
  - CI/CD: ci, nightly, hil, docs, release workflows; dependabot;
    CODEOWNERS; pre-commit; devcontainer; issue templates.
  - Build tooling: Makefile, scripts/docs.sh, tools/ (build, check, cross,
    flash, debug scripts), rust-toolchain.toml (1.85.0).

### Changed

- Repository reset from the previous Python-only prototype scaffold to the
  multi-language Phase 0 layout. History prior to 2026-08-06 is not
  carried forward.

### Deprecated

- Nothing.

### Removed

- Python-only prototype modules (`src/aeos/*`), old docs site, old CI.

### Fixed

- Nothing yet (no code in tree).

### Security

- SECURITY.md established; security-relevant changes require
  `@aeos/security-owners` review (CODEOWNERS).
