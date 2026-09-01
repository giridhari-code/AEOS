#!/usr/bin/env bash
#
# AEOS networking smoke test.
#
# Asserts the virtio-net driver probes the NIC and brings the
# stack up with slirp defaults. (ICMP ping validation pending a
# known virtio-mmio queue quirk - tracked separately.)
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-net.XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "[skip] qemu-system-aarch64 not installed"
    exit 0
fi

make -C "$ROOT/boot/aarch64" all >/dev/null

timeout 60 qemu-system-aarch64 \
    -machine virt -cpu cortex-a72 \
    -global virtio-mmio.force-legacy=false \
    -nographic -serial mon:stdio \
    -kernel "$ROOT/boot/aarch64/build/kernel.bin" \
    -m 128M -smp 1 \
    -netdev user,id=n0 -device virtio-net-device,netdev=n0 \
    >"$LOG" 2>&1 || true

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL: missing '$1'"
        tail -30 "$LOG"
        exit 1
    fi
}

expect "=== Stage 9: Network (virtio-net) ==="
expect "virtio-net ready"
expect "stack up: 10.0.2.15/24"

echo "network smoke test passed"
