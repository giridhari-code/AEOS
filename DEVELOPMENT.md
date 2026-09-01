# Development Guide

How to set up and work with the AEOS repository. Build mechanics live in
[BUILD.md](docs/architecture/BUILD.md); style rules in
[CODE_STYLE.md](CODE_STYLE.md); contribution process in
[CONTRIBUTING.md](CONTRIBUTING.md).

- [1. Prerequisites](#1-prerequisites)
- [2. One-time setup](#2-one-time-setup)
- [3. Daily workflow](#3-daily-workflow)
- [4. Repo layout at a glance](#4-repo-layout-at-a-glance)
- [5. Versioning and compatibility](#5-versioning-and-compatibility)
- [6. Debugging](#6-debugging)
- [7. Common tasks](#7-common-tasks)

## 1. Prerequisites

- Rust 1.85.0 (pinned via `rust-toolchain.toml`; rustup required)
- Python ≥ 3.13 with [uv](https://docs.astral.sh/uv/)
- CMake ≥ 3.25 + a C17 compiler (clang recommended), NASM (x86_64)
- GNU Make
- QEMU (any arch you target) for boot smoke tests
- Optional: `grub-mkrescue` (build-iso), `gdb-multiarch` (run-gdb)

The pinned toolchain, all targets, and expected versions are in
`rust-toolchain.toml` and BUILD.md §2.

## 2. One-time setup

```sh
# Toolchain components, uv, cmake, qemu (adjust for your distro)
make setup

# Install Python environment from lockfile
uv sync --locked

# Pre-commit hooks
uv run pre-commit install
```

Prefer the devcontainer (`.devcontainer/devcontainer.json`) for a
reproducible environment.

## 3. Daily workflow

```sh
# Everything: lint, unit, contract, integration, e2e, docs
make ci

# Iterate on one layer only
cargo test -p aeos-scheduler          # Rust subsystem
ctest --test-dir build/hal            # C layer
uv run pytest tests/sdk               # Python layer
```

The Makefile is the canonical entry point; CI runs the same targets.

## 4. Repo layout at a glance

| Path | Contents |
|------|----------|
| `boot/` | Assembly bootstrap (x86_64, aarch64, riscv64) |
| `kernel/` | Rust kernel: arch, ffi, core |
| `hal/`, `drivers/`, `device/` | C boundary, MISRA-aligned |
| `runtime/`, `memory/`, `scheduler/`, `ipc/`, `security/`, `network/`, `storage/`, `filesystem/`, `services/` | System Rust crates |
| `logging/`, `telemetry/`, `diagnostics/` | Observability crates + Python bindings |
| `perception/` … `communication/` | Python AI stack |
| `sdk/` | Python SDK + Rust bindings |
| `plugins/`, `cli/`, `config/` | Extension point, CLI, schemas |
| `tools/` | Build, cross, flash, debug, check scripts |
| `docs/` | Architecture, ADRs, guides, API reference |
| `tests/`, `benchmarks/` | Cross-cutting suites |

Full normative description: ARCHITECTURE.md §3 (why every directory
exists).

## 5. Versioning and compatibility

- SemVer 2.0.0. Breaking changes require a deprecation window and a
  CHANGELOG note (CONTRIBUTING §7).
- The plugin ABI versions independently: `AEOS-ABI MAJOR.MINOR`
  (PLUGINS.md §5).
- Message schema versions are embedded in the envelope
  (`schema_version`, COMMUNICATION.md §3.3); old versions are served until
  the deprecation window closes.
- Error codes (`AEOS-<MOD>-<NNN>`) are never renumbered or reused
  (CODE_STYLE.md §3.2); new codes are registered in `config/schemas/`.

## 6. Debugging

- Kernel: `make run-gdb` (QEMU `-S -s`); connect `gdb-multiarch`, load
  `target/.../aeos-kernel`, use the per-arch `.gdbinit` scripts in `tools/debug/`.
- Serial logs: `make run` prints serial to stdout; kernel logs follow
  tracing filters (`AEOS_LOG_LEVEL`).
- Python: standard debugger against `uv run`; `AEOS_LOG_LEVEL=debug` for
  cross-layer tracing.
- Rust panics abort the kernel image by design (release profile); panic
  messages go to serial, and `diagnostics/` reports watchdog-reset state.

## 7. Common tasks

| Task | Command |
|------|---------|
| Add a Rust crate | `cargo new --lib <name>`; register in `Cargo.toml` workspace + `workspace.dependencies` |
| Add a Python package | `uv init --package <name>`; add to `pyproject.toml` uv workspace + members |
| Add a C target | CMake `add_library` in `hal/CMakeLists.txt` (repeat pattern in drivers/, device/) |
| Regenerate Rust FFI from C headers | `cd kernel && make ffi` (bindgen; see BUILD.md §5) |
| New event/message/error code | Update `config/schemas/*.proto` + registry; ADR if public |
| New port | Port trait + contract tests in `<subsystem>/tests/contract/` (TESTING.md §2.4) |
| Release | Maintainers only: tag `vX.Y.Z`, release.yaml runs HIL gate + SBOM + signing |

Environment variables are documented in `.env.example`; none are required
for local development.
