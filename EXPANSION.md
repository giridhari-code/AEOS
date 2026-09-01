# Future Expansion Plan

How AEOS grows beyond the current repository without breaking the
architecture. Expansion is gated by the same rules as any change: ADR
first, contract tests, boundary compliance (ARCHITECTURE.md §4, §7).

## 1. New hardware architectures

| Architecture | Entry point | Precondition |
|--------------|-------------|--------------|
| arm64 (bare metal, e.g. RPi) | `boot/aarch64/`, `kernel/src/arch/aarch64/` | HAL contract suite green under QEMU `virt` (Phase 0 exit) |
| riscv64 | `boot/riscv64/`, `kernel/src/arch/riscv64/` | same, Phase 1–2 |
| future (e.g. loongarch, powerpc) | add `boot/<arch>/`, `kernel/src/arch/<arch>/`, HAL arch shim | ADR + cross-compile CI job before hardware commit |

Adding an arch must not change any arch-neutral crate: the arch split in
`kernel/src/arch/` + `hal/src/arch/` is the contract (ARCHITECTURE.md §5.1).

## 2. New device classes (C layer)

New hardware appears as new contracts in `device/include/`, driven by
drivers in `drivers/src/device/`, behind `hal/` bus bindings:

- Motor controllers, IMUs, GNSS, depth cameras, manipulator joints,
  grippers, power management.
- Each device contract: header + contract test + HIL matrix row
  (TESTING.md §4.2). No device contract ships without its HIL plan.

## 3. New AI capabilities (Python layer)

- New modules follow the `aeos_<name>` package pattern (pyproject.toml
  uv members) and only communicate via the SDK and the event catalog.
- A new AI capability needs: a scenario in `simulation/scenarios/`, a
  fixed-fixture regression suite in `tests/ai/`, and a safety review if it
  can influence motion (CODEOWNERS safety-subcommittee).
- Learning systems must support model versioning + rollback from day one
  (ROADMAP Phase 2); any online-learning module inherits that contract.

## 4. New transports and message schemas

- New transports are added to COMMUNICATION.md §2 and the IPC ABI surface
  with a new ADR; old transports are retired only after the two-minor
  deprecation window (PLUGINS.md §5 semantics apply to schemas too).
- Message schemas: new `schema_id`s are additive; field changes bump
  `schema_version` and require dual-version contract tests.

## 5. Plugin ecosystem

- Phase 2+ (WASM host) expands `plugins/`; the manifest schema
  (`aeos-plugin.toml`) is extensible only in backward-compatible ways.
- Marketplace plans (Phase 5) are out-of-tree: a signed manifest +
  reputation service, not a change to the kernel.
- ABI v2 will be designed as an additive superset with an explicit
  compatibility matrix (PLUGINS.md §6).

## 6. Fleet / multi-node (Phase 5+)

- New top-level `fleet/` directory (ADR-gated) for coordination,
  distributed planning, and decentralized identity. It must not couple to
  any single-domain AI module; it speaks only the event catalog and
  network transport contracts.
- Design principle: a single robot remains fully functional without the
  fleet layer (offline-first).

## 7. Verification growth

- Formal verification (Phase 4) lands as `tools/verify/` + proof artifacts
  in `docs/verification/`; contract suites are written to be
  machine-checkable later (precondition/postcondition annotations in port
  docs).
- The HIL lab grows by boards, not by bespoke harnesses: `tests/hil/` is
  board-agnostic, configured by `config/profiles/*.toml`.

## 8. What we will NOT build (scope guard)

Per README: no desktop environment, no general-purpose container runtime,
no ML framework reinvention, no microkernel rewrite before Phase 6 review.
Expansion proposals that cross these lines require a replacement plan,
not just an ADR.

## 9. Process

Every expansion above has the same entry checklist:

1. ADR draft (docs/adr/README.md process).
2. Directory charter: `README.md` in the new directory (why it exists).
3. CI wiring: lint + tests green on the relevant pipeline.
4. Documentation: master document §3 table updated.
5. Risk row: RISKS.md updated if the expansion changes severity.
