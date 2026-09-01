#!/usr/bin/env bash
#
# AEOS boot smoke tests (aarch64/QEMU).
# Builds boot/aarch64 and asserts key boot milestones over the serial
# console. Skips (exit 0) when qemu-system-aarch64 is not installed.
#
# Usage: tests/boot/smoke_aarch64.sh
#

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-boot-smoke.XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "[skip] qemu-system-aarch64 not installed"
    exit 0
fi

make -C "$ROOT/boot/aarch64" all >/dev/null

run_qemu() { # <mem> <smp> <timeout-secs>
    timeout "${3:-12}" qemu-system-aarch64 \
        -machine virt -cpu cortex-a72 \
        -nographic -serial mon:stdio \
        -kernel "$ROOT/boot/aarch64/build/kernel.bin" \
        -m "$1" -smp "$2" >"$LOG" 2>&1 || true
}

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL: serial output missing '$1'"
        echo "--- last 40 lines ---"
        tail -40 "$LOG"
        exit 1
    fi
}

expect_count() { # <pattern> <expected-count>
    local got
    got="$(grep -c "$1" "$LOG" || true)"
    if [ "$got" != "$2" ]; then
        echo "FAIL: '$1' seen $got times, expected $2"
        echo "--- last 40 lines ---"
        tail -40 "$LOG"
        exit 1
    fi
}

echo "== smoke: 1 core, 128M (built-in RAM default) =="
run_qemu 128M 1
expect "AEOS-LDR stage-2"
expect "UART: 0x9000000"
expect "Hello from AEOS"
expect "Memory Management Working!"
expect "Shell ready"

echo "== smoke: 2 cores, 256M (DTB RAM parse + secondary parking) =="
run_qemu 256M 2
expect "DTB found @ "
expect "RAM: 256 MB (measured from DTB)"
expect "Memory Management Working!"
expect "Shell ready"
expect_count "Hello from AEOS" 1   # secondaries must stay parked

echo "== smoke: process model (EL0 address spaces) =="
run_qemu 128M 1 30
expect "=== Stage 6: Processes (EL0 address spaces) ==="
expect "spawned 'ping' pid=1"
expect "spawned 'pong' pid=2"
expect "\[ping\] hello from EL0 process land"
expect "\[pong\] process model alive"
expect "'ping' exited code=7"
expect "'pong' exited code=9"
expect "reaped pid 1, code=7"
expect "reaped pid 2, code=9"
expect "all children reaped - process model OK"

echo "== smoke: storage persistence (two runs, one disk image) =="
DISK="$(mktemp /tmp/aeos-disk.XXXXXX.img)"
dd if=/dev/zero of="$DISK" bs=1M count=16 2>/dev/null

run_qemu_disk() {
    timeout 12 qemu-system-aarch64 \
        -machine virt -cpu cortex-a72 \
        -global virtio-mmio.force-legacy=false \
        -nographic -serial mon:stdio \
        -kernel "$ROOT/boot/aarch64/build/kernel.bin" \
        -m 128M -smp 1 \
        -drive if=none,file="$DISK",format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0 >"$LOG" 2>&1 || true
}

run_qemu_disk
expect "virtio-blk ready"
expect "AEOS-FS formatted"          # blank disk -> format
expect "write+read verify OK"

run_qemu_disk                        # second boot, SAME disk
expect "AEOS-FS mounted"            # existing FS must be recognized
expect "write+read verify OK"       # data survived the reboot
rm -f "$DISK"

echo "boot smoke tests passed"
