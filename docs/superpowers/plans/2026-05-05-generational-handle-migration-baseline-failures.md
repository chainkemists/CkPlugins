# AutoTest baseline — pre-migration known-failing tests

Captured at branch `feature/generational-handle-migration` on 2026-05-05.
Per user direction during Pre-Flight PF.3:

> "Run only your tests. Skip all the other ones."

## Convention

The pre-migration AutoTest suite has known-failing tests unrelated to the
generational-handle migration. We **deliberately do not enumerate them here**
because:

- Enumeration would require a slow full-suite functional-test run (each test
  opens a level + drives PIE; 10-30+ minutes for the whole CkTests body).
- The known failures are not migration-related and chasing them is out of scope.
- The simpler discipline below is sufficient for migration safety.

## Migration test discipline

**During migration phases:** test runs use these filters only.

- **C++ unit tests:** `Filter: "Ck.Registry.*"` — covers the slot-table tests
  added in Phase 1 and the lifetime-inversion test added in Phase 0.
- **AS AutoTests (functional tests):** `Filter: "Project.Functional Tests.*Registry*"`
  — covers the new `CkAutoTest_Registry_*` tests added in Phase 0.

**Phase 6 / Phase 8 sweeps:** widen to `Project.Functional Tests` for cross-
module regression detection. **Pass criterion: every test added by this
migration is green.** Any pre-existing failure that was failing before the
migration may continue to fail; no new investigation required for those.

If a test that was not added by this migration starts failing AND was
believed-passing pre-migration, that's a regression to investigate. Otherwise,
ignore.

## Tests added by this migration (must be green at completion)

C++ unit tests (under `Ck.Registry.*`):
- `Ck.Registry.SlotTable.BasicAllocateFreeResolve`
- `Ck.Registry.SlotTable.UnsetHandleResolvesNullSilently`
- `Ck.Registry.SlotTable.GenerationWrapDoesNotCollideWithSentinel`
- `Ck.Registry.LifetimeInversion.HandleSurvivesRegistry`
- `Ck.Registry.LifetimeInversion.FreeAfterTableDestruction`
- `Ck.Registry.Benchmark.HandleCopyDestroy` (microbenchmark — informational)

AS AutoTests (under `Project.Functional Tests.*Registry*`):
- `CkAutoTest_Registry_HandleCopyDestroy`
- `CkAutoTest_Registry_HandleInFragmentLifecycle`
- `CkAutoTest_Registry_PieStartStopStress`

(The previously-planned `CkAutoTest_Registry_StaleHandleAfterPieStop` was
replaced with the C++ `LifetimeInversion.HandleSurvivesRegistry` test per
CTO review — the AS-side variant couldn't reach the same GC-ordering bucket
as the real bug.)
