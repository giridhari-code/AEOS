#!/usr/bin/env bash
set -euo pipefail

# Enforce the kernel unsafe budget (ARCHITECTURE.md §5.2, ADR-0002).
#
# `#![forbid(unsafe_code)]` is set workspace-wide in Cargo.toml. This check
# is the *explicit* exception: each crate declares a BUDGET line naming the
# number of allowed `unsafe` blocks and the reason. Adding unsafe without
# updating the budget (or the ADR that justifies it) fails CI.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

failures=0

BUDGET_RE='^// AEOS-UNSAFE-BUDGET:[[:space:]]+[0-9]+[[:space:]]+[A-Za-z0-9 _-]+$'

for crate in kernel runtime memory scheduler ipc security network storage filesystem services logging telemetry diagnostics plugins/cli cli plugins/runtime sdk/bindings; do
  [[ -d "$crate" ]] || continue
  manifest="$crate/Cargo.toml"
  [[ -f "$manifest" ]] || continue
  src_dir="$crate/src"
  [[ -d "$src_dir" ]] || continue

  count=$(grep -rInE '\bunsafe\b' "$src_dir" --include='*.rs' | grep -vE '^\s*//' | grep -c 'unsafe' || true)
  budget_line=$(grep -E 'AEOS-UNSAFE-BUDGET' "$manifest" || true)
  if [[ -n "$budget_line" ]]; then
    budget=$(echo "$budget_line" | sed -E 's/.*BUDGET:[[:space:]]+([0-9]+).*/\1/')
    reason=$(echo "$budget_line" | sed -E 's/.*BUDGET:[[:space:]]+[0-9]+[[:space:]]+(.*)/\1/')
    if [[ "$count" -gt "$budget" ]]; then
      echo "VIOLATION: $crate uses $count unsafe blocks, budget $budget (reason: $reason)" >&2
      failures=$((failures + 1))
    else
      echo "ok: $crate $count/$budget unsafe ($reason)"
    fi
  else
    # No budget declared: any unsafe is a violation (crate inherits forbid).
    if [[ "$count" -gt 0 ]]; then
      echo "VIOLATION: $crate has $count unsafe blocks but declares no AEOS-UNSAFE-BUDGET" >&2
      failures=$((failures + 1))
    fi
  fi
done

if [[ $failures -gt 0 ]]; then
  echo "FAIL: unsafe budget exceeded (see ARCHITECTURE.md §5.2, ADR-0002)" >&2
  exit 1
fi
echo "OK: unsafe budget within limits."
