# tests/ — Cross-cutting test suites

**Charter (ARCHITECTURE.md §3):** the suites that span layers or live
outside any single crate. Normative strategy: [TESTING.md](../../TESTING.md).

| Path | Scope |
|------|-------|
| `kernel/` | whole-kernel integration (host harness + QEMU) |
| `hal/` | C boundary integration |
| `drivers/` | driver integration |
| `sdk/` | SDK contract suites |
| `ai/` | AI regression + benchmark (`-m bench`) |
| `e2e/` | ISO-under-QEMU scenarios (`--scenarios all`) |
| `contract/` | cross-layer ABI compatibility |
| `fuzz/` | fuzz targets (Rust `cargo fuzz` + C libFuzzer) |
| `hil/` | hardware-in-the-loop (hil.yaml only) |

Rules: every suite is deterministic, pinned by the workspace lockfiles,
and wired into a pipeline (TESTING.md §6).
