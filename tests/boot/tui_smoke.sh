#!/usr/bin/env bash
#
# AEOS TUI smoke test: enter the dashboard via the `tui` command,
# verify all panels render over serial, exit with q.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$(mktemp /tmp/aeos-tui.XXXXXX.log)"
trap 'rm -f "$LOG"' EXIT

if ! command -v qemu-system-aarch64 >/dev/null 2>&1; then
    echo "[skip] qemu-system-aarch64 not installed"
    exit 0
fi

make -C "$ROOT/boot/aarch64" all >/dev/null

{
    sleep 13
    printf 'tui\r'
    sleep 4
    printf 'q'
    sleep 2
} | timeout 30 qemu-system-aarch64 \
    -machine virt -cpu cortex-a72 \
    -nographic -serial mon:stdio \
    -kernel "$ROOT/boot/aarch64/build/kernel.bin" \
    -m 128M -smp 1 >"$LOG" 2>&1 || true

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "FAIL: missing '$1'"
        tail -20 "$LOG"
        exit 1
    fi
}

expect "AEOS DASHBOARD"
expect "TASKS"
expect "MEMORY"
expect "AI / CMT ENGINES"
expect "back to shell"

echo "TUI smoke test passed"
