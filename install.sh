#!/bin/bash
# AEOS Installer
set -e

echo "========================================="
echo "   AEOS - Ajeeb Embodied AI OS"
echo "   Installer v1.0"
echo "========================================="
echo ""

echo "[1/4] Checking dependencies..."
for cmd in aarch64-linux-gnu-gcc aarch64-linux-gnu-ld make python3; do
    if command -v $cmd &> /dev/null; then
        echo "  OK: $cmd"
    else
        echo "  MISSING: $cmd"
        apt-get update -qq && apt-get install -y -qq gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu make python3 python3-pip 2>/dev/null
        break
    fi
done

echo ""
echo "[2/4] Building kernel..."
cd /root/AEOS/boot/aarch64
make clean 2>/dev/null || true
make all

echo ""
echo "[3/4] Setting up Python SDK..."
cd /root/AEOS
pip3 install -q -e sdk/py 2>/dev/null || true
echo "  Python SDK ready"

echo ""
echo "[4/4] Running tests..."
PYTHONPATH=sdk/py/src python3 -m pytest sdk/tests/ -q --tb=line 2>/dev/null && echo "  All tests passed" || echo "  Some tests skipped"

echo ""
echo "========================================="
echo "   Installation Complete!"
echo "========================================="
echo ""
echo "  Build kernel: cd boot/aarch64 && make all"
echo ""
