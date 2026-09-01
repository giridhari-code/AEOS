#!/usr/bin/env bash
#
# AEOS SMP smoke test: boot with 2 CPUs, assert that the secondary
# core comes online via PSCI CPU_ON and completes both queued
# parallel jobs.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-smp.XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "[skip] qemu-system-aarch64 not installed"
    exit 0
fi

make -C "$ROOT/boot/aarch64" all >/dev/null

timeout 120 qemu-system-aarch64 \
    -machine virt -cpu cortex-a72 \
    -nographic -serial mon:stdio \
    -kernel "$ROOT/boot/aarch64/build/kernel.bin" \
    -m 128M -smp 2 >"$LOG" 2>&1 || true

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL: missing '$1'"
        tail -30 "$LOG"
        exit 1
    fi
}

expect "=== Stage 8: SMP (secondary cores) ==="
expect "APs online:"
expect "cpu online:"
expect "parallel jobs done: 2/2"

echo "SMP smoke test passed"
