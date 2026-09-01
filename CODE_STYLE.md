# AEOS Code Style & Engineering Standards

> **Status:** normative · Every rule below is enforced by CI (tooling) or
> mandatory review (marked *review*). Cross-language standards first, then
> per-language rules.

## 1. Cross-language standards

### 1.1 Naming conventions

| Kind | Rule | Example |
|---|---|---|
| System error codes | `AEOS-<MOD>-<NNN>`, unique project-wide, listed in subsystem README | `AEOS-MEM-014` |
| Message/event names | Past-tense verbs for events; verbs for commands; nouns for queries | `PlanReady`, `move_to`, `get_state` |
| Syscalls / IPC methods | `snake_case` verbs | `aeos_channel_send` |
| Public headers | `aeos_<subsystem>.h` with `AEOS_<SUBSYSTEM>_H` guards | `aeos_hal.h` |
| Crates / packages | `aeos-<name>` (Rust), `aeos-<name>` (Python dist, module `aeos_<name>`) | `aeos-kernel`, `aeos_perception` |
| Configuration keys | lowercase, dot-separated sections, `snake_case` fields | `kernel.tick_rate_hz` |
| Documentation | English only; every public symbol documented | — |

### 1.2 Error handling strategy

- One taxonomy: `AEOS-<MOD>-<NNN>` across languages. Module prefixes are
  registered in `config/schemas/` and each module README.
- **Rust:** `Result<T, E>` with enums (`thiserror`); no panics across FFI;
  `panic = "abort"` in kernel with crash dump (`diagnostics/`).
- **C:** `aeos_errno_t` return codes from `aeos_errno.h`; no exceptions; data
  via out-params with explicit length; NULL returns only for optional data.
- **Python:** typed exceptions per subsystem mapping 1:1 to `AEOS-*` codes;
  boundary adapters translate, never leak foreign exceptions.
- Translation happens only at FFI boundaries (ARCHITECTURE §2.1).
- Retry policy: exponential backoff + jitter, bounded budgets; circuit breakers
  around all external services. Safety-critical paths never retry blindly.

### 1.3 Logging, configuration, telemetry

- Logging: structured JSON via `logging/`; bind `correlation_id`; never log
  secrets or raw payloads; hot paths are sampled.
- Configuration: TOML via `config/`; read once at boot, immutable; modules
  receive typed config objects, never parse files themselves.
- Telemetry: OTel via `telemetry/`; every request/event sampled with a budget.

### 1.4 Documentation rules

- Every public symbol has a doc comment (Rust), header comment (C), or docstring
  (Python). CI checks for undocumented public symbols.
- Every subsystem README is maintained with the code: responsibility, public
  API, error codes, events, dependencies.
- Docs are reviewed like code; ADRs precede architectural changes.

### 1.5 Code review rules

- Two reviewers for cross-boundary changes; one domain reviewer + safety
  subcommittee for `hal/`, `drivers/`, `device/`, `robotics/`, `security/`.
- No self-merges. Nit comments prefixed `nit:`.
- Review gates: lint green, tests green, no secrets, no unbounded loops, no
  panics/asserts in release paths, unsafe audited, benchmarks where touched.

### 1.6 Security rules (summary — see SECURITY.md)

- Secrets only via `security::Keyring`; never in code, config, logs, or tests.
- All remote transports TLS 1.3 minimum. Least privilege by default.
- New capabilities, permissions, or audit events require security review.

### 1.7 Performance rules

- No allocation in IRQ context, in hot loops, or in the control path.
- IPC: zero-copy rings for high-rate payloads; small messages for control.
- Benchmarks are part of the definition of done; regressions >10% block merge.
- Profiling before optimizing; budgets are contracts (`telemetry/`).

### 1.8 Plugin guidelines

- Plugin ABI is the SDK surface + permission model only (PLUGINS.md §6).
- Every plugin ships a manifest with declared, shrink-only permissions.
- New permission strings require an ADR entry in `config/schemas/`.

### 1.9 API design rules

- Public API = crate `lib.rs` exports, `aeos_*.h` exports, and Python
  `__init__` exports. Everything else is private.
- SemVer 2.0.0 for the OS version, mirrored across all crates/packages;
  plugin ABI versioned independently (CONTRIBUTING §Versioning).
- Deprecate for two OS minors before removal; runtime warnings + README notes.
- No public function without: documented preconditions, failure modes, and
  complexity statement (Rust `// Complexity:`; C header comment; Python
  docstring).

### 1.10 Dependency rules

- Rust: workspace crates only; no duplicate dependency versions; `cargo-deny`
  enforces licenses, bans, and advisories.
- C: no dynamic allocation of dependencies beyond `hal/`; link order is
  declared in CMake targets.
- Python: `uv` workspace; new deps only via `uv add` (lockfile in same PR);
  pure-Python preferred in AI layer, native only behind adapters.
- Cross-language: dependencies flow only along ARCHITECTURE §4.2 rules.

### 1.11 Import rules

