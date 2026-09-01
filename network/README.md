# network/ — Networking stack

**Charter (ARCHITECTURE.md §3):** the kernel TCP/IP stack, socket API, and
the segmentation that keeps control traffic isolated from everything else
(SECURITY.md §4). Ports: `SocketProvider`, `InterfaceDriver`
(ARCHITECTURE.md §5.2).

Scope:

- Phase 0: virtio-net on QEMU; Phase 1: TCP/IP MVP (`network.socket`
  capability); Phase 3: TSN support, network segmentation, secure boot
  integration (ROADMAP).

Boundary: drives `drivers/` via `hal/`; never parses untrusted input
without fuzzing (`tests/fuzz/` network targets). Consumed by
`communication/` (remote) via the SDK.
