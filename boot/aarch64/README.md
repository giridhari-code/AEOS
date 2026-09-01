# AEOS - AArch64 Boot Kernel

> Stage 2 (memory) + Stage 3 (interrupts) + Stage 4 (in-kernel AI core)

AEOS boot kernel for ARM64 (AArch64), targeting the QEMU `virt` machine.

## Directory Structure

```
boot/aarch64/
├── boot.S          # Entry: EL detection, EL2->EL1 drop, stack, BSS clear
├── exceptions.S    # Exception vector table + register save/restore stubs
├── kernel.c        # kernel_main: init sequence + memory tests
├── memory.h        # Shared API declarations
├── uart.h/.c       # PL011 console driver (shared)
├── pmm.c           # Physical Memory Manager (bitmap)
├── vmm.c           # Virtual Memory Manager (2MB blocks + MMU enable)
├── heap.c          # Kernel heap (bump allocator, 16MB)
├── gic.c           # GICv2 interrupt controller driver
├── timer.c         # ARM generic timer (EL1 virtual timer, CNTV)
├── irq.c           # IRQ dispatch + fault dump + tick subscription registry
├── switch.S        # Context switch (cpu_switch_to) + task trampoline
├── utils.c         # memset, memcpy, memcmp (freestanding)
├── ../kernel/ai.c     # In-kernel AI subsystem (fixed-point MLP policy)
├── ../kernel/sched.c  # Cooperative round-robin scheduler
├── linker.ld       # Memory layout
├── Makefile        # Build system (-O2 -Werror)
└── README.md
```

## CRITICAL: QEMU load address

QEMU's `-kernel` loader places raw AArch64 images at the ARM64 Image
protocol `text_offset` = **0x80000 into RAM**, i.e. VA **0x40080000** —
NOT at the RAM base. The linker script must use `KERNEL_BASE =
0x40080000` or every absolute address (VBAR_EL1, page tables, heap)
silently points at the wrong page. This caused a multi-hour silent-hang
bug; do not change it without re-verifying a full boot.

Other hard-won facts encoded in this code:

- **Vector table layout:** offsets 0x000-0x1FF are the *SP_EL0* group,
  0x200-0x3FF are the *SP_ELx* group. The kernel runs on SP_ELx
  (`msr spsel, #1` in boot.S), so live handlers live at 0x200+.
- **Exception stubs must save x30** (LR): the stub's own `bl`
  destroys the interrupted frame's return address otherwise.
- **Timer:** the EL1 *virtual* timer (CNTV, `s3_3_c14_c2_*`) is used;
  its PPI is INTID **30** on this machine (verified via GICD_ISPR).
  The physical timer (CNTP) traps to EL2 unless CNTHCTL_EL2 gates are
  opened. CNTFRQ is 62.5MHz on QEMU virt.
- **Faults are loud:** sync/SError handlers print ESR/FAR/ELR and halt.

## Build & Run

```bash
cd boot/aarch64
make run
```

## Output

```
Hello from AEOS
ARM64 Kernel Started Successfully!
=== Stage 2: Memory Management ===   ... all tests pass ...
=== Stage 3: Interrupt Handling ===  GICv2 + vector table + 100Hz timer
=== Stage 4: In-Kernel AI Core ===   fixed-point MLP (3-8-3, Q16.16)
--- System Report ---
  Timer ticks: 500
  Callback fired: 500 times
  AI inferences: 10
  AI last action: SUSTAIN
  AI timer policy: 100 Hz
=== AEOS running with interrupts + AI core. ===
```

## The AI engine (kernel/ai.c)

The AI is a proper kernel subsystem living in `kernel/ai.{h,c}`, built
into the image via the Makefile's `VPATH`:

- **Perceive** (every 50 ticks): heap pressure, page pressure, tick
  activity — normalized to Q16.16 fixed point.
- **Decide**: 3-8-3 MLP with softsign activations; argmax over
  {SUSTAIN, BOOST, REST}.
- **Act**: adjusts the timer rate (`timer_set_hz`, clamped 20-200Hz).

**Linking with the timer interrupt:** `ai_init()` calls
`tick_subscribe(ai_on_tick)`. The IRQ dispatcher (`irq.c`) keeps a
small static registry and invokes every subscriber after the timer
driver re-arms — no hardcoded AI references in the IRQ path.

**Safety:** ISR-safe (static state only, bounded fixed-size matmul,
reentrancy guard), fail-safe latch on state corruption (magic word +
error counter; kernel continues without AI; `ai_reset()` re-enables),
decision ring buffer + optional live UART logging via
`ai_set_verbose(1)`.

All integer arithmetic (`-mgeneral-regs-only`: no FP), no heap
allocation, deterministic weights baked at compile time. The host-side
mirror in `sdk/bindings/src/aeos_kernel.c` uses identical weights so
SDK simulations match on-target decisions.

## Build Targets

| Command      | Description                     |
|--------------|---------------------------------|
| `make`       | Build the kernel                |
| `make run`   | Build and run in QEMU           |
| `make debug` | Run with GDB server (port 1234) |
| `make clean` | Remove build artifacts          |
| `make size`  | Show binary size                |

## Memory Map

```
0x08000000    GIC distributor (+0x10000 CPU interface)
0x09000000    PL011 UART
0x40000000    RAM start (128MB)
0x40080000    Kernel image (QEMU Image text_offset)
0x41080000    Kernel heap (16MB, reserved from PMM)
0x41280000    PMM-managed free frames
0x48000000    End of RAM
```

## Next Stages

- ~~Stage 5: Scheduler~~ **DONE** — cooperative round-robin
  (`kernel/sched.c` + `switch.S`); 4-task demo in kernel_main;
  per-tick CPU accounting via the tick registry.
- **Stage 6:** Preemption (move the switch into the exception-return
  path), then syscalls + user mode (EL0).
- **Stage 7:** File system.
- **Stage 8:** SDK FFI boundary against a live kernel (serial bridge).
