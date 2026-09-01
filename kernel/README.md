# kernel/ — The Rust kernel core

**Charter (ARCHITECTURE.md §3):** the only place besides `boot/` where
assembly may appear (`kernel/src/arch/`); the home of the capability
checker, syscall ABI surface, and the inter-subsystem port wiring. The
kernel image is composed here from the system crates (ADR-0002).

Scope:

- `src/arch/{x86_64,aarch64,riscv64}/` — trap vectors, context switch,
  mmu setup, the unsafe budget's first entries,
- `src/` — boot handoff, scheduler/memory wiring, syscall dispatch,
- `ffi/` — bindgen-generated bindings from C headers (`hal/include/`,
  `drivers/include/`, `device/include/`) and the diff-checked regeneration,
- `tests/`, `benches/` — kernel-wide suites (TESTING.md §2.3, §4.1).

Boundary: depends on `hal/`/`drivers/`/`device/` headers via `ffi/` only;
never on Python. Assembly outside `kernel/src/arch/` fails CI
(`make boundary-check`).

Rules: `#![forbid(unsafe_code)]` unless a crate declares an
`AEOS-UNSAFE-BUDGET` in its Cargo.toml (tools/check/unsafe_budget.sh).
