# drivers/ — Device drivers (C)

**Charter (ARCHITECTURE.md §3):** the driver implementations behind
`hal/`'s bus abstraction: `drivers/src/bus/` (PCI, I2C, SPI, virtio) and
`drivers/src/device/` (specific devices). `drivers/include/aeos_driver.h`
is the driver registration contract.

Scope:

- Phase 0: virtio-net/virtio-blk on QEMU; Phase 1: emulated boards;
  Phase 3: real SoCs (ROADMAP),
- every driver ships a contract test (`drivers/tests/`) and a HIL matrix
  row before it is trusted (TESTING.md §4.2),
- C17 MISRA-aligned; owned by `@aeos/safety-subcommittee` (CODEOWNERS).

Boundary: consumes `hal/` and `device/` headers only.
