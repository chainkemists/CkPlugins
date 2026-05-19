# Continuation Prompt — CkGoap Bundle/Tier Refactor (Phase 1+)

**One-line summary:** Spec + Plan + Phase 0 (cleanup) are landed and committed. Phase 1 (additive new data types) is blocked on a build-verification setup issue. Pick up from a green Phase 0 baseline.

---

## What's done

### Design artifacts (committed in CkPlugins root, branch `dev`)

- **Spec:** `docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md` — 16 design decisions locked, 14 new tests scoped, 9-phase roadmap.
- **Plan:** `docs/superpowers/plans/2026-05-19-CkGoap-BundleTierRefactor-plan.md` — step-by-step implementation plan covering Phase 0 through Phase 8.

### Phase 0 — Cleanup & stubs (committed)

**CkTests submodule** (commit `6b7ea0f` on `dev`):
- Deleted **15** HGOAP SharedWS test files under `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_SharedWS_*.as`.
- Removed the 15 corresponding `ACk_AutoTest_Goap_SharedWS_*_Actor` wrappers from `Plugins/CkTests/Script/Generated/CkTests_AutoTestActors.as`.

(Note — the original spec said "12 HGOAP tests" but the actual count was 15. All are removed.)

**CkGameplayDebugger submodule** (commit `5dd6a68` on `dev`):
- `Source/CkGoapDebugger/Public/CkGoapDebugger/Data/CkGoapDebugger_DataCollector.cpp` — full body stubbed; `Collect`, `CollectGoapEntity`, `TrackPlanCompletion`, `TrackSearchProgress` all return / do nothing. Includes pruned to just the .h and `Engine/World.h`.
- `Source/CkGoapDebugger/Public/CkGoapDebugger/Data/CkGoapDebugger_Types.h` — removed `class UCk_GoapGoal_EntityScript;` forward decl + the `TSubclassOf<UCk_GoapGoal_EntityScript> GoalClass` field on `FCkGoapDebugger_GoalInfo`. (`UCk_GoapGoal_EntityScript` is being deleted entirely in Phase 2.)

**CkPlugins root** (commit `cd2e23e` on `dev`):
- Pointer bumps for CkTests + CkGameplayDebugger.

The CkFoundation submodule has NOT been touched yet — that's all Phase 1+.

---

## What's blocking Phase 1

Phase 1 is purely additive (new fragment definitions, new param structs, new handles, new signals — all alongside existing planner-specific code, none removed yet). It should be low-risk to write.

**However:** I could not autonomously verify the build because the project's `EngineAssociation` GUID `{22D2B5AE-4AE5-C485-F291-F79F407369F4}` in `CkPlugins.uproject` is not registered in `HKCU:\Software\Epic Games\Unreal Engine\Builds`. The registered engines are:

- `{21E60FAC-48AD-69BF-42B6-E98C333A2E90}` → `D:/Repos/UnrealEngineAngelscript`
- `{946A086D-4CB9-124F-BC71-BCAB4C1DA079}` → `D:/Repos/UnrealEngineCk`

When I tried `Build.bat` directly with `D:/Repos/UnrealEngineCk/Engine/Build/BatchFiles/Build.bat`, UBT reported:

```
Unable to find plugin 'AngelscriptEnhancedInput' (referenced via CkPlugins.uproject).
Install it and try again, or remove it from the required plugin list.
```

Per `CLAUDE.md`, the recommended path is `& "$env:CLAUDE_PROJECT_DIR\CkAuto\Get-ProjectEnginePath.ps1"` — but `CkAuto/Get-ProjectEnginePath.ps1` does not exist in the current `CkAuto/` submodule version. Existing scripts in `CkAuto/` are submodule helpers + `UnrealToolbox.exe`.

Resolving this is outside the scope of the refactor work. The user's normal workflow probably uses runreal (`runreal build editor`) or the toolbox, both of which need the proper engine setup to be in place.

---

## How to resume

1. **Confirm the build harness works on your end.** Open the project in the editor as you normally do, or run the toolbox `build-test` workflow. If that succeeds against the current `dev` tip with the Phase 0 commits above, Phase 0 is verified and we can move on.

