# AEOS Plugin System

> **Status:** normative · Part of the Master Architecture Document
> (deliverable 6). The plugin system is the extension economy of the OS: the
> mechanism by which third parties add skills, tools, policies, drivers, and
> integrations without forking the platform.

## 1. Design goals

1. **Extend, never fork.** Plugins add capabilities; the core stays core.
2. **Least privilege by default.** Every plugin declares permissions; every
   permission is enforced at the port, not trusted from the manifest.
3. **Isolation with a migration path.** Phase 1: in-process with strict
   verification. Phase 2+: WASM sandbox (WASI) for untrusted code; native
   shared libraries remain for trusted, signed, kernel-adjacent plugins.
4. **Stable ABI over time.** The plugin ABI is versioned independently of the
   OS version; a plugin written for ABI vN runs on every OS that supports vN.
5. **Observable lifecycle.** Load → verify → init → start → stop → unload, each
   step audited and evented.

## 2. What a plugin is

A plugin is a **distribution unit** (WASM module in Phase 2, native
`.so`/`.dylib`/`.dll` for trusted plugins, Python wheel for AI plugins) that
carries:

```
aeos-plugin.toml          # the manifest (normative)
├── name, version
├── abi                     # plugin ABI version this plugin targets
├── entry                   # entry symbol / module path
├── permissions             # capability list (see §4)
├── dependencies            # other plugins or services
├── safety_class            # none | controlled | safety-critical
└── signature               # detached signature (verified at load)
```

Python plugins additionally carry `pyproject.toml` entry points
(`aeos.plugins.<abi>`) — the SDK is the host for the Python plugin world.

## 3. Hosts and runtime (`plugins/runtime/`)

| Host | Language | Loads | Isolation |
|---|---|---|---|
| Rust plugin runtime | Rust | WASM (WASI) + native libs | Phase 1: process boundary for native, verification only in-process; Phase 2: WASM sandbox (memory, CPU, syscall filtering) |
| Python plugin host | Python (in `sdk/`) | wheels with entry points | Python runtime + declared permissions; optional subprocess isolation |

The Rust runtime is the *trusted* host for system plugins (drivers, protocols,
policies). The Python host is the *rich* host for AI plugins (skills, tools,
models, curricula). Both share: manifest schema, permission model, lifecycle
events, quarantine.

## 4. Permission model

Permissions are capability strings, namespaced by subsystem:

```
kernel.syscall.read_clock
ipc.channel.open
ipc.channel.subscribe:<topic-prefix>
network.socket.open
storage.object.read:<bucket>
filesystem.path.read:/var/lib/aeos/plugins/*
models.infer:<model-family>
motion.request                        # safety_class: controlled
motion.emergency_stop                 # safety_class: safety-critical
security.secret.read:<key-prefix>
```

Rules:

- A plugin receives a scoped **capability set** at load; every port invocation
  checks the set (`security::CapabilityManager`).
- `safety_class: safety-critical` plugins require a second signature (safety
  subcommittee key) and are quarantined on any verification doubt.
- Manifest permissions can only *shrink* across versions, never grow.

## 5. Lifecycle

```
discover → verify(signature, manifest, ABI) → load → init(capabilities)
        → start → [run] → stop → unload
                          └─ fault → quarantine(record cause, OS continues)
```

- Every transition emits an event (`PluginLoaded`, `PluginUnloaded`,
  `PluginQuarantined`) and an audit record.
- Quarantine is sticky: a quarantined plugin does not auto-restart; an
  operator must clear it (documented in `cli/`).

## 6. ABI versioning (the contract that lasts 15 years)

- The plugin ABI is defined in `plugins/api/` — types, entry points, and the
  version constant. It is versioned `MAJOR.MINOR` independently of OS SemVer.
- OS major releases must not break plugin ABIs that are still supported;
  breaking ABI changes are introduced with a **deprecation window of two OS
  minor releases** (see CONTRIBUTING §Versioning).
- The ABI surface is exactly: the manifest schema, the entry-point table, the
  permission strings, and the event envelope. Nothing else is stable.

## 7. What plugins may NOT do

- Import or link against OS internals (`kernel/`, `memory/`, `scheduler/`,
  `hal/` implementation headers) — enforced by build-time checks.
- Raise or catch kernel-level exceptions/panics.
- Register interrupt handlers (drivers shipped in-tree or as signed native
  plugins only, Phase 2+).
- Reach other plugins' memory or state.

## 8. Shipping and distribution

- Signed at build time; signatures verified at load.
- Manifest, signature, and SBOM recorded in `security/` audit.
- Version resolution and updates are orchestrated by `services/` (update
  subsystem); plugins participate in transactional updates.

## 9. Contract tests (TESTING.md §Contract)

- Every plugin host implements a common test suite: manifest parsing, ABI
  negotiation, permission enforcement, lifecycle, quarantine, malicious
  fixtures (`tests/fixtures/security/`).
- Fuzzing targets for manifest parsing and ABI handshakes live in
  `tests/fuzz/`.

## 10. Writing a plugin (getting started)

See `examples/` (Rust native, WASM, and Python plugins) and
`docs/guides/writing-a-plugin.md`. The rule of thumb: the plugin API is the
SDK plus the permission model — if a plugin needs more, the API is wrong.
