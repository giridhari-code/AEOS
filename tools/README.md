# tools/ — Build, cross, flash, debug, check

**Charter (ARCHITECTURE.md §3):** the scripts and small utilities that glue
the repository together. Every tool has a Makefile target or CI job so
humans never run them ad hoc.

Layout:

| Path | Purpose |
|------|---------|
| `build/` | `setup.sh` (toolchain), `make_image.sh` (ISO), `run_qemu.sh` (emulation) |
| `cross/` | cross toolchains + target presets (Phase 3 hardware) |
| `flash/` | board flashing helpers (HIL lab, Phase 3) |
| `debug/` | gdb scripts per arch (`run-gdb`), serial monitor helpers |
| `check/` | `boundaries.sh` (FFI constitution), `unsafe_budget.sh` (kernel unsafe budget) |

Rules: POSIX bash, `set -euo pipefail`, no secrets, no interactive
prompts (CI must be able to run every tool).
