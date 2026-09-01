# AEOS Implementation Roadmap

> Dependency-ordered plan to turn AEOS into a real bootable Rust kernel.
> NOT based on TODO count — based on actual dependency chain.

---

## Dependency Chain

```
COMPILATION FIX
 ↓
PHYSICAL MEMORY (frame allocator)
 ↓
VIRTUAL MEMORY (page tables)
 ↓
TASK CONTEXT (register layout)
 ↓
CONTEXT SWITCH (architecture asm)
 ↓
SCHEDULER INTEGRATION
 ↓
SYSCALL DISPATCH
 ↓
IPC INTEGRATION
 ↓
SECURITY/CAPABILITIES
 ↓
DEVICE FRAMEWORK
 ↓
FILESYSTEM
 ↓
NETWORKING
 ↓
SERVICES/RUNTIME
 ↓
FULL SYSTEM INTEGRATION
 ↓
BOOT (Rust bare-metal)
```

---

## Slice 0: Fix Workspace Compilation [P0]

**Goal:** `cargo build --workspace` succeeds.

**Why first:** Nothing else can be tested until the workspace compiles.

**Tasks:**
1. Remove `telemetry` from workspace members (directory has no Cargo.toml)
2. Remove `sdk/bindings` from workspace members (it's a C project, not Rust)
3. Remove `aeos-telemetry` from workspace dependencies
4. Fix all `#![forbid(unsafe_code)]` + `static mut` contradictions across 6 crates

**For the `static mut` fix:** Replace `static mut X: Option<T>` + `unsafe { X.get_or_insert_with(...) }` with either:
- `spin::Once<T>` (from the existing `spin` dependency) — single-init, no mutation after init
- `spin::Mutex<Option<T>>` — if post-init mutation needed

**Affected files:**
- `Cargo.toml` — remove 2 members
- `kernel/src/lib.rs` — lines 20, 120, 127
- `memory/src/lib.rs` — lines 15, 77, 84
- `scheduler/src/lib.rs` — `static mut SCHEDULER`
- `ipc/src/lib.rs` — `static mut IPC_STATE`
- `security/src/lib.rs` — `static mut SECURITY_MANAGER`, `static mut NEXT_CAP_ID`
- `cli/src/lib.rs` — `static mut SHELL`

**Verification:** `cargo build --workspace 2>&1` exits 0.
`cargo clippy --workspace --all-targets 2>&1` exits 0.

---

## Slice 1: Fix Frame Allocator [P0]

**Goal:** `alloc_frame()` returns real physical frames instead of always `OutOfFrames`.

**Why:** Every subsystem above (page tables, scheduler stacks, IPC buffers) needs physical memory.

**Problem:** `init()` calculates `bitmap_words` but never:
- Allocates actual bitmap storage
- Marks reserved frames as allocated
- Initializes the free stack with available frames

**Implementation:**
1. Add a `static` bitmap buffer in `frame_alloc.rs` (max 256MB = 65536 frames = 1024 u64 words)
2. In `init()`: point `self.bitmap` at the static buffer, mark reserved frames (0–kernel_end), push free frames to free stack
3. Remove the empty `&mut []` pattern

**Constraints:**
- Must use `static` buffer (no heap in bare-metal)
- Must not use `unsafe` except for the static buffer access (which we're replacing with `spin::Once`)
- Bitmap must be properly aligned

**Verification:**
```rust
#[test]
fn alloc_returns_distinct_frames() {
    let mut a = FrameAllocator::new();
    a.init(0x4000_0000, 128 * 1024 * 1024).unwrap();
    let f1 = a.alloc_frame().unwrap();
    let f2 = a.alloc_frame().unwrap();
    assert_ne!(f1, f2);
}

#[test]
fn free_and_realloc() {
    let mut a = FrameAllocator::new();
    a.init(0x4000_0000, 128 * 1024 * 1024).unwrap();
    let f = a.alloc_frame().unwrap();
    a.free_frame(f).unwrap();
    let f2 = a.alloc_frame().unwrap();
    assert_eq!(f, f2);
}
```

---

## Slice 2: Fix Page Table Allocation [P1]

**Goal:** `PageTable::map()` creates real page table hierarchy.

**Why:** Virtual memory is required for process isolation and kernel/user separation.

**Problem:** `get_or_create_table()` casts a physical address to a raw pointer. New page tables are never allocated from the frame allocator.

**Implementation:**
1. `get_or_create_table()` must allocate a frame from the frame allocator for each new level
2. Zero the newly allocated page table frame
3. Return a reference to it (requires careful lifetime management or raw pointer + unsafe)
4. Implement `map()` to walk/create the 4-level hierarchy and set entry flags
5. Implement `unmap()` for page deallocation
6. Add TLB invalidation (x86 `invlpge`, aarch64 `TLBI`)

**Constraints:**
- This slice WILL require `unsafe` for raw pointer manipulation — must be minimal, documented, and isolated
- Page table allocation goes through the frame allocator (Slice 1 dependency)

**Verification:**
```rust
#[test]
fn map_single_page() {
    // Initialize allocator + page table
    // Map virtual 0xDEAD_0000 → physical 0x4000_1000
    // Verify entry exists with correct flags
}
```

---

## Slice 3: Fix Task Context Layout [P1]

**Goal:** `TaskContext` matches real CPU register layout for the target architecture.

**Why:** Context switch cannot work without correct register save/restore offsets.

**Problem:** `TaskContext` has `regs: [usize; 32]` with `regs[13]=SP`, `regs[14]=LR` — ARM64 indices on a generic array. Doesn't match the actual assembly `switch.S` which saves x19–x28, x29, x30 (12 slots).

**Implementation:**
1. Define architecture-specific `TaskContext` behind `#[cfg(target_arch)]`
2. AArch64: 12 slots matching `switch.S` offsets [0]–[11]: x19–x28, x29(fp), x30(lr), SP
3. x86_64: 12 slots matching `switch.S` offsets: rbx, r12–r15, rbp, rsp, rip, + fpu state
4. `TaskContext::new()` must set up the initial frame as if the task had been switched out once
5. Remove fake `fp_regs`/`simd_regs` (not saved in current assembly)

**Verification:** Compile against `switch.S` and verify offset constants match.

---

## Slice 4: Implement Real Context Switch [P1]

**Goal:** `arch::context_switch()` actually saves/restores CPU registers.

**Why:** Without this, the scheduler can select tasks but never switch to them.

**Implementation (Rust + inline asm or FFI to assembly):**
1. For AArch64: expose `cpu_switch_to(prev: *mut TaskContext, next: *mut TaskContext)` via `extern "C"` FFI to `switch.S`
2. For x86_64: expose `cpu_switch_to(prev: *mut TaskContext, next: *mut TaskContext)` via FFI to `switch.S`
3. `arch::context_switch()` in `kernel/src/arch.rs` must:
   - Get current task's context pointer
   - Get next task's context pointer
   - Call the assembly `cpu_switch_to`
4. Fix `halt()` to be called from the idle task, not as a standalone function

**Constraints:**
- Assembly is already written and tested in C (`switch.S`). The Rust side just needs to call it.
- Must use `extern "C"` FFI, not inline asm (preserve the existing tested assembly).

**Verification:** Two tasks alternate executing, each seeing different stack contents.

---

## Slice 5: Fix Scheduler Integration [P2]

**Goal:** Scheduler selects tasks AND switches to them.

**Implementation:**
1. Fix `RoundRobin::should_preempt()` to return true on time-slice expiry
2. Fix `Task::new()` to allocate stack via frame allocator instead of fake address
3. Wire `scheduler.tick()` → `arch::context_switch()` in the kernel timer handler
4. Implement idle task that calls `halt()`

**Verification:** Timer tick causes task preemption and round-robin scheduling.

---

## Slice 6: Implement Real Syscalls [P2]

**Goal:** Userspace SVC/INT 0x80 enters Rust syscall handler and performs real operations.

**Implementation:**
1. Wire assembly exception vectors to Rust `handle_syscall()`
2. Implement the 18 stub syscalls:
   - `Print` → UART write
   - `Ticks` → timer count
   - `Read`/`Write`/`Open`/`Close`/`Ioctl` → device framework
   - `Create`/`Delete`/`List` → filesystem
   - `TaskSpawn`/`TaskExit` → scheduler + proc
   - `IpcCreate`/`IpcSend`/`IpcRecv`/`IpcDestroy` → IPC
   - `WaitPid` → process wait
3. User pointer validation (port `is_user_ptr()` from C)

---

## Slice 7–12: Remaining Subsystems (Future)

Each depends on the previous:
- Slice 7: IPC integration with scheduler (block on empty recv)
- Slice 8: Security capabilities wired to syscalls
- Slice 9: Device framework (function pointer dispatch)
- Slice 10: Filesystem (port AFS or implement Rust equivalent)
- Slice 11: Networking (port ARP+ICMP or implement)
- Slice 12: Services + Runtime

---

## Immediate Target: Slice 0 + Slice 1

Starting NOW with:
1. Fix workspace compilation
2. Fix frame allocator

These are the two blockers preventing ANY Rust code from running.
