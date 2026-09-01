# examples/ — Runnable examples

**Charter (ARCHITECTURE.md §3):** minimal, runnable, *reviewed* examples per
language zone — `examples/rust/`, `examples/c/`, `examples/python/`,
`examples/assembly/`. Examples are documentation with an entry point.

Rules:

- each example runs from a README one-liner and exits cleanly,
- CI compiles them (they are part of the workspaces or have their own
  build job),
- no example may require hardware that CI lacks (use `simulation/`).

No placeholder code: if an example can't be run, it isn't merged
(CONTRIBUTING §2).