2. **Decide what to do about the missing `Get-ProjectEnginePath.ps1` helper.** Either:
   - Re-add it to `CkAuto/` (it's referenced from `CLAUDE.md`), or
   - Update `CLAUDE.md` to reflect the actual path (e.g. runreal-based, or invoke the toolbox).
   - This is independent of the refactor but unblocks autonomous build verification in future sessions.

3. **Tell the agent to start Phase 1.** The plan is detailed enough to execute task-by-task. Phase 1 lays down:
   - `FCk_Handle_Goap_Bundle`, `FCk_Handle_Goap_Tier` typesafe handles
   - `FCk_GoapWS_Condition_Authored` public condition struct
   - `_ActionTag` + `SetActionTag` on `UCk_GoapAction_EntityScript`
   - `FCk_Fragment_Goap_BundleParamsData`, `FCk_Fragment_Goap_TierParamsData`, `FCk_Fragment_Goap_RootParamsData`
   - Bundle ECS fragments (`_Params`, `_Current`, `_ActiveTiers`, `_TierCatalogIndex`, `RecordOfGoapTiers`)
   - Tier ECS fragments (`_Params`, `_Current`, `_Actions`, `_ActionClasses`, `_Requests`, `_ReplanThrottle`, `_SearchState`, `_Result`, `_PlanContext`)
   - `FFragment_RecordOfGoapBundles` + `FFragment_Goap_Root_Params` alias
   - 5 new signals (`Goap_OnPlanComplete`, `Goap_OnPlanFailed`, `Goap_OnActiveTiersChanged`, `Goap_OnTierActivated`, `Goap_OnTierDeactivated`)
   - 3 new gameplay tag categories (`Goap.Bundle`, `Goap.Tier`, `Goap.Action`)

   Phase 1 ends with a build-test gate — verify everything compiles before moving to Phase 2 (old API removal).

---

## Things to know for the agent

- The `FActionDef` struct in `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Algorithm/CkGoap_Types.h` will need two new fields in Phase 1: a parallel `Effects_RawKeys` list (or a `FGameplayTag` alongside each `FWorldStateEffect`) so the new chain-update processor can re-resolve effects against the child tier's registry. The plan flags this in Phase 4 Task 4.5's NOTE block but the actual struct change should land as part of the Phase 1 additive batch.
- There's a second consumer of going-away CkGoap types beyond `CkGoapDebugger`: `Plugins/CkGameplayDebugger/Source/CkEcsDebugger/Public/CkEcsDebugger/Inspectors/CkInspector_Goap.cpp` references `FFragment_Goap_Current`, `FFragment_Goap_Params`, `FFragment_Goap_Actions`, `FFragment_Goap_Goals`, `FFragment_Goap_Diagnostics` directly. These survive through Phase 1 (additive) but break in Phase 2. Plan to stub this inspector at the start of Phase 2, identical to the CkGoapDebugger_DataCollector stub.
- AS dynamic-handle regeneration via `UCkDynamicHandleSubsystem::GenerateHandleTypeRegistry` (Editor Subsystems panel) — required after Phase 1 lands the new handles, before AS-side tests in Phase 7 can reference them. Editor restart needed for AS bindings.
- Spec decisions locked in this session (in addition to the 6 from the prior brainstorm):
  - `_Plan[0]` IS first-to-execute in our existing ordering (verified at `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Processor.cpp:495` — `Algo::Reverse` flips the regressive A* path).
  - Strict full-tag equality for `Action.Tag == Tier.Tag`.
  - Synchronous goal injection at chain growth (not deferred).
  - Defer newly-appended tier's first plan to next frame.
  - Port `_InvalidGoalWorldState` diagnostic.
  - Imperative API (`Add`, `AddBundle`, `AddTier`, `AddAction`); no DataAsset-driven declaration.
  - Drop `UCk_GoapGoal_EntityScript` (goal is just a WorldState).
  - Multi-bundle: all simultaneous, per-bundle enable/disable toggle.
  - Root tier goal source: `TierParams._InitialGoal_RootOnly` + `Request_SetGoalWorldState` for updates.
  - Two separate specs (refactor now, debugger redesign in a follow-up session).
  - Phase-0 test handling: delete-at-start (15 files gone, in git history).
  - `Add` verb is kept on `UCk_Utils_Goap_UE`, repurposed for root-only construction.
  - CkGoapDebugger: data-collector body stubbed; UI shells preserved.

---

## Suggested first message

```
Continue the CkGoap Bundle/Tier refactor implementation. Phase 0 is committed
on `dev` across CkTests + CkGameplayDebugger + CkPlugins root. Spec at
docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md.
Plan at docs/superpowers/plans/2026-05-19-CkGoap-BundleTierRefactor-plan.md.

Confirm the editor builds against the current `dev` tip (toolbox build-test
or your normal flow). If green, start Phase 1 — additive new data types only.
Don't touch the old planner-per-entity surface; that's Phase 2.
```
