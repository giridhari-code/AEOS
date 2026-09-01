# ADR-0002: Modular Monolithic Kernel with Capability Security

- Status: accepted
- Date: 2026-08-06
- Deciders: Architecture Working Group

## Context

AEOS must schedule hard-real-time safety-critical motion, host untrusted
plugins, and isolate AI workloads on one system. Microkernel message-passing
costs and correctness risks conflict with the 1 ms–10 ms control loop
budgets; a fully flat monolithic kernel conflicts with our isolation and
safety requirements. seL4 proves capability-based design at the extreme end
of verification, but full proof of a large multi-language system is beyond
our 15-year plan's first decade.

## Decision

Adopt a **modular monolithic kernel** with **capability-based security**:

1. All subsystems (memory, scheduler, ipc, security, network, storage,
   filesystem, runtime, logging, telemetry, diagnostics) are crates in one
   kernel image, linked statically, cooperating in one address space
   (ARCHITECTURE.md §5.2).
2. Isolation is enforced by *structure and capability* rather than by
   address spaces: subsystems expose **ports** (trait boundaries with
   contract tests) and acquire capabilities through the CapabilityManager
   (`security/`); unauthorized cross-subsystem access fails closed.
3. Plugins, untrusted code, and AI workloads run outside the kernel in
   worker processes with their own address spaces, memory budgets, and
   syscall-mediated capabilities; plugin isolation moves to WASM
   (ADR-0003, Phase 2).
4. The only inter-subsystem communication is via the four IPC transports
   (syscall ABI, message channels, shared-memory rings, system event bus;
   COMMUNICATION.md §2) and the events catalog (§3).
5. Verification is layered (TESTING.md): per-port contract suites, whole-kernel
   tests in QEMU, fuzzing, HIL on hardware. Formal verification is scoped to
   the capability checker and the memory allocator as candidates for later
   phases (ROADMAP Phase 5).

## Consequences

**Positive**

- Syscall and IPC latencies stay within control-loop budgets (benchmarks/
  kernel).
- `forbid(unsafe_code)` holds per crate; the small unsafe list is confined
  to arch, early boot, and ring-buffer pages.
- Untrusted code is still isolated (worker processes now, WASM later), so
  the monolithic core does not expand the trusted computing base to plugins.

**Negative**

- A bug in one subsystem can crash the whole kernel: mitigated by strong
  port contracts, fuzzing, fault injection, and watchdog supervision
  (`diagnostics/`).
- The system is not formally verified end-to-end, unlike seL4-style
  kernels: documented in RISKS.md (R-01) with a scoped mitigation plan.

**Neutral**

- The line between "kernel-internal" and "kernel-external" (process) will
  shift with the WASM plugin host (ADR-0003) and hardware bring-up
  (Phase 3); the port/capability discipline is designed to survive both.

## Compliance

New subsystems must enter through a port with contract tests and a
capability registration, or the ADR that introduces them is incomplete.
`boundary-checks` CI enforces the import graph (ARCHITECTURE.md §4.4).

## References

- ARCHITECTURE.md §4 (dependency graph), §5.2 (kernel core)
- SECURITY.md §2 (threat model), §3 (authz/capabilities)
- COMMUNICATION.md §2 (IPC), §5 (reliability)
- TESTING.md §2.4 (contract suites), §3.2 (kernel e2e)
