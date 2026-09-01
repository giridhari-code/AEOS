# AEOS Implementation Reconciliation Audit

> Generated: 2026-09-01
> Method: Full source-code read of every file in the repository.
> Scope: Every Rust, C, assembly, Python, and shell source file.
> Constraint: No trust placed in prior reports, comments, or documentation claims.

---

## Executive Summary

AEOS has **two parallel implementations**:

1. **C kernel** (`boot/aarch64/`, `boot/x86_64/`) — **REAL, functional, boots on QEMU**. Contains ~3000+ lines of working C and assembly: boot, memory, interrupts, scheduler, syscalls, IPC, filesystem, networking, AI engine, shell, TUI, SMP. This is the real operating system.

2. **Rust workspace** (`kernel/`, `memory/`, `scheduler/`, `ipc/`, `security/`, etc.) — **Partially real, partially broken, does not compile**. Contains real data structures and algorithms but critical compilation blockers, broken frame allocator, fake context switching, and empty crates.

**The workspace does not compile.** Two referenced members (`telemetry`, `sdk/bindings`) have no `Cargo.toml`. This means `cargo test`, `cargo clippy`, `cargo build --workspace` all fail immediately.

---

## Phase 1 — Subsystem Audit

### 1. Boot (AArch64) — REAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| Entry assembly (`boot.S`) | REAL | Parks secondaries, drops EL2→EL1, clears BSS, calls loader |
| Stage 2 loader (`loader.c`) | REAL | Validates kernel bounds, walks DTB for UART/RAM, fills bootinfo |
| Kernel entry (`kernel.c`) | REAL | 709 lines, 9-stage init, spawns 10+ tasks, user-mode EL0 |
| Physical memory (`pmm.c`) | REAL | Bitmap allocator, first-fit, proper bounds checking |
| Virtual memory (`vmm.c`) | REAL | 3-level page tables, 2MB block mappings, MMU enable |
| UART (`uart.c`) | REAL | PL011 MMIO, 115200@24MHz, FIFO enabled |
| Interrupt controller (`gic.c`) | REAL | GICv2 distributor + CPU interface |
| IRQ dispatch (`irq.c`) | REAL | VBAR_EL1, timer INTID 30, syscall routing |
| Timer (`timer.c`) | REAL | CNTV virtual timer, frequency detection |
| Context switch (`switch.S`) | REAL | Saves/restores x19–x28, x29, x30, SP |
| Exception vectors (`exceptions.S`) | REAL | 16-entry table, 4KB aligned, full register save/restore |
| SMP (`smp.c`) | REAL | PSCI CPU_ON, LL/SC spinlocks, 16-slot job queue |
| virtio-blk (`virtio.c`) | REAL | Legacy+modern, split vring, MTTCG workarounds |
| virtio-net (`vnet.c`) | REAL | RX/TX queues, pre-posted receive buffers |
| Heap (`heap.c`) | PARTIAL | Bump allocator works, `kfree()` is a no-op |
| User programs (`ping.s`, `pong.s`) | REAL | EL0 SVC-based syscalls |
| Linker script (`linker.ld`) | REAL | Correct section layout, KERNEL_BASE=0x40080000 |
| Makefile | REAL | Cross-compiles 30+ files, user program pipeline |

**Missing:** `kfree()` reclamation, heap accounting, heap overflow protection.

**Source files:** `boot/aarch64/boot.S`, `loader.c`, `kernel.c`, `pmm.c`, `vmm.c`, `uart.c`, `gic.c`, `irq.c`, `timer.c`, `switch.S`, `exceptions.S`, `smp.c`, `smp_entry.S`, `virtio.c`, `vnet.c`, `heap.c`, `psci.c`, `ramfs.c`, `ipc.c`, `proc.c`, `device.c`, `shell.c`, `tui.c`, `sync.c`, `net.c`, `utils.c`, `linker.ld`, `Makefile`

---

