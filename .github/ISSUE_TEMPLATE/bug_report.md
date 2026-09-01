name: Bug report
description: Report a defect in AEOS
title: "[BUG] "
labels: [bug, triage]
body:
  - type: markdown
    attributes:
      value: |
        Before filing: search existing issues, and check whether the defect
        crosses the FFI boundary (assembly / Rust / C / Python).
  - type: input
    id: component
    attributes:
      label: Component
      description: Which directory/module is affected? (e.g. scheduler, hal, vision)
    validations:
      required: true
  - type: dropdown
    id: layer
    attributes:
      label: Layer
      options: [boot, kernel, hal/drivers, sdk, ai, plugins, tools]
    validations:
      required: true
  - type: textarea
    id: repro
    attributes:
      label: Reproduction steps
      description: Exact commands or scenario. Include environment (QEMU model, board, Python/Rust versions).
      placeholder: |
        1. make build-iso
        2. qemu-system-x86_64 -cdrom build/aeos.iso ...
        3. ...
    validations:
      required: true
  - type: textarea
    id: expected
    attributes:
      label: Expected behavior
    validations:
      required: true
  - type: textarea
    id: actual
    attributes:
      label: Actual behavior
    validations:
      required: true
  - type: textarea
    id: logs
    attributes:
      label: Logs / trace
      description: Paste logs, serial output, panic messages, or telemetry excerpts.
  - type: checkboxes
    id: safety
    attributes:
      label: Safety impact
      options:
        - label: This bug can cause unsafe motion, crash the kernel, or violate isolation
  - type: textarea
    id: fix
    attributes:
      label: Suggested fix (optional)