- **Rust:** imports only from crate public API; no `pub(crate)` leakage across
  crates; no `use path::to::internal`; enforced by CI boundary checks.
- **C:** a source file includes only its own `include/` + `hal/` + `device/`
  public headers; `include-what-you-use` clean; no kernel headers in drivers.
- **Python:** `import-linter` layers; no deep imports into other subsystems;
  FFI only inside `sdk/bindings/`.
- **Assembly:** only in `boot/` and `kernel/src/arch/`; CI grep fails
  anywhere else.

## 2. Rust standards

- Edition 2024, toolchain pinned in `rust-toolchain.toml`.
- `#![forbid(unsafe_code)]` by default. `unsafe` allowed only in: `kernel/ffi/`,
  `kernel/src/arch/`, `memory/` (frame table, paging), `ipc/` (rings),
  `scheduler/` (runqueue interior mutability). Each `unsafe` block needs a
  `// SAFETY:` comment citing the invariant; a count of unsafe blocks per crate
  is enforced in CI (regressions fail).
- Formatting: `rustfmt` (defaults). Lints: `clippy -- -D warnings` with the
  workspace lint set (pedantic-lite: correctness + perf lints on, style lints
  curated).
- Errors: `thiserror` enums; `Result` in signatures; no `unwrap`/`expect`/
  `panic!` outside `kernel` tests (lint rule).
- Naming: Rust conventions (modules `snake_case`, types `PascalCase`, traits
  `Trait`-named-by-capability, constants `SCREAMING_SNAKE`).
- Concurrency: `Send`/`Sync` discipline; interior mutability only via
  `parking_lot`/`spin` behind dedicated types in `ipc/`/`scheduler/`.
- Testing: unit tests beside code; `proptest` for invariants; `doc-tests` run.

## 3. C standards

- C17 (C11 minimum for constrained embedded targets).
- Formatting: `clang-format` (LLVM-based style, 4-space indent, `aeos` base
  config at repo root). Lints: `clang-tidy` with the `aeos` config;
  `cppcheck` in CI.
- Naming: `snake_case`; functions `aeos_<mod>_<verb>_<noun>`; types
  `aeos_<mod>_t`; macros `AEOS_<MOD>_<NAME>`; static functions
  `_<verb>_<noun>`.
- Headers: include guards, `extern "C"` blocks, only public types — opaque
  handles (`aeos_handle_t`) for kernel-owned objects.
- Error handling: `aeos_errno_t` (from `aeos_errno.h`); every function
  documents its return codes; no `goto` except single-exit cleanup labels
  (`_exit` pattern, MISRA 15.6 documented).
- MISRA-C:2023 aligned; deviations documented in the file header
  (`/* MISRA: <rule> - <reason> */`).
- Memory: no dynamic allocation in drivers by default (allocate at probe via
  `device/` registry API); fixed-size, bounds-checked buffers; no recursion.
- Interrupts: IRQ handlers are short, lock-free, defer work to the kernel
  workqueue; never call blocking APIs.
- Testing: Unity/Cmocka unit tests per driver; sanitizer builds in CI.

## 4. Python standards

- `>=3.13`, `uv` workspace, src-layout, fully typed.
- Formatting/linting: `ruff` (line length 88) + `ruff format`; `mypy --strict`;
  `bandit` for security lints.
- Naming: PEP 8; packages `aeos_<name>`; modules `snake_case`; classes
  `PascalCase`; protocols named by capability (no `I`/`Base` prefix); events
  past-tense; exceptions `<Thing>Error`.
- Docstrings: Google style; every public symbol documented (CI check).
- Async-first: no blocking calls on the event loop (lint rule); blocking
  computation goes to worker pools (`anyio`).
- No global mutable state; DI via constructors; composition root per subsystem.
- Determinism: every AI subsystem seeds from `config`; runs carry `run_id`.

## 5. Assembly standards

- Files: per-arch, NASM (x86_64) / GAS (aarch64, riscv64), `.S`-style
  directives, minimal.
- Every file header documents: purpose, entry points, clobbered registers,
  stack usage, ABI contract.
- No dynamic state; no floating point in boot path; comments reference the
  architecture manual section for every non-obvious instruction.

## 6. Git and commit standards

- Conventional Commits, scoped to subsystem:
  `feat(kernel): add waitqueue syscall`, `fix(memory): correct COW refcount`.
- One commit = one logical change; PRs < 400 lines unless an ADR justifies
  more.
- Branch strategy: trunk-based (`main` always releasable) + short-lived
  branches `feat/<subsystem>/<slug>`, `fix/<slug>`, `docs/`, `ci/`,
  `security/` (CONTRIBUTING §3).
- Squash-merge via merge queue; linear history.

## 7. Testing, CI/CD, versioning

- Testing: TESTING.md — pyramid per layer, contract tests for every port,
  QEMU e2e, HIL gate.
- CI/CD: `.github/workflows/` — ci/nightly/hil/release; every merge is
  releasable.
- Versioning: SemVer 2.0.0, single OS version across all workspaces; changelog
  from conventional commits (`git-cliff`); deprecation window two minors.