### 2. Boot (x86_64) — PARTIAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| Multiboot entry (`boot.asm`) | REAL | Protected→long mode, 1GB identity pages, GDT |
| Stage 0 (`stage0.S`) | REAL | 32-bit wrapper embedding 64-bit kernel |
| MBR (`mbr.S`) | REAL | INT 13h extended read, real→protected mode |
| El Torito (`cdboot.S`) | REAL | No-emulation boot |
| Context switch (`switch.S`) | REAL | + preemption thunk |
| IRQ/IDT (`irqs.S`, `irq.c`) | REAL | PIC remap, PIT 100Hz, 33 vectors + syscall |
| Kernel (`kernel_x86.c`) | REAL | VGA, UART, TSS, ring-3 user tasks |
| VGA (`vgacon.c`) | REAL | 80x25 text buffer |
| UART (`uart.c`) | REAL | COM1 16550 |
| PMM (`mm.c`) | **STUB** | Hardcoded constants, bump pointer only |
| Heap accounting | **FAKE** | `heap_get_used()` returns hardcoded 1408 |
| AFS (`stubs.c`) | **STUB** | All AFS functions return error codes |
| PSCI (`stubs.c`) | **STUB** | `psci_system_reset()` = `hlt` loop |

**Source files:** `boot/x86_64/boot.asm`, `stage0.S`, `mbr.S`, `cdboot.S`, `switch.S`, `irqs.S`, `irq.c`, `kernel_x86.c`, `mm.c`, `uart.c`, `vgacon.c`, `stubs.c`, `linker.ld`, `Makefile`

---

### 3. Boot (BIOS) — REAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| Entry (`start.S`) | REAL | Same pattern as aarch64 boot.S |
| Firmware (`bios_main.c`) | REAL | UART→PMM→virtio-blk→AFS→load kernel→jump |

**Source files:** `boot/bios/start.S`, `bios_main.c`, `linker.ld`, `Makefile`

---

### 4. Kernel (C) — REAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| Process model (`proc.c`) | REAL | spawn/exit/wait, per-process FDs, address spaces |
| Scheduler (`sched.c`) | REAL | Cooperative round-robin, 16 tasks, watchdog |
| Syscalls (`syscall.c`) | REAL | 21 syscall numbers, 17 implemented, user pointer validation |
| IPC (`ipc.c`) | REAL | 8 queues, 16 msgs/queue, 64-byte messages |
| Device framework (`device.c`) | REAL | Function pointer dispatch, name lookup |
| Logging (`log.c`) | REAL | Ring buffer, levels, ANSI colors |
| Network (`net.c`) | REAL | ARP + ICMP echo/ping over virtio-net |
| RAMFS (`ramfs.c`) | REAL | 32 files, 4KB max, CRUD ops |
| Sync (`sync.c`) | REAL | Mutex + semaphore, cooperative blocking |
| Shell (`shell.c`) | REAL | Interactive commands, self-update |
| TUI (`tui.c`) | REAL | ANSI dashboard, 3 panels |
| AI core (`ai_core.c`) | REAL | 3-8-3 fixed-point MLP |
| AI API (`ai_api.c`) | REAL | Tick subscription, history, health |
| AI learn (`ai_learn.c`) | REAL | Q-learning with experience replay |
| AI debug (`ai_debug.c`) | PARTIAL | Scanning real, 3 of 7 fixes are stubs |
| CMT engine (`cmt_core.c`) | REAL | 6-12-3 MLP consciousness model |
| CMT API (`cmt_api.c`) | REAL | Tick handler, diagnostics |
| AFS filesystem (`afs.c`) | REAL | On-disk via virtio-blk, superblock, directory, data sectors |
| Fixed-point math (`fp.h`) | REAL | Correct Q16.16 multiply, softsign, clamp |

