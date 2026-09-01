# boot/ — Bootstrap layer

**Charter (ARCHITECTURE.md §3):** the first bytes of AEOS, per architecture:
`boot/x86_64/`, `boot/aarch64/`, `boot/riscv64/`. Assembly lives only here
and in `kernel/src/arch/` (FFI constitution rule 4, ADR-0001).

Scope:

- power-on setup (GDT/IDT, UART bring-up, memory map discovery),
- handoff to the Rust kernel entry in `kernel/src/arch/`,
- nothing else: no drivers, no policy, no Rust.

Boundary: consumes only the `kernel/src/arch` entry contract; may set up
temporary page tables for the kernel's first `unsafe` windows.

Files expected: `boot.asm`/`boot.s` + `linker.ld` per arch, boot smoke
tests in `tests/boot/`.
