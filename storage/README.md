# storage/ — Persistence primitives

**Charter (ARCHITECTURE.md §3):** block device abstraction, object store,
and key-value store behind the `BlockDevice`, `ObjectStore`, `KvStore`
ports (ARCHITECTURE.md §5.2). `filesystem/` builds on top.

Scope:

- Phase 0: virtio-blk + block device layer; encrypted-at-rest integration
  with `security/` Keyring (Phase 3);
- OTA image staging and rollback storage (Phase 3, ROADMAP).

Boundary: drives `drivers/` via `hal/`; never touches Python. Fuzzed
nightly.
