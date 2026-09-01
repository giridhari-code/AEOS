# Contributing to AEOS

AEOS is a multi-language OS (Assembly/Rust/C/Python) for embodied AI.
Contributions are welcome across every layer, from the boot sequence to
the learning stack. Read the [ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md)
master document first — its dependency graph and the FFI constitution
(§2.1) are normative.

- [1. Code of conduct](#1-code-of-conduct)
- [2. What we accept](#2-what-we-accept)
- [3. Workflow](#3-workflow)
- [4. Branching and commits](#4-branching-and-commits)
- [5. Review](#5-review)
- [6. ADRs](#6-adrs)
- [7. Versioning](#7-versioning)
- [8. Definition of done](#8-definition-of-done)
- [9. Ownership](#9-ownership)

## 1. Code of conduct

Be respectful and constructive. Disagreement is technical; critique the
design, not the person. Maintainers have final say on scope.

## 2. What we accept

- Bug fixes, with a failing test that reproduces the bug (TESTING.md §1).
- New subsystems, via ADR first (docs/adr/README.md §Process).
- Performance work, with benchmarks proving the win (benchmarks/).
- Documentation and tests for anything that was missing them.
- FFI-boundary-crossing changes only if they respect the constitution
  (ARCHITECTURE.md §2.1): no Python touching hardware, no `unsafe` outside
  the kernel-exception list, no assembly outside `boot/` +
  `kernel/src/arch/`.

We do not accept: placeholders masquerading as features, dead code,
undocumented public symbols, secrets, or changes that break the dependency
graph (ARCHITECTURE.md §4.4).

## 3. Workflow

1. **File or claim an issue.** Small changes can skip this, but say what
   you're doing in the PR.
2. **Fork / branch.** We use trunk-based development: short-lived branches
   off `main`, PRs merged fast. No long-running feature branches.
3. **Change, with tests.** New behavior ships with tests; changed public
   events/messages/schemas/error codes ship with registry updates
   (COMMUNICATION.md §5) and an ADR if needed.
4. **Run the gates locally**: `make ci` (lint, unit, contract, integration,
   e2e, docs). This mirrors the CI pipeline.
5. **Open a PR** against `main` using the template. Request the owning team
   from [CODEOWNERS](.github/CODEOWNERS).
6. **Address review.** Two approvals required for anything touching the
   kernel, HAL, drivers, device, or security; one otherwise.

## 4. Branching and commits

- Branch name: `<area>/<short-description>` (e.g. `scheduler/priority-preempt`).
- Commits: [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/),
  imperative mood, lowercase scope:
  `feat(scheduler): add priority preemption`, `fix(memory): correct frame
  leak on unmap`, `docs(adr): accept ADR-0002`. Allowed types: `feat`,
  `fix`, `docs`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `adr`.
- One logical change per commit; squash-and-merge on the PR (a single
  conventional message is the merged history).
- Signed commits: enable commit signing; CI rejects unsigned commits on
  release tags.

## 5. Review

- PRs must be smaller than ~400 lines of diff unless justified by an ADR.
- Reviewers verify: safety impact, FFI boundary compliance, contract-test
  coverage, error-code registration, benchmark evidence for perf claims.
- Merging rules (ci.yaml): lint-rust, lint-c, lint-py, boundary-checks,
  test-rust, test-c, test-py, boot-smoke, docs must be green.
- The Architecture Working Group reviews anything touching
  `docs/architecture/`, `docs/adr/`, or the master dependency graph.
- Never merge your own PR without a second pair of eyes.

## 6. ADRs

Any decision that affects public behavior, safety properties, or crosses an
FFI boundary needs an ADR (docs/adr/README.md). Accepted ADRs are
normative; a change contradicting one is rejected.

## 7. Versioning

AEOS follows SemVer 2.0.0 (DEVELOPMENT.md §5). The plugin ABI has its own
independent MAJOR.MINOR scheme (PLUGINS.md §5). Releases are cut from
`main` by maintainers only, after the HIL gate (hil.yaml).

## 8. Definition of done

A contribution is done when:

- [ ] `make ci` green locally
- [ ] public symbols documented; API reference regenerated if changed
- [ ] new events/messages/error codes registered (COMMUNICATION.md §5)
- [ ] new ports have contract tests (TESTING.md §2.4)
- [ ] benchmarks added/updated for perf-sensitive changes
- [ ] CHANGELOG updated (`Unreleased` section)
- [ ] no ADR violation (boundary-checks CI job passed)
- [ ] security-aware review done if touching security-sensitive areas
      (CODEOWNERS: safety-subcommittee, security-owners)

## 9. Ownership

Module ownership is defined in [.github/CODEOWNERS](.github/CODEOWNERS).
Owners triage issues and approve PRs for their areas. The Architecture
Working Group owns cross-cutting structure. Placeholder team names must be
replaced with real teams at first repository setup.

## Getting help

- Build/test questions: [DEVELOPMENT.md](DEVELOPMENT.md), [BUILD.md](docs/architecture/BUILD.md)
- Style: [CODE_STYLE.md](CODE_STYLE.md)
- Security issues: do **not** open a public issue — follow
  [SECURITY.md §8](SECURITY.md).
