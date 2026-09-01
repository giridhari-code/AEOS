#!/usr/bin/env bash
set -euo pipefail

# Enforce the FFI constitution (ARCHITECTURE.md §2.1, ADR-0001):
#   1. Assembly only in boot/ and kernel/src/arch/
#   2. No unsafe in C or Python; Rust unsafe confined to the budget list
#      (tools/check/unsafe_budget.sh)
#   3. No forbidden imports across the language zones
#
# Exit code 1 with a report on any violation. Run via `make boundary-check`
# and in the boundary-checks CI job.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

failures=0

# 1. Assembly files anywhere outside boot/ and kernel/src/arch/
mapfile -t asm_files < <(find . -type f \( -name '*.asm' -o -name '*.s' -o -name '*.S' \) -not -path './target/*' -not -path './build/*' -not -path './.git/*' -not -path './vendor/*')
allowed=(boot kernel/src/arch)
if [[ ${#asm_files[@]} -gt 0 ]]; then
  for f in "${asm_files[@]}"; do
    ok=0
    # Strip leading ./ for comparison
    f_clean="${f#./}"
    for a in "${allowed[@]}"; do
      [[ "$f_clean" == "$a"* ]] && ok=1
    done
    if [[ $ok -eq 0 ]]; then
      echo "VIOLATION (asm out of zone): $f" >&2
      failures=$((failures + 1))
    fi
  done
fi

# 2. Unsafe keywords in C and Python are banned outright
if grep -rInE '\bunsafe\b' hal drivers device sdk perception vision audio planning reasoning learning simulation robotics communication \
    --include='*.c' --include='*.h' --include='*.py' 2>/dev/null | grep -v -E '\.(md|txt)$' >/dev/null; then
  echo "VIOLATION (unsafe in C/Python zone):" >&2
  grep -rInE '\bunsafe\b' hal drivers device sdk perception vision audio planning reasoning learning simulation robotics communication --include='*.c' --include='*.h' --include='*.py' >&2 || true
  failures=$((failures + 1))
fi

# 3. Rust crates must not import Python packages, and Python must not import
#    C/asm: no import lines in .py may reach past sdk/ for system access.
if grep -rInE '^(from|import) (cffi|ctypes|mmap|io\b)' sdk perception vision audio planning reasoning learning simulation robotics communication --include='*.py' 2>/dev/null | grep -v -E 'sdk/py/src/aeos_sdk/ffi\.py' >/dev/null; then
  echo "VIOLATION (Python touching native layers outside sdk):" >&2
  grep -rInE '^(from|import) (cffi|ctypes|mmap|io\b)' sdk perception vision audio planning reasoning learning simulation robotics communication --include='*.py' 2>/dev/null | grep -v -E 'sdk/py/src/aeos_sdk/ffi\.py' >&2 || true
  failures=$((failures + 1))
fi

# 4. Rust must never depend on Python (no serde_python / pyo3 in system crates)
if grep -rInE 'pyo3|cpython|python3-sys' kernel runtime memory scheduler ipc security network storage filesystem services logging telemetry diagnostics plugins cli --include='Cargo.toml' 2>/dev/null >/dev/null; then
  echo "VIOLATION (system Rust crate depends on Python)" >&2
  failures=$((failures + 1))
fi

# 5. C only imports hal/device/boot headers, never kernel Rust types
if grep -rInE '#include [<"](kernel|memory|scheduler|ipc|security|network)' hal drivers device --include='*.c' --include='*.h' 2>/dev/null >/dev/null; then
  echo "VIOLATION (C includes Rust-kernel header)" >&2
  failures=$((failures + 1))
fi

if [[ $failures -gt 0 ]]; then
  echo "FAIL: $failures boundary violation(s) (ARCHITECTURE.md §2.1)" >&2
  exit 1
fi
echo "OK: FFI boundary constitution holds."
