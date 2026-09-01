#!/usr/bin/env bash
#
# AEOS self-boot smoke test - the full self-hosting loop:
#
#   boot 1: OS comes up from a host-provisioned disk
#     -> `install` : the OS writes its OWN running image back to
#                    the boot volume as kernel.bin
#     -> `reboot`  : PSCI reset, BIOS reruns
#   boot 2: BIOS loads the kernel the OS ITSELF wrote to disk
#     -> persistent boot counter must read #2
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-selfboot.XXXXXX.log)"
DISK="$(mktemp /tmp/aeos-selfboot-disk.XXXXXX.img)"
trap 'rm -f "$LOG" "$DISK"' EXIT

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "[skip] qemu-system-aarch64 not installed"
    exit 0
fi

make -C "$ROOT/boot/aarch64" all >/dev/null
make -C "$ROOT/boot/bios" all >/dev/null

python3 "$ROOT/tools/build/disktool.py" format "$DISK" --size-mb 16 >/dev/null
python3 "$ROOT/tools/build/disktool.py" add "$DISK" \
    "$ROOT/boot/aarch64/build/kernel.bin" kernel.bin >/dev/null

{
    sleep 12
    printf 'install\r'
    sleep 8
    printf 'reboot\r'
    sleep 30
} | timeout 75 qemu-system-aarch64 \
    -machine virt -cpu cortex-a72 \
    -global virtio-mmio.force-legacy=false \
    -nographic -serial mon:stdio \
    -bios "$ROOT/boot/bios/obj/aeos-bios.bin" \
    -m 128M -smp 1 \
    -drive if=none,file="$DISK",format=raw,id=hd0 \
    -device virtio-blk-device,drive=hd0 >"$LOG" 2>&1 || true

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL: missing '$1'"
        tail -30 "$LOG"
        exit 1
    fi
}

expect "self-update:"
expect "rebooting..."

BOOT_COUNT="$(grep -ac 'LDR] AEOS-LDR' "$LOG")"
if [ "$BOOT_COUNT" -lt 2 ]; then
    echo "FAIL: expected >=2 boots after reboot, saw $BOOT_COUNT"
    tail -30 "$LOG"
    exit 1
fi

expect "\[self\] boot #1"
expect "\[self\] boot #2"

echo "self-boot smoke test passed ($BOOT_COUNT boots observed)"
