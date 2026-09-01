#!/usr/bin/env bash
#
# AEOS GPT disk image smoke test.
#
# build/ajeeb.gos is ONE image that boots everywhere (.gos = the
# AEOS distribution format, like windows.iso):
#   - x86_64: SeaBIOS -> our MBR boot code -> kernel @ LBA 64
#   - aarch64: AEOS-BIOS -> virtio-blk -> AEOS-FS -> kernel.bin
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-gpt.XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

command -v qemu-system-x86_64 >/dev/null 2>&1 || { echo "[skip] no qemu"; exit 0; }

make -C "$ROOT" build-gos >/dev/null

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL(x86): missing '$1'"
        tail -30 "$LOG"
        exit 1
    fi
}

echo "== GPT image on x86_64 (own MBR code) =="
timeout 60 qemu-system-x86_64 \
    -drive file="$ROOT/build/ajeeb.gos",format=raw \
    -serial stdio -display none -no-reboot >"$LOG" 2>&1 || true
expect "AEOS kernel booted"
expect "Shell ready"

echo "== same GPT image on aarch64 (AEOS-BIOS) =="
if command -v qemu-system-aarch64 >/dev/null 2>&1 && \
   [ -f "$ROOT/boot/bios/obj/aeos-bios.bin" ]; then
    : >"$LOG"
    timeout 35 qemu-system-aarch64 \
        -machine virt -cpu cortex-a72 \
        -global virtio-mmio.force-legacy=false \
        -display none -serial stdio \
        -bios "$ROOT/boot/bios/obj/aeos-bios.bin" \
        -m 128M -smp 1 \
        -drive if=none,file="$ROOT/build/ajeeb.gos",format=raw,id=hd0 \
        -device virtio-blk-device,drive=hd0 >"$LOG" 2>&1 || true
    expect "AEOS-BIOS v"
    expect "AEOS-FS mounted"
    expect "Hello from AEOS"
    expect "Shell ready"
fi

echo ".gos image smoke test passed"
