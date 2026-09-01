# AEOS Security

> An operating system that moves actuators is a physical system: a security
> failure is a safety failure. Security policy and architecture are inseparable.
> This document covers policy and operations; the architecture lives in
> ARCHITECTURE.md §6 and the security subsystem in `security/`.

## 1. Threat model

### 1.1 Current (Phase 0–1, emulated/simulated)

| Threat | Vector | Mitigation |
|---|---|---|
| Malicious or buggy plugin | Plugin distribution | Manifest permissions, signature verification, quarantine, WASM sandbox (Phase 2) |
| Supply chain compromise | Dependencies, toolchains | Pinned toolchains + lockfiles, `cargo-deny`/`pip-audit`/`gitleaks` in CI, signed artifacts, SBOM |
| Buggy driver / HAL fault | Hardware I/O | Driver/HAL confined to C layer behind checked FFI; sanitizers in CI; fault → device quarantine |
| Remote console/tooling hijack | `communication/` adapters | mTLS/JWT, capability checks, rate limits, audit |
| AI output misuse | Model outputs in agent loop | Outputs are untrusted data: validated against plan invariants and safety envelopes before execution |
| Data exfiltration | Logs, telemetry, memory stores | Redaction, privacy flags, encrypted-at-rest stores, audit of access |

### 1.2 Future (physical hardware, Phase 3+)

- **Local network attack** on robot control LAN → network segmentation,
  mTLS-only control plane, signed commands.
- **Physical access** → secure boot, tamper-evident state, hardware E-stop
  wired independently of software.
- **Fleet attack** → device attestation, per-device keys, air-gapped update
  channels.
- **Safety interlocks are the last line of defense:** implemented at the
  hardware-adjacent layer (`hal/`, fail-safe driver path), independent of the
  network, the event bus, and AI components.

## 2. Principles

1. **Least privilege** — every process, plugin, and caller receives exactly the
   capabilities its manifest/role declares; enforced at the port, never
   trusted from the manifest.
2. **Fail closed** — any verification failure: deny, quarantine, safe state.
3. **Defense in depth** — authN at the edge, capability checks at every port,
   audit on every decision, interlocks below everything.
4. **Never trust AI output with authority** — all model-produced actions pass
   through plan invariants and safety envelopes (see §6).
5. **Secrets are opaque** — secret values exist only inside `security::Keyring`
   and its adapters; they never appear in logs, config, or dumps.

## 3. Authentication and authorization

- **Principals:** kernel processes (capability sets), plugins (manifest
  permissions), remote callers (mTLS cert / short-lived JWT).
- **Capability model:** `security::CapabilityManager` grants and checks
  capability strings (`kernel.syscall.*`, `ipc.channel.*`, `motion.request`, ...)
  at every port boundary (seL4-inspired; ADR-0002).
- **Audit:** every allow/deny decision, plugin lifecycle transition, config
  change, and safety interlock is an audit record (hash-chained, append-only,
  exported off-device in production).

## 4. Secrets management

- `Keyring` port with adapters (env, file-backed keystore, hardware-backed
  TPM/secure element in Phase 3+).
- Config files reference secrets only as `${keyring:path}` — never inline.
- Rotation is policy-driven; leaked secrets are revoked by incident response.

## 5. Network and transport

- All external transports (gRPC, WebSocket, MQTT, ROS2 bridge) require TLS 1.3;
  no plaintext fallback.
- Inbound messages are validated, size-bounded, and rate-limited before
  dispatch; every inbound message maps to a capability check.

## 6. Safety (operational)

- `SafetyInterlock` monitors: hardware E-stop, watchdog heartbeats, motion
  limit violations, command-link timeouts, kernel liveness.
- Any trigger → commanded safe state via the **fail-safe driver path** plus a
  `SafetyInterlockTriggered` event and audit record.
- The interlock path never depends on the event bus, Python, or the network.
- Safety-critical test suites (`tests/hil`, `@safety` markers) gate releases.

## 7. Memory safety and FFI safety

- The trusted core is Rust with `#![forbid(unsafe_code)]` except a counted,
  reviewed set of modules (ARCHITECTURE §2.1, CODE_STYLE §2).
- C is confined to `hal/`, `drivers/`, `device/`, `boot/`; kernel↔C boundary is
  checked FFI with opaque handles and `aeos_errno_t` only.
- Kernel build: ASLR, NX, stack protector, panic-on-unsafe in debug.
- Sanitizer builds (ASan/UBSan) run against all C code in CI.

## 8. Supply chain

- Pinned toolchains (`rust-toolchain.toml`, C cross toolchains), `Cargo.lock`,
  `uv.lock` — all CI builds `--locked`/`--frozen`.
- `cargo-deny` (licenses, bans, advisories), `pip-audit`, `bandit`, `gitleaks`,
  CodeQL in CI; dependency drift checked nightly.
- Release artifacts are signed; SBOM (CycloneDX) attached to every release.

## 9. Security testing in CI

| Check | Tool | Stage |
|---|---|---|
| Secrets in repo | `gitleaks` | pre-commit + CI |
| Dependency vulnerabilities | `cargo-deny`, `pip-audit` | CI + nightly |
| Static analysis | `clippy`, `clang-tidy`, `bandit`, `cppcheck` | CI |
| Code scanning | CodeQL | push to main |
| Capability enforcement | contract + component tests with adversarial fixtures | CI |
| Plugin verification | malicious manifest fixtures | CI |
| Wire-format fuzzing | `tests/fuzz/` | nightly |

## 10. Reporting and incident response

- **Reporting:** never open a public issue for vulnerabilities. Contact the
  maintainers privately (address published with the first release); reply
  within 72 h. Encrypted disclosures welcome.
- **Disclosure:** 90-day coordinated disclosure; physical-safety issues get an
  immediate advisory with mitigations.
- **Incident response:** runbook in `docs/guides/security-incidents.md` —
  revoke secrets → quarantine components → audit review → advisory.
  Post-mortems are blameless and public.
- **HOF/bug bounty:** program announced with the first stable release.

## 11. Secure development practices

- No secrets in code, config, tests, docs, or dumps (CI-enforced).
- Dependencies enter only through locked manifests in the same PR.
- Security-sensitive changes (any touch of `security/`, `kernel/`, `hal/`,
  `drivers/`, `device/`, `communication/`, `plugins/`) require an explicit
  security-aware review (CONTRIBUTING §Review).
- Review checklist items for unsafe Rust and MISRA deviations are mandatory,
  not advisory.
