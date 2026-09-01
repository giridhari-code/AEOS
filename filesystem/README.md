# filesystem/ — The AEOS VFS

**Charter (ARCHITECTURE.md §3):** the virtual filesystem and its
filesystem drivers, so that every object (files, devices, capabilities,
plugin manifests) has a namespace. Ports: `Vfs`, `Filesystem`
(ARCHITECTURE.md §5.2).

Scope:

- VFS core + ext2 read MVP (Phase 0), write + journaling later,
- path-based capability aliases (e.g. `filesystem.path.read:/var/lib/aeos/plugins/*`,
  SECURITY.md §3),
- no user-visible API beyond the SDK.

Boundary: consumes `storage/`; fuzzed nightly (`tests/fuzz/`).
