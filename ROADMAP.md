# Roadmap

AEOS is a 15+ year program to build an operating system for embodied AI:
real-time safety-critical motion control, a capable AI stack, and hostile
multi-tenancy, all on commodity and safety hardware. Phases are sequenced
so each one produces a bootable, testable artifact. Dates are targets, not
commitments; scope is per phase, not per year.

Status notation: `[ ]` planned, `[~]` in progress, `[x]` done.

## Phase 0 — Foundations (2026–2027) `[~]`

- [~] Repository structure, workspaces, CI/CD, docs (current work).
- [ ] Boot `x86_64`: assembly stub → Rust kernel on QEMU (serial output,
      GDT/IDT, pic/pit), `make run` reaches "kernel booted".
- [ ] HAL v1 (`aeos_hal.h`): timer, uart, irq, gpio contracts; bindgen FFI.
- [ ] memory: frame allocator + paging MVP with kernel page tables.
- [ ] scheduler: single-CPU preemptive round-robin + priority; syscalls
      surface through the ABI.
- [ ] ipc: message channels + system event bus with the envelope contract.
- [ ] security: capability manager, audit log, `forbid(unsafe_code)` green.
- [ ] drivers: virtio-net/virtio-blk on QEMU; storage + filesystem (ext2
      read) MVP.
- [ ] runtime + services: process model with worker isolation; CLI shell.
- [ ] sdk/py: first `aeos_sdk` calls through bindings; e2e smoke in QEMU.
- [ ] Boot `aarch64` (virt) and `riscv64` (virt) stubs reach the same
      kernel entry.

Exit: every PR gate green (ci.yaml), nightly e2e all-scenarios green, first
external contributor onboarded, docs site live.

## Phase 1 — Control & Perception (2027–2028)

- [ ] Real-time scheduler tier (fixed-priority preemption, budget
      enforcement, latency benchmarks < 100 µs syscall).
- [ ] perception/vision/audio: sensor pipelines over shared-memory rings,
      zero-copy frames; synthetic-world tests in `simulation/scenarios/`.
- [ ] planning/reasoning: first planning loop with the event catalog
      (`PerceptBundleReady → PlanReady`), safety interlock proto
      (`SafetyInterlockTriggered`).
- [ ] device/ layer: servo/camera/LIDAR device contracts via `device/`
      with C drivers on emulated boards.
- [ ] network: TCP/IP stack MVP in the kernel; `network.socket` capability.
- [ ] plugin ABI v1 (native, Rust host): manifest, capability grant,
      quarantine on fault; plugins/contract tests.
- [ ] logging/telemetry/diagnostics v1 with OTLP export.

Exit: demo = robot-in-simulation planning a path and dispatching
trajectories, with telemetry and an emergency stop.

## Phase 2 — Isolation & Native SDK (2028–2030)

- [ ] WASM plugin host (ADR-0003): untrusted plugins sandboxed, no kernel
      access; WASI-like host imports mapped to capabilities.
- [ ] Memory budgets, overcommit policy, out-of-memory supervisor.
- [ ] SDK hardening: Python SDK GA (mypy strict, audit), `sdk/bindings`
      performance parity for hot paths.
- [ ] learning: online learning over replay buffers with model versioning
      and rollback (safety-constrained by design).
- [ ] Multi-core scheduler (SMP): per-CPU run queues, migration, IRQ
      affinity.
- [ ] First CVE process drill; fuzz corpus coverage report.

Exit: an untrusted third-party plugin runs inside a robot process and
cannot break the control loop (demonstrated failure-injection test).

## Phase 3 — Real Hardware & Fleet (2030–2033)

- [ ] Board bring-ups: x86_64 SBCs, RPi-class aarch64, RISC-V boards;
      HIL lab with two+ boards under test (hil.yaml green on hardware).
- [ ] Real-time networking: TSN support, network segmentation for
      control traffic; network security posture (SECURITY.md §4).
- [ ] Persistent encrypted storage; secure boot chain (signed kernel +
      verified measurements), measured boot attestation.
- [ ] Over-the-air updates with rollback; update-signing CA.
- [ ] Fleet diagnostics: telemetry aggregation, anomaly detection on
      device telemetry.

Exit: first field unit runs AEOS end-to-end (boot → perception → planning →
motion) with signed OTA updates; two HIL boards in continuous CI.

## Phase 4 — Safety Case (2033–2036)

- [ ] Formal verification pilot: capability checker + frame allocator
      (seL4-adjacent tooling); proof-linked contract tests.
- [ ] MISRA/CERT alignment audit for HAL/drivers/device; static analysis
      in CI (coded warnings).
- [ ] Functional-safety process (ISO 26262-aligned documentation where the
      market requires it), independent safety audit.
- [ ] Fail-operational supervisor: watchdogs, lockstep drivers, safe
      state machine; `SafetyInterlockTriggered` end-to-end.

Exit: third-party safety audit report; published verification artifacts.

## Phase 5 — Scale & Ecosystem (2036–2040)

- [ ] Multi-node embodied fleets: distributed planning, swarm
      coordination, decentralized identity for devices.
- [ ] Plugin marketplace with signed manifests and reputation; ABI v2.
- [ ] Domain-specific SDKs (navigation, manipulation, inspection) on top
      of `sdk/`.
- [ ] Educational outreach: curriculum, summer of code, academic
      partnerships.

Exit: AEOS powers products in at least two domains; the plugin ecosystem
has third-party maintainers independent of the core team.

## Phase 6+ — Beyond (2040+)

- Long-term: verified microkernel ambitions (seL4-adjacent end-to-end
  proofs), heterogeneous accelerators, humanoid-scale motion control,
  multi-robot fleets with economic incentives.

---

Every phase exit is a release milestone (v1.0 at Phase 2 exit, SemVer
policy in DEVELOPMENT.md §5). Roadmap changes are architecture-wg decisions
(CONTRIBUTING §6); see RISKS.md for the risks that can move these dates.
