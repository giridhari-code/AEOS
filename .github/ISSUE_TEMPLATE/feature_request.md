name: Feature request
description: Propose a new capability or subsystem
title: "[FEATURE] "
labels: [enhancement, triage]
body:
  - type: textarea
    id: problem
    attributes:
      label: Problem statement
      description: What capability is missing, and which embodied-AI use case needs it?
    validations:
      required: true
  - type: textarea
    id: proposal
    attributes:
      label: Proposed solution
      description: Behavior, interfaces (events/ports/schema), affected directories.
    validations:
      required: true
  - type: dropdown
    id: scope
    attributes:
      label: Scope
      options:
        - New subsystem or directory
        - Extension of an existing subsystem
        - New transport or plugin capability
        - Developer tooling / CI
        - Documentation
    validations:
      required: true
  - type: checkboxes
    id: impact
    attributes:
      label: Impact check
      options:
        - label: Crosses an FFI boundary (assembly/Rust/C/Python)
        - label: Adds or changes a public event, message schema, or error code (needs ADR + registry)
        - label: Affects safety properties (motion, isolation, emergency behavior)
        - label: Affects ABI stability or plugin compatibility
  - type: textarea
    id: alternatives
    attributes:
      label: Alternatives considered
