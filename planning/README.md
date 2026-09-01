# planning/ — Motion & task planning (Python)

**Charter (ARCHITECTURE.md §3):** the decision layer: perception-derived
state in, `PlanReady` plans out; trajectories are dispatched only via the
SDK's motion capability, never by direct device access.

Scope:

- planning loop consuming `PerceptBundleReady` and emitting `PlanReady`
  (COMMUNICATION.md §3),
- `PlanRejected` + replan paths; safety checks before any dispatch,
- scenario-based validation in `simulation/scenarios/`.

Boundary: Python; `motion.request` requires a human-audited capability
grant (SECURITY.md §3, PLUGINS.md §7). The safety interlock
(`SafetyInterlockTriggered`) always overrides plans.