**Source files:** `kernel/proc.c`, `sched.c`, `syscall.c`, `ipc.c`, `device.c`, `log.c`, `net.c`, `ramfs.c`, `sync.c`, `shell.c`, `tui.c`, `ai_core.c`, `ai_api.c`, `ai_learn.c`, `ai_debug.c`, `cmt_core.c`, `cmt_api.c`, `afs.c`, `fp.h`

---

### 5. Kernel (Rust) — BROKEN

| Aspect | Status | Evidence |
|--------|--------|----------|
| Crate compiles | **NO** | References `telemetry` and `sdk/bindings` which don't exist |
| `#![forbid(unsafe_code)]` | **CONTRADICTED** | `static mut KERNEL` + `unsafe { KERNEL.get_or_insert_with(...) }` at line 127 |
| `_start()` entry | **MISPLACED** | In `lib.rs`, not `main.rs` |
| `Kernel::run()` | **STUB** | `loop { core::hint::spin_loop(); }` |
| `arch::context_switch()` | **STUB** | `let _ = task_id;` — no-op |
| `arch::enable/disable_interrupts()` | **STUB** | Empty functions |
| `arch::halt()` | REAL | `hlt`/`wfi` behind `#[cfg]` |
| `boot_tick()` method | **SHADOWS FIELD** | Method name same as field |
| Syscall dispatch | PARTIAL | 3 of 21 real (Yield, Exit, GetPid), 18 stubs returning `Ok(0)` |

**Source files:** `kernel/src/lib.rs`, `kernel/src/arch.rs`, `kernel/src/syscall.rs`, `kernel/src/panic.rs`

---

### 6. Memory (Rust) — BROKEN

| Aspect | Status | Evidence |
|--------|--------|----------|
| `#![forbid(unsafe_code)]` | **CONTRADICTED** | `static mut FRAME_ALLOC` + `unsafe { FRAME_ALLOC.get_or_insert_with(...) }` |
| Frame allocator `init()` | **BROKEN** | Calculates `bitmap_words` but never allocates bitmap; `self.bitmap` remains `&mut []` |
| `alloc_frame()` | **ALWAYS FAILS** | Free stack is empty, bitmap is empty slice → always `OutOfFrames` |
| `alloc_frames()` | **STUB** | Returns `OutOfFrames` with TODO comment |
| `free_frame()` | DEAD CODE | Structurally correct but can never be exercised |
| Page table `get_or_create_table()` | **BROKEN** | Casts physical address to pointer, always returns `PageTableAllocationFailed` |
| Page table `map()` | **BROKEN** | Calls `get_or_create_table()` which always fails |
| `VirtAddr`/`PhysAddr` | REAL | Correct bit extraction for x86_64 paging |
| `PageTableFlags` | REAL | Correct bitflags |
| `MemoryRegion` | REAL | Correct pure logic with tests |

**Source files:** `memory/src/lib.rs`, `memory/src/frame_alloc.rs`, `memory/src/page_table.rs`, `memory/src/region.rs`

---

### 7. Scheduler (Rust) — PARTIAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| `#![forbid(unsafe_code)]` | **CONTRADICTED** | `static mut SCHEDULER` in lib.rs |
| Task state machine | REAL | Ready/Running/Blocked/Suspended/Zombie transitions |
| Priority queues | REAL | Correct priority round-robin, FIFO policies |
| `tick()` | REAL | Decrements time slice, triggers `schedule()` |
| `schedule()` | REAL | Picks highest-priority, manages state transitions |
| `create_task()` | REAL | Proper queue insertion |
| `exit_task()` | REAL | State transition to Zombie |
| Context switch | **FAKE** | Returns `TaskId` but no register save/restore |
| Stack allocation | **FAKE** | `0x4000_0000 + id.0 * stack_size` — hardcoded addresses |
| `TaskContext::new()` | **FAKE** | Sets `regs[13]` and `regs[14]` but 32-element array doesn't match any architecture |
| `RoundRobin::should_preempt()` | **BUG** | Always returns `false` — never preempts |

