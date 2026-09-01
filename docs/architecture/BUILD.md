# AEOS Build System

> **Status:** normative · Part of the Master Architecture Document
> (deliverable 7). How every byte of AEOS — from the first assembly instruction
> to the Python SDK — is built, reproducibly, on a developer laptop and in CI.

## 1. Toolchain summary

| Layer | Tool | Notes |
|---|---|---|
| Assembly | NASM (x86_64), GAS (aarch64/riscv64) | Per-arch, minimal, deterministic |
| Rust | `cargo` with `rust-toolchain.toml` (pinned, checksummed) | Workspace-wide edition 2024 |
| C | CMake + clang/gcc (per-target cross toolchain) | C17; MISRA-aligned checks |
| Python | `uv` workspace (`uv.lock` pinned) | `>=3.13`, src-layout |
| Orchestration | GNU Make at the root; `tools/build/` for the rest | One entry point: `make` |
| Images | linker scripts + `objcopy` + `grub-mkrescue` (or UEFI app) → `build/aeos.iso` | QEMU-bootable |

Single rule: **the same commands build the same bytes on a laptop, in CI, and
on a release server** (locked toolchains, locked manifests, no network during
build).

## 2. Repository workspaces

```
aeos/                        # one Makefile at the root
├── Cargo.toml               # Rust workspace: kernel, runtime, memory, scheduler,
│                            #   ipc, security, network, storage, filesystem,
│                            #   services, cli, plugins, logging, telemetry,
│                            #   diagnostics (each dir = one crate `aeos-*`)
├── rust-toolchain.toml      # pinned channel + components + target list
├── uv.toml + pyproject.toml # Python workspace: aeos-sdk, aeos-perception, ...
├── hal/CMakeLists.txt       # C HAL (static lib libaeos_hal.a)
├── drivers/CMakeLists.txt   # C drivers
├── device/CMakeLists.txt    # C device model
└── Makefile                 # entry points (below)
```

Cross-language linkage:

- `kernel/ffi/` bindings are **generated** from `hal/include/` +
  `device/include/` (bindgen) and checked in CI as up-to-date (never
  hand-maintained twice).
- The SDK extension (`sdk/bindings/`) is built with maturin/PyO3 from the
  `aeos-ipc` + `aeos-security` crates and the protocol schema.
- The protocol schema (COMMUNICATION.md §3) is the single source for Rust/C/
  Python message types; codegen produces all three.

## 3. Targets

| Target | Toolchain | Produces |
|---|---|---|
| Host (`x86_64-unknown-linux-gnu`) | host toolchain | tests, tools, CLI |
| `x86_64-none` (kernel) | `x86_64-unknown-none` | `aeos-kernel` static lib + image |
| `aarch64-none` (kernel) | `aarch64-unknown-none` | staged |
| `riscv64gc-none` (kernel) | `riscv64gc-unknown-none` | staged |
| `x86_64-pc-windows-msvc` (SDK) | host | SDK bindings (dev only) |
| Python (cp313) | uv/maturin | `aeos_sdk` wheels |

## 4. Build stages (`make` targets)

```text
make setup        # install/verify toolchain: rustup components, uv, cmake, qemu
make build        # host debug build of all Rust + C + Python (no image)
make build-release# optimized, LTO, stripped
make build-iso    # assemble boot/ + kernel into build/aeos.iso (QEMU bootable)
make run          # build-iso + launch QEMU with GDB stub
make test         # all layers: Rust, C, Python (see TESTING.md)
make bench        # benchmark suites with regression thresholds
make lint         # rustfmt+clippy, clang-format+tidy, ruff+mypy, boundary checks
make docs         # build the documentation site
make ci           # the full gate (mirrors .github/workflows/ci.yaml)
make clean
```

Every target is implemented in `tools/build/` as small, composable scripts;
the Makefile only orchestrates (same rule as the OS itself: thin core, no god
targets).

## 5. Reproducibility guarantees

- `cargo build --locked` + `uv sync --locked`; toolchain pinned in
  `rust-toolchain.toml` with checksums; C cross toolchains pinned in
  `tools/cross/`.
- Builds are offline after the first fetch (vendored/`CARGO_HOME` cache in CI).
- Every release artifact carries: git commit, build hash, SBOM, and signatures
  (see SECURITY.md §Supply chain).
- `make build-release` is bit-reproducible for the kernel image (fixed
  timestamps, no build path leakage) — verified by a CI job that builds twice
  and diffs.

## 6. CI/CD integration

| Pipeline | Runs |
|---|---|
| `ci.yaml` (PR + main) | lint, boundary checks, all unit/component/contract tests, SDK build, image build, docs, coverage |
| `nightly.yaml` | e2e in QEMU, fuzz, benchmarks vs main, dependency drift, CodeQL |
| `hil.yaml` | hardware-in-the-loop (self-hosted lab runners, gated) |
| `release.yaml` (tag `v*`) | release build, SBOM + signatures, publish artifacts (crates, wheels, OCI, image), docs deploy |

## 7. Debug builds

- Kernel: `--features debug_assertions`, symbols, `-C debuginfo=2`; QEMU + GDB
  stub (`make run-gdb`); `diagnostics/` crash-dump format is parseable by
  `cli/` (`aeosctl crash <dump>`).
- C: `-fsanitize=address,undefined` variants in CI for drivers/HAL unit tests.
- Python: `aeos_sdk` debug wheels with full backtraces across FFI.

## 8. Adding a subsystem

1. Create the directory with its README charter (ARCHITECTURE.md §7).
2. Add the crate/package to the workspace manifests.
3. Wire the composition root (kernel or SDK).
4. Add contract tests + a CI job touch.
5. ADR if it touches a boundary.
