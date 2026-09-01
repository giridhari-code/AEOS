# Testing Strategy

AEOS correctness is layered: unit tests inside each crate, contract suites
at every port, whole-system tests in emulation, fuzzing, and finally
hardware-in-the-loop. The rule of thumb: **every boundary has a contract
test, every risk has a test layer that is cheaper than the failure it
prevents.**

- [1. Principles](#1-principles)
- [2. Test pyramid](#2-test-pyramid)
- [3. Contract suites](#3-contract-suites)
- [4. Cross-cutting suites](#4-cross-cutting-suites)
- [5. Where tests live](#5-where-tests-live)
- [6. CI mapping](#6-ci-mapping)

## 1. Principles

1. **Tests are code.** Reviewed, linted, and documented like production.
2. **Contract over implementation.** Port tests mock the environment, not
   the port.
3. **Deterministic.** No sleeps, no wall-clock assumptions, seeded RNGs,
   virtual time in simulation.
4. **Reproducible from the lockfile/toolchain pins.**
5. **Every failure first reproduces as a test** — bug fixes ship with a
   failing test (CONTRIBUTING §2).
6. **Safety properties get dedicated markers** so they run first and fail
   the build immediately (`-m safety`).

## 2. Test pyramid

### 2.1 Unit tests

- Rust: `#[cfg(test)]` in-crate; `cargo test -p <crate>`.
- C: test executables per library, run via CTest (`ctest --test-dir build/hal`).
- Python: `uv run pytest <package>/tests`; pure logic only, no hardware,
  no kernel syscalls.

### 2.2 Component / integration tests

- Rust: `tests/` dirs per crate; the `aeos-kernel` crate integration tests
  boot the kernel in a host harness where possible.
- Python: cross-module AI-pipeline tests (e.g. perception→planning) using
  recorded sensor fixtures in `tests/ai/`.

### 2.3 System tests in emulation

- `tests/e2e/` boots the built ISO under QEMU (`make build-iso` +
  `tests/e2e/`) and asserts on serial output, telemetry, and file artifacts.
- Scenario files live in `simulation/scenarios/`; `--scenarios all` runs
  the full nightly matrix.

### 2.4 Contract suites

Every port (trait/interface across a subsystem boundary) has a test file
named `<port>_contract.rs` / `test_<port>_contract.c` / `test_<port>_contract.py`
placed in the consumer's test tree. A port is not merged without its
contract suite (CONTRIBUTING §8, CODEOWNERS).

## 3. Contract suites

A contract suite validates:

- input/output behavior per the port's documented invariant (from
  ARCHITECTURE.md §5 tables),
- error behavior: every documented error code `AEOS-<MOD>-<NNN>` is
  reachable and returned exactly,
- capability enforcement: unauthorized callers receive
  `AEOS-SEC-NO_CAPABILITY` (SECURITY.md §3),
- cancellation / shutdown behavior.

Contract suites use property testing where cost-effective (proptest in
Rust, Hypothesis in Python).

## 4. Cross-cutting suites

| Suite | Location | Marker | Runs in |
|-------|----------|--------|---------|
| Safety & liveness invariants | `tests/safety/` | `safety` | ci (first), nightly |
| SDK contract (Python↔bindings) | `tests/sdk/` | — | ci |
| AI regression (fixed fixtures) | `tests/ai/` | `bench` for perf | ci (unit), nightly (bench) |
| End-to-end emulation | `tests/e2e/` | `e2e` | ci (smoke), nightly (all) |
| Fuzzing | `tests/fuzz/` | — | nightly |
| Hardware-in-the-loop | `tests/hil/` | `hil` | hil.yaml only |
| ABI compatibility | `tests/contract/` | `contract` | ci, nightly |

### 4.1 Fuzzing

- Rust: `cargo fuzz` targets in `tests/fuzz/` (kernel syscall ABI, IPC
  decoders, filesystem, network parsers).
- C: libFuzzer harnesses for HAL/driver message parsing, run with ASan/UBSan.
- Seed corpora are committed; findings open security issues via SECURITY.md §8.

### 4.2 Hardware-in-the-loop (HIL)

- Runs only on self-hosted lab runners (`hil.yaml`, label `hil-lab`),
  gated on PR label `run-hil` or manual dispatch.
- Safety-marked HIL tests must pass before any release (release.yaml).
- A board bring-up requires: serial log baseline, watchdog reset test,
  and the HAL contract suite on that board (`tests/hil/`).

### 4.3 Benchmark gates

- Rust: `cargo bench` (criterion) with saved baselines; perf regressions
  > 5% block merge.
- Python: `pytest -m bench` with `pytest-benchmark`, autosaved baselines.
- Benchmarks are run on nightly; HIL benchmarks on hardware only for
  release gates.

## 5. Where tests live

- Per-crate tests: `<crate>/tests/` (Rust `tests/` dirs; Python `tests/` in
  each package).
- C: `hal/tests/`, `drivers/tests/`, `device/tests/` (CTest).
- Cross-cutting: root `tests/{kernel,hal,drivers,sdk,ai,e2e,contract,fuzz,hil}/`.
- Benchmarks: `benchmarks/{kernel,hal,sdk,ai}/` + per-crate `benches/`.

## 6. CI mapping

| Pipeline | Runs | Gates on |
|----------|------|----------|
| ci.yaml | lint (rust/c/py), boundary-checks, unit+integration (all layers), boot-smoke (QEMU), docs | every PR and main |
| nightly.yaml | e2e (all scenarios), fuzz, benchmarks, dependency drift, CodeQL | daily 02:00 |
| hil.yaml | hardware suites | PR label `run-hil` / dispatch; release gate |
| release.yaml | reproducibility (double-build diff), SBOM, signing, HIL gate | tag `v*` |

Local: `make test` (all unit/integration), `make ci` (full PR gate).
