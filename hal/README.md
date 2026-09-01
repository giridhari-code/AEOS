# hal/ — Hardware Abstraction Layer (C)

**Charter (ARCHITECTURE.md §3):** the C boundary between the Rust kernel and
hardware. `hal/include/aeos_hal.h` is the contract the kernel compiles
against via bindgen (`kernel/ffi/`); `hal/src/arch/` holds per-arch shims
(register access, timer, uart, irq, gpio).

Scope:

- C17, MISRA-C:2023-aligned (CODE_STYLE.md §5),
- opaque `aeos_handle_t` everywhere — C never sees Rust types
  (ADR-0001 rule 3),
- contract tests in `hal/tests/` (CTest) run on host and on HIL boards
  (`tests/hil/`, TESTING.md §4.2).

Boundary: imports only `device/include` and its own headers; never kernel
Rust types. Sanitized builds (`-DAEOS_SANITIZE=ON`) in CI.
