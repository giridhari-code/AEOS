# Risk Register

Top risks to AEOS's 15-year program, with owner, likelihood/impact,
and the concrete mitigation baked into the architecture. This file is
maintained by the Architecture Working Group (CODEOWNERS) and reviewed
quarterly at minimum.

Scoring: L (likelihood) and I (impact), each 1–5; product = severity.
Red (≥12), amber (8–11), green (<8).

## R-01 — Kernel fault takes down the control loop

- L 3 · I 5 · 15 · **red**
- A single misstep in any Rust subsystem can crash the whole monolithic
  kernel while the robot is moving.
- Mitigations: `#![forbid(unsafe_code)]` with a tiny counted exception
  list; port contract suites + fuzzing on every boundary; watchdog +
  fault-injection suites (`tests/safety/`); supervision via
  `diagnostics/`; real-time tier pinning control threads; formal
  verification pilots scoped to the capability checker and allocator
  (Phase 4). Residual risk documented in ADR-0002; revisiting a
  microkernel split is a Phase 6 option if this bites.

## R-02 — FFI drift between C headers and Rust bindings

- L 4 · I 3 · 12 · **red**
- bindgen output silently diverges from `hal/drivers/device` headers
  (struct layout, enum values), breaking ABI assumptions.
- Mitigations: `kernel/ffi` regeneration is a CI gate with a committed
  diff check; ABI contract tests compile both sides against the same
  generated constants; `tools/check/boundaries.sh` + unsafe budget.
  Per-commit bindgen drift fails the build, not the field.

## R-03 — Real-time budget blowout on AI integration

- L 4 · I 3 · 12 · **red**
- Python AI layers introduce unbounded latencies; the planner misses the
  10 ms control tick.
- Mitigations: hard budget enforcement in the RT scheduler tier;
  shared-memory zero-copy rings keep data-plane latency bounded; hot
  inference moves to `sdk/bindings/` or plugins behind the capability
  model; `SafetyInterlockTriggered` is a hardware/OS path, not a Python
  path (SECURITY.md §6); benchmark gates in nightly.

## R-04 — Multi-arch scope (x86_64 + aarch64 + riscv64) stalls Phase 0

- L 3 · I 3 · 9 · **amber**
- Three architectures triple the boot/hal/asm surface for a small team.
- Mitigations: strict per-arch isolation (`kernel/src/arch/`); x86_64 is
  the primary until Phase 2; aarch64/riscv64 gated behind their own CI
  jobs; HAL contracts designed arch-neutral; QEMU `virt` targets keep
  bring-up cheap. If the team is < 4, cut riscv64 to Phase 3.

## R-05 — Toolchain pin rot (Rust 1.85 + targets) blocks builds

- L 3 · I 3 · 9 · **amber**
- Pinned nightly-ish targets (`*-unknown-none`) and component availability
  can rot the CI matrix silently.
- Mitigations: dependabot for the workspace, nightly `cargo update
  --dry-run` + drift job; devcontainer image built from the pins; release
  uses the pinned toolchain only (BUILD.md §7). Regular toolchain bumps as
  explicit maintenance PRs.

## R-06 — Plugin isolation proves insufficient (WASM phase)

- L 2 · I 5 · 10 · **amber**
- WASM sandbox or native plugin host leaks capabilities; a malicious
  plugin reaches the control plane.
- Mitigations: capability grants are minimum-scope and audited
  (PLUGINS.md §7); quarantine is sticky and tamper-evident; plugins run in
  worker processes with separate address spaces before WASM lands; the
  permission model is designed so a plugin can never acquire
  `motion.request` without a deliberate, human-audited grant.

## R-07 — Regulatory/safety certification exceeds budget

- L 3 · I 4 · 12 · **red**
- Functional-safety certification (Phase 4) may cost more than planned or
  require redesign.
- Mitigations: ISO 26262-aligned *documentation* from Phase 0 (SECURITY.md
  §6, CODE_STYLE §safety); safety-critical code confined to
  hal/drivers/device/robotics with dedicated owners; certification is a
  Phase 4 feature flag, not a rewrite; independent audit scoped early.

## R-08 — Talent retention for a 15-year project

- L 4 · I 2 · 8 · **amber**
- Long-horizon projects lose momentum and people.
- Mitigations: quarterly demos (every phase exit is demoable); plugin
  ecosystem + academic partnerships build external contributors
  (Phase 5); roadmap documents visible progress; small-scope early wins
  (Phase 0 exits within months).

## R-09 — Supply chain attack on toolchain or deps

- L 2 · I 4 · 8 · **amber**
- Compromised crates/pip packages or CI images compromise the signed
  release chain.
- Mitigations: lockfiles + `cargo deny`/`pip-audit` in CI; SBOM per
  release (release.yaml); sigstore signing; gitleaks pre-commit;
  CodeQL nightly; dependency groups reviewed weekly (dependabot);
  rebuild-from-source reproducibility check in release.

## R-10 — Event-catalog drift breaks cross-layer contracts

- L 3 · I 2 · 6 · **green**
- Producers and consumers of the event catalog (COMMUNICATION.md §3)
  diverge as schemas evolve.
- Mitigations: schema-first protobuf codegen, `schema_version` in the
  envelope, deprecation windows, registry changes required in the same PR,
  contract tests compile both sides from the generated schema.

## R-11 — Scope creep into "everything OS"

- L 4 · I 2 · 8 · **amber**
- A 36-directory ambitious OS attracts broad scope; effort leaks from the
  critical path.
- Mitigations: README + ROADMAP define what AEOS is *not* (no desktop
  environment, no general-purpose L4, no ML framework reinvention);
  ADR-before-subsystem rule (CONTRIBUTING §6); per-directory charters
  (READMEs) freeze responsibilities; scope additions require an ADR with
  an exit strategy.

---

## Review cadence

- Quarterly: re-score all rows, record changes in CHANGELOG.
- Ad hoc: any accepted ADR that touches risk-bearing areas adds/updates
  a row here in the same PR.
- Release: a release is blocked on red rows having an owner and an
  active mitigation (release.yaml HIL gate is the enforcement point).

## Escalation

New red-level risks are escalated to the Architecture Working Group within
two weeks of identification. If a mitigation fails twice, the fallback
strategy (listed per row) becomes the plan.
