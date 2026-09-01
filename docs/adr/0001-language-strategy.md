# ADR-0001: Language Strategy & FFI Constitution

- Status: accepted
- Date: 2026-08-06
- Deciders: Architecture Working Group

## Context

AEOS spans boot firmware, a safety-critical kernel, hardware drivers, and an
embodied-AI stack. No single language dominates every concern. We must
partition the system so that each layer uses the right tool while keeping the
whole auditable and the boundaries enforceable by automated checks.

## Decision

Four languages, each confined to a strict zone:

| Language | Zone | Rationale |
|----------|------|-----------|
| Assembly | `boot/` and `kernel/src/arch/<arch>/` only | First bytes, CPU setup, trap entry, context switch |
| Rust | kernel + system subsystems + `sdk/bindings/` | Memory safety by default; `#![forbid(unsafe_code)]` enforced workspace-wide |
| C (C17, MISRA-C:2023 aligned) | `hal/`, `drivers/`, `device/` | Hardware-facing headers consumed by Rust via bindgen |
| Python (≥3.13) | AI layers + `sdk/py` | Model glue, perception/planning/learning R&D velocity |

The boundary rules (the "FFI Constitution", ARCHITECTURE.md §2.1):

1. **Python never touches hardware.** It reaches the system exclusively
   through `sdk/`.
2. **The Rust kernel never calls Python.**
3. **C never sees Rust types.** All C/Rust interchange uses opaque handles
   (`aeos_handle_t`) and the header contracts in `hal/include/`,
   `drivers/include/`, `device/include/`; Rust derives bindings with bindgen
   into `kernel/ffi/`.
4. **Assembly lives only in `boot/` and `kernel/src/arch/`** (per-arch
   `linker.ld`, trap vectors, context switch).
5. **Errors translate only at boundaries**, using the
   `AEOS-<MOD>-<NNN>` code taxonomy (config/schemas/), never by leaking
   language-specific error types across zones.
6. **No shared mutable globals across languages.** State crosses boundaries
   by value (messages, events, shared-memory rings) or through explicitly
   registered ports.

Rust is pinned in `rust-toolchain.toml` (1.85.0, edition 2024); Python
packages are pinned in `uv.lock`. Both are part of reproducible builds
(BUILD.md §7).

## Consequences

**Positive**

- Kernel is memory-safe by default with a short, counted, reviewed list of
  `unsafe` sites (tools/check/unsafe_budget.sh).
- Hardware-facing code is written in C, which is the native ABI of every
  driver and every SoC vendor SDK.
- Python keeps the AI layer experiment-friendly without contaminating
  system code.
- Boundaries are mechanically checkable in CI (`boundary-checks` job).

**Negative**

- Bindgen adds a codegen step to builds; C header discipline is mandatory
  (no struct layout leakage, no C99-isms that bindgen cannot parse).
- Four toolchains must be maintained in the devcontainer and CI images.

**Neutral**

- The AI layers are slower than native; performance-critical inference
  paths may later move behind `sdk/bindings/` (PyO3) or plugins (ADR-0003,
  reserved).

## Compliance

Enforced by `make boundary-check` and the `boundary-checks` CI job:
assembly only in allowed dirs; no `unsafe` in Python/C; no cross-language
imports other than the sanctioned paths.

## References

- ARCHITECTURE.md §2.1, §5
- CODE_STYLE.md §1 (cross-language rules) and per-language sections
- BUILD.md §2 (toolchains), §5 (bindgen)
