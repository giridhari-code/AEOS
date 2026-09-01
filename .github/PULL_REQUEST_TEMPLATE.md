## Summary

<!-- One paragraph: what and why. Reference the issue/ADR if any. -->

## Changes

<!-- Bullet list of user-visible or architectural changes. -->

## Checklist

- [ ] Small and single-purpose (< 400 lines diff unless justified by an ADR)
- [ ] `make ci` green locally (lint, unit, contract, integration, e2e, docs)
- [ ] Public symbols documented (docstring + module README API list updated)
- [ ] New events / message schemas / ports / error codes (`AEOS-<MOD>-<NNN>`)
      registered (COMMUNICATION.md §5, config/schemas/) with ADR if public
- [ ] New ports have contract tests (TESTING.md §3)
- [ ] FFI boundary respected: no `unsafe` outside the kernel-exception list,
      no Python touching hardware, no assembly outside `boot/` +
      `kernel/src/arch/`
- [ ] No secrets or PII; audit-worthy actions emit audit records
- [ ] Perf-sensitive change: benchmark added / existing one re-run
- [ ] Breaking change: deprecation path + CHANGELOG note
- [ ] Security-aware review performed if touching `security/`, `hal/`,
      `drivers/`, `device/`, `robotics/`, `communication/`, or `plugins/`

## Test plan

<!-- How the reviewer verifies the change: commands, scenarios, coverage. -->
