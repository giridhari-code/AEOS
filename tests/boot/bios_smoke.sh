#!/usr/bin/env bash
#
# AEOS-BIOS smoke test: full disk-boot pipeline.
#
#   1. build kernel + BIOS
#   2. format an AEOS-FS image and inject kernel.bin (host-side tool)
#   3. boot QEMU with -bios ONLY (no -kernel) and assert the OS came
#      up from the virtual hard drive
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-bios-smoke.XXXXXX.log)"
DISK="$(mktemp /tmp/aeos-bios-disk.XXXXXX.img)"
trap 'rm -f "$LOG" "$DISK"' EXIT

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "[skip] qemu-system-aarch64 not installed"
    exit 0
fi

make -C "$ROOT/boot/aarch64" all >/dev/null
make -C "$ROOT/boot/bios" all >/dev/null

python3 "$ROOT/tools/build/disktool.py" format "$DISK" --size-mb 16
python3 "$ROOT/tools/build/disktool.py" add "$DISK" \
    "$ROOT/boot/aarch64/build/kernel.bin" kernel.bin

timeout 25 qemu-system-aarch64 \
    -machine virt -cpu cortex-a72 \
    -global virtio-mmio.force-legacy=false \
    -nographic -serial mon:stdio \
    -bios "$ROOT/boot/bios/obj/aeos-bios.bin" \
    -m 128M -smp 1 \
    -drive if=none,file="$DISK",format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 >"$LOG" 2>&1 || true

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL: serial output missing '$1'"
        echo "--- last 40 lines ---"
        tail -40 "$LOG"
        exit 1
    fi
}

expect "AEOS-BIOS v"
expect "virtio-blk ready"
expect "AEOS-FS mounted"
expect "kernel.bin:"
expect "jumping to AEOS"
expect "Hello from AEOS"
expect "Shell ready"

echo "BIOS boot smoke test passed"
