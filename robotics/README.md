# robotics/ — Robotics application layer (Python)

**Charter (ARCHITECTURE.md §3):** end-to-end robotic behaviors: the
composition of perception → planning → motion into skills and tasks. The
first domain built on AEOS and the proving ground for the SDK.

Scope:

- skill/task libraries on `aeos_sdk` (motion, inspection, navigation
  skills),
- behavior tests run in `simulation/` scenarios and on HIL,
- owned by `@aeos/safety-subcommittee` (CODEOWNERS): every skill has a
  safety review and an interlock story.

Boundary: Python; never bypasses the SDK, never touches devices directly.