**Source files:** `scheduler/src/lib.rs`, `scheduler/src/task.rs`, `scheduler/src/queue.rs`, `scheduler/src/policy.rs`

---

### 8. IPC (Rust) — REAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| `#![forbid(unsafe_code)]` | **CONTRADICTED** | `static mut IPC_STATE` |
| Channel create/send/recv | REAL | Bounded queue, message types |
| Message type | REAL | 64-byte data, UTF-8 validation |
| Event catalog | REAL | Priority assignments, domain events |
| Error: channel closed | MINOR | Returns `ChannelNotFound` instead of distinct error |

**Source files:** `ipc/src/lib.rs`, `ipc/src/channel.rs`, `ipc/src/message.rs`, `ipc/src/envelope.rs`

---

### 9. Security (Rust) — PARTIAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| `#![forbid(unsafe_code)]` | **CONTRADICTED** | `static mut SECURITY_MANAGER`, `static mut NEXT_CAP_ID` |
| Grant/revoke/delegate | REAL | Correct capability logic |
| Audit log | PARTIAL | `recent()` has off-by-one when buffer wraps; timestamps always 0 |
| Capability IDs | **NOT CRYPTOGRAPHIC** | Sequential integers, not hashed (contradicts module claim) |
| `NEXT_CAP_ID` race | **DATA RACE** | `static mut` with non-atomic increment |

**Source files:** `security/src/lib.rs`, `security/src/capability.rs`, `security/src/audit.rs`, `security/src/rights.rs`

---

### 10. Empty Rust Crates — ZERO IMPLEMENTATION

| Crate | Content |
|-------|---------|
| `runtime` | `//! AEOS runtime module` — nothing else |
| `services` | `//! AEOS services module` — nothing else |
| `filesystem` | `//! AEOS filesystem module` — nothing else |
| `storage` | `//! AEOS storage module` — nothing else |
| `logging` | `//! AEOS logging module` — nothing else |
| `diagnostics` | `//! AEOS diagnostics module` — nothing else |
| `plugins/runtime` | `//! AEOS runtime module` — nothing else |
| `network` | Error enum + stub `init()` returning `Ok(())` |

---

### 11. HAL / Drivers / Device — STUB

| Component | Status | Evidence |
|-----------|--------|----------|
| `hal/CMakeLists.txt` | **STUB** | `add_library(aeos-hal INTERFACE)` — no sources |
| `drivers/CMakeLists.txt` | **STUB** | `add_library(aeos-drivers INTERFACE)` — no sources |
| `device/CMakeLists.txt` | **STUB** | `add_library(aeos-device INTERFACE)` — no sources |
| `hal/src/arch/` | **EMPTY** | No files |
| `drivers/src/bus/` | **EMPTY** | No files |
| `drivers/src/device/` | **EMPTY** | No files |

---

### 12. CLI (Rust) — PARTIAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| Shell I/O | REAL | Character-by-character input processing |
| Command parsing | REAL | Enum-based dispatch |
| All command implementations | **FAKE** | `cmd_ps()` returns static string, `cmd_mem()` returns static string, `cmd_uptime()` returns "0 ticks", etc. |

**Source files:** `cli/src/lib.rs`, `cli/src/commands.rs`

---

### 13. Python SDK — REAL

| Aspect | Status | Evidence |
|--------|--------|----------|
| `KernelPort` protocol | REAL | Hexagonal boundary |
| `MockKernel` | REAL | In-process mock with 3-8-3 MLP |
| FFI loader | REAL | ctypes + fallback to Mock |
| AI core | REAL | Q16.16 fixed-point MLP |
| Tests (~50) | REAL | All have real assertions |

**Source files:** `sdk/py/src/aeos_sdk/*.py`, `sdk/tests/test_*.py`

---

### 14. CI — MOSTLY REAL

