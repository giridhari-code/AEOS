# device/ — Device contracts (C)

**Charter (ARCHITECTURE.md §3):** the *contracts* for device classes, not
the drivers: motor controllers, cameras, LIDAR, IMUs, servos —
`device/include/aeos_device.h` plus one header per device class. Drivers
implement these contracts; the kernel and SDK consume them.

Scope:

- class headers + contract tests (`device/tests/`),
- device identity and capability mapping (`network.socket`, `motion.*`
  etc. resolve against these contracts),
- new device classes are additive and ADR-light: header + tests + HIL row
  (EXPANSION.md §2).

Boundary: C only, MISRA-aligned; no implementation beyond test doubles.
