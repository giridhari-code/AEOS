# Ajeeb Embodied AI Operating System (AEOS)

> A 15+ year operating system for embodied artificial intelligence — one
> platform for virtual agents, humanoid robots, industrial robots, autonomous
> vehicles, drones, and future AI hardware.

AEOS is engineered like a real OS because it is one: a bootloader, a Rust
kernel with memory manager, scheduler, IPC fabric and network stack, a C
hardware abstraction layer and driver model, a Python AI runtime — and a
plugin economy around the whole. Simulation and real hardware are both
adapters behind the same interfaces.

| | |
|---|---|
| **Boot / CPU init / context switching** | Assembly (`boot/`, `kernel/src/arch/`) |
| **Kernel, runtime, scheduler, memory, IPC, networking, security** | Rust |
| **HAL, drivers, device model, embedded support** | C |
| **AI runtime, vision, audio, planning, reasoning, learning, simulation, robotics, SDK** | Python |
| **Language mixing** | Never — cross-language calls exist only at declared FFI boundaries |
| **Architecture** | Clean · Hexagonal · DDD · Event-driven · CQRS · Capability security |
| **Status** | Phase 0 — foundations (see [ROADMAP.md](ROADMAP.md)) |

## The core bet

Simulation is a first-class hardware provider. The same agent code runs in a
physics world today and on a humanoid tomorrow — because `hal/`, `device/`,
and `drivers/` are the only places that know what "hardware" means.

## Repository layout

| Path | What it is |
|---|---|
| `boot/` | Assembly bootloader, CPU initialization, arch startup (per-arch) |
| `kernel/` `memory/` `scheduler/` `ipc/` `security/` `network/` `storage/` `filesystem/` | The Rust kernel core and its subsystems |
| `runtime/` `services/` `cli/` | Rust userland: async runtime, system services, `aeosctl` |
| `hal/` `drivers/` `device/` | The C hardware boundary: abstraction, drivers, device model |
| `perception/` `vision/` `audio/` `planning/` `reasoning/` `learning/` | Python AI subsystems |
| `simulation/` `robotics/` `communication/` | Python simulation, control frameworks, external protocols |
| `sdk/` | The Python developer SDK + FFI bindings (the only Python→kernel path) |
| `plugins/` | Plugin runtime, sandbox, ABI (Rust) |
| `config/` `logging/` `telemetry/` `diagnostics/` | Cross-cutting: config, logs, metrics, crash/post-mortem |
| `tools/` | Build, cross-compile, flash, debug tooling |
| `examples/` `docs/` `tests/` `benchmarks/` | Examples, documentation, verification, performance |

Why every directory exists: see the per-directory `README.md` files and
[ARCHITECTURE.md §3](docs/architecture/ARCHITECTURE.md).

## Quick start

```bash
make setup          # toolchains: rustup components, uv, cmake, qemu
make build-iso      # assemble boot + kernel into build/aeos.iso
make run            # boot it in QEMU
make test           # all test layers (Rust, C, Python)
make ci             # the full release gate
```

Requires: Linux (or macOS), Rust toolchain via rustup, `uv`, CMake, QEMU.
See [DEVELOPMENT.md](DEVELOPMENT.md) for details.

## Documentation map

| Document | Read it when |
|---|---|
| [docs/architecture/ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md) | You want the full system design (start here) |
| [docs/architecture/COMMUNICATION.md](docs/architecture/COMMUNICATION.md) | You want the IPC/event model |
| [docs/architecture/PLUGINS.md](docs/architecture/PLUGINS.md) | You want to write or host plugins |
| [docs/architecture/BUILD.md](docs/architecture/BUILD.md) | You want to build anything |
| [CODE_STYLE.md](CODE_STYLE.md) | You are writing or reviewing code |
| [DEVELOPMENT.md](DEVELOPMENT.md) | You are setting up and working on the tree |
| [TESTING.md](TESTING.md) | You are writing or running tests |
| [SECURITY.md](SECURITY.md) | You touch security, drivers, or report vulnerabilities |
| [CONTRIBUTING.md](CONTRIBUTING.md) | You want to contribute |
| [ROADMAP.md](ROADMAP.md) | You want the 10+ year plan |
| [RISKS.md](RISKS.md) | You want the honest risk analysis |
| [EXPANSION.md](EXPANSION.md) | You want the future expansion plan |
| `docs/adr/` | You are changing an architectural decision |

## The 12 deliverables

1. Master Architecture Document — `docs/architecture/ARCHITECTURE.md`
2. Complete directory tree — same, §3
3. Dependency graph — same, §4
4. Module responsibilities — same, §5
5. Internal communication model — `docs/architecture/COMMUNICATION.md`
6. Plugin system — `docs/architecture/PLUGINS.md`
7. Build system — `docs/architecture/BUILD.md`
8. Coding standards — `CODE_STYLE.md`
9. Development workflow — `CONTRIBUTING.md` + `DEVELOPMENT.md`
10. Roadmap (10+ years) — `ROADMAP.md`
11. Risk analysis — `RISKS.md`
12. Future expansion plan — `EXPANSION.md`

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Short version: trunk-based
development, conventional commits, every PR passes `make ci`, every public
symbol is documented, and every boundary change needs an ADR.

## License

Apache-2.0 — see [LICENSE](LICENSE). Security issues: see
[SECURITY.md](SECURITY.md) — do not open public issues for vulnerabilities.
