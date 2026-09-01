#!/usr/bin/env bash
#
# AEOS ISO smoke test: build/aeos.iso must boot in QEMU from the
# El Torito record (AEOS's own bootloader - no GRUB) and reach a
# working shell on the serial console.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-iso.XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "[skip] qemu-system-x86_64 not installed"
    exit 0
fi

make -C "$ROOT" build-iso >/dev/null

timeout 30 qemu-system-x86_64 \
    -cdrom "$ROOT/build/aeos.iso" \
    -serial stdio -display none -no-reboot >"$LOG" 2>&1 || true

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL: serial output missing '$1'"
        echo "--- last 40 lines ---"
        tail -40 "$LOG"
        exit 1
    fi
}

expect "AEOS kernel booted"
expect "Scheduler + Shell"
expect "Shell ready"

echo "ISO boot smoke test passed"