| Component | Status | Evidence |
|-----------|--------|----------|
| Rust lint/test | REAL | `cargo fmt`, `clippy`, `deny`, `test` |
| Python lint/test | REAL | `ruff`, `mypy`, `bandit`, `pytest` |
| C lint | REAL | `clang-format` |
| C build (Makefile) | **HIDDEN FAILURE** | Lines 17–19: `\|\| true` swallows all CMake errors |
| Boot smoke tests | REAL | 8 scripts, real QEMU execution |
| Nightly benchmarks | WEAK | `\|\| true` hides baseline comparison failures |
| HIL gate | **NO-OP** | Just echoes "HIL gate passed" |
| Referenced tests | **MISSING** | `tests/e2e/`, `tests/hil/`, `tests/fuzz/`, `tests/ai/`, `tests/contract/`, `tests/safety/` don't exist |

---

### 15. Documentation — REAL BUT OVERSTATES

False claims identified:

| Document | False Claim | Reality |
|----------|------------|---------|
| `README.md` | `make run` quick start | Target does not exist |
| `TESTING.md` | References `tests/e2e/`, `tests/hil/`, `tests/fuzz/` | None exist |
| `CHANGELOG.md` | "Nothing yet (no code in tree)" | 269 files committed |
| `ARCHITECTURE.md` §3 | Lists `fleet/`, many subdirs | Don't exist |
| `SECURITY.md` | "cryptographic hashing for capabilities" | IDs are sequential integers |
| All subsystem READMEs | Describe complete implementations | Most are empty placeholders |

---

## Phase 2 — Architecture vs Reality Matrix

| Architecture Claim | Source Implementation | Buildable? | Executable? | Tested? | Integrated? | Verified? | STATUS |
|---|---|---|---|---|---|---|---|
| AArch64 boot sequence | `boot/aarch64/boot.S` + `loader.c` + `kernel.c` | YES | YES (QEMU) | YES (smoke tests) | YES | YES | **REAL** |
| x86_64 boot sequence | `boot/x86_64/boot.asm` + `kernel_x86.c` | YES | YES (QEMU) | YES (smoke tests) | YES | YES | **REAL** |
| BIOS firmware | `boot/bios/bios_main.c` | YES | YES (QEMU) | YES | YES | YES | **REAL** |
| Physical frame allocator (C) | `boot/aarch64/pmm.c` | YES | YES | NO dedicated tests | YES | YES | **REAL** |
| Physical frame allocator (Rust) | `memory/src/frame_alloc.rs` | **NO** (workspace broken) | NO | NO | NO | NO | **BROKEN** |
| Virtual memory (C) | `boot/aarch64/vmm.c` | YES | YES | NO dedicated tests | YES | YES | **REAL** |
| Virtual memory (Rust) | `memory/src/page_table.rs` | NO | NO | NO | NO | NO | **BROKEN** |
| Scheduler (C) | `kernel/sched.c` | YES | YES | NO dedicated tests | YES | YES | **REAL** |
| Scheduler (Rust) | `scheduler/src/*.rs` | NO | NO | NO | NO | NO | **PARTIAL** |
| Context switch (C) | `boot/aarch64/switch.S` | YES | YES | YES (implicit) | YES | YES | **REAL** |
| Context switch (Rust) | `kernel/src/arch.rs` | NO | NO | NO | NO | NO | **STUB** |
| Syscalls (C) | `kernel/syscall.c` | YES | YES | YES (user programs) | YES | YES | **REAL** |
| Syscalls (Rust) | `kernel/src/syscall.rs` | NO | NO | NO | NO | NO | **16/21 STUBS** |
| IPC (C) | `kernel/ipc.c` | YES | YES | NO dedicated tests | YES | YES | **REAL** |
| IPC (Rust) | `ipc/src/*.rs` | NO | NO | NO | NO | NO | **REAL** (data only) |
| Device framework (C) | `kernel/device.c` | YES | YES | NO | YES | YES | **REAL** |
| Filesystem AFS (C) | `kernel/afs.c` | YES | YES | YES (shell cmds) | YES | YES | **REAL** |
| Filesystem (Rust) | `filesystem/src/lib.rs` | NO | NO | NO | NO | NO | **EMPTY** |
| Networking (C) | `kernel/net.c` | YES | YES | YES (ping) | YES | YES | **REAL** |
| Networking (Rust) | `network/src/lib.rs` | NO | NO | NO | NO | NO | **STUB** |
| AI engine | `kernel/ai_*.c` | YES | YES | NO dedicated tests | YES | YES | **REAL** |
| CMT engine | `kernel/cmt_*.c` | YES | YES | NO | YES | YES | **REAL** |
| Shell | `kernel/shell.c` | YES | YES | YES (manual) | YES | YES | **REAL** |
| TUI | `kernel/tui.c` | YES | YES | YES (manual) | YES | YES | **REAL** |
| HAL | `hal/` | **NO** (INTERFACE only) | NO | NO | NO | NO | **STUB** |
| Drivers | `drivers/` | **NO** (INTERFACE only) | NO | NO | NO | NO | **STUB** |
| Device layer | `device/` | **NO** (INTERFACE only) | NO | NO | NO | NO | **STUB** |
| Python SDK | `sdk/py/` | YES | YES | YES (~50 tests) | YES | YES | **REAL** |
| CI pipeline | `.github/workflows/` | YES | YES | YES | YES | YES | **REAL** |
| Rust workspace build | `Cargo.toml` | **NO** | NO | NO | NO | NO | **BROKEN** |

---

## Phase 3 — Critical Issues

### P0 — Compilation Blockers

| # | Issue | File | Line |
|---|-------|------|------|
| P0-1 | **Workspace does not compile** — `telemetry/Cargo.toml` missing | `Cargo.toml` | 20 |
| P0-2 | **Workspace does not compile** — `sdk/bindings/Cargo.toml` missing (it's a C project) | `Cargo.toml` | 22 |
| P0-3 | **`#![forbid(unsafe_code)]` contradicted by `static mut`** — kernel crate | `kernel/src/lib.rs` | 20, 120, 127 |
| P0-4 | **`#![forbid(unsafe_code)]` contradicted by `static mut`** — memory crate | `memory/src/lib.rs` | 15, 77, 84 |
| P0-5 | **`#![forbid(unsafe_code)]` contradicted by `static mut`** — scheduler crate | `scheduler/src/lib.rs` | (same pattern) |
| P0-6 | **`#![forbid(unsafe_code)]` contradicted by `static mut`** — IPC crate | `ipc/src/lib.rs` | (same pattern) |
| P0-7 | **`#![forbid(unsafe_code)]` contradicted by `static mut`** — security crate | `security/src/lib.rs` | (same pattern) |
| P0-8 | **`#![forbid(unsafe_code)]` contradicted by `static mut`** — CLI crate | `cli/src/lib.rs` | (same pattern) |

### P1 — Broken Core Functionality

| # | Issue | File | Line |
|---|-------|------|------|
| P1-1 | **Frame allocator bitmap never initialized** — `init()` calculates size but never sets `self.bitmap` to actual memory | `memory/src/frame_alloc.rs` | 76–94 |
| P1-2 | **`alloc_frame()` always returns `OutOfFrames`** — free stack empty + bitmap empty slice | `memory/src/frame_alloc.rs` | 98–121 |
| P1-3 | **Page table `get_or_create_table()` casts physical addr to pointer** — always fails | `memory/src/page_table.rs` | 208–224 |
| P1-4 | **Context switch is a no-op** — `kernel/src/arch.rs:12–16` | `kernel/src/arch.rs` | 12–16 |
| P1-5 | **Task stack allocation uses fake addresses** — `0x4000_0000 + id * size` | `scheduler/src/task.rs` | 115 |
| P1-6 | **TaskContext layout doesn't match any architecture** — `regs[13]`/`regs[14]` on 32-element array | `scheduler/src/task.rs` | 58–70 |
| P1-7 | **`RoundRobin::should_preempt()` always returns false** — never preempts | `scheduler/src/policy.rs` | (see agent report) |

### P2 — Safety Issues

| # | Issue | File | Line |
|---|-------|------|------|
| P2-1 | **Data race on `NEXT_CAP_ID`** — `static mut` + non-atomic increment | `security/src/capability.rs` | 63 |
| P2-2 | **Audit log `recent()` off-by-one** — reads stale data when buffer wraps | `security/src/audit.rs` | 77–85 |
| P2-3 | **Audit timestamps always 0** — no timer integration | `security/src/audit.rs` | 68 |
| P2-4 | **`boot_tick()` method shadows field** — naming collision | `kernel/src/lib.rs` | 44, 99 |

### P3 — Documentation/Build Inconsistencies

| # | Issue | File |
|---|-------|------|
| P3-1 | Makefile `\|\| true` hides C build failures | `Makefile:17–19` |
| P3-2 | `make run` referenced in README but not defined | `README.md`, `Makefile` |
| P3-3 | `CHANGELOG.md` says "no code in tree" | `CHANGELOG.md` |
| P3-4 | `TESTING.md` references non-existent test dirs | `TESTING.md` |
| P3-5 | `SECURITY.md` claims "cryptographic" capability IDs | `SECURITY.md` |
| P3-6 | Nightly `\|\| true` hides benchmark failures | `.github/workflows/nightly.yaml:45` |
| P3-7 | HIL gate is no-op echo | `.github/workflows/hil.yaml` |

---

## Phase 4 — Summary Table

| Subsystem | C Implementation | Rust Implementation | Compilation | Execution | Tests |
|-----------|-----------------|---------------------|-------------|-----------|-------|
| Boot (AArch64) | REAL | N/A | OK | OK (QEMU) | OK |
| Boot (x86_64) | REAL (PMM stub) | N/A | OK | OK (QEMU) | OK |
| Boot (BIOS) | REAL | N/A | OK | OK (QEMU) | OK |
| Physical memory | REAL | **BROKEN** | FAIL | FAIL | FAIL |
| Virtual memory | REAL | **BROKEN** | FAIL | FAIL | FAIL |
| Scheduler | REAL | **PARTIAL** | FAIL | FAIL | FAIL |
| Context switch | REAL | **STUB** | FAIL | FAIL | FAIL |
| Syscalls | REAL | **16/21 STUBS** | FAIL | FAIL | FAIL |
| IPC | REAL | REAL (data only) | FAIL | FAIL | FAIL |
| Security/Caps | REAL (implicit) | **PARTIAL** | FAIL | FAIL | FAIL |
| Device framework | REAL | N/A | OK | OK | NO |
| Filesystem | REAL (AFS) | **EMPTY** | FAIL | FAIL | FAIL |
| Networking | REAL (ARP+ICMP) | **STUB** | FAIL | FAIL | FAIL |
| AI engine | REAL | N/A | OK | OK | NO |
| CMT engine | REAL | N/A | OK | OK | NO |
| Shell/TUI | REAL | N/A | OK | OK | OK |
| HAL | **STUB** | N/A | N/A | N/A | N/A |
| Drivers | **STUB** | N/A | N/A | N/A | N/A |
| Device layer | **STUB** | N/A | N/A | N/A | N/A |
| CLI (Rust) | N/A | **FAKE COMMANDS** | FAIL | FAIL | FAIL |
| Runtime | N/A | **EMPTY** | FAIL | FAIL | FAIL |
| Services | N/A | **EMPTY** | FAIL | FAIL | FAIL |
| Python SDK | N/A | N/A | OK | OK | OK (~50) |
| CI | REAL | N/A | OK | OK | OK |
| Boot tests | N/A | N/A | OK | OK | OK |
