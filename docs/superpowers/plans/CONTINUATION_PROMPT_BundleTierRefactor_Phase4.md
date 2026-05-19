# Continuation Prompt — CkGoap Bundle/Tier Refactor (Phase 4+)

**One-line summary:** Phases 0-3 complete and build-verified green. New imperative API surface is in place (Add / AddBundle / AddTier / AddAction + queries + per-tier requests + signals). Old planner-per-entity surface is fully removed. Phase 4 (processors) is the heavyweight remaining work.

---

## What's done (all committed on `dev`, none pushed)

### Design artifacts

- **Spec:** `docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md` (committed `f96c8fa`)
- **Plan:** `docs/superpowers/plans/2026-05-19-CkGoap-BundleTierRefactor-plan.md` (committed `fe7e49a`)
- **Phase 1+ continuation (now superseded):** `CONTINUATION_PROMPT_BundleTierRefactor_Phase1.md`

### Phase 0 — Cleanup & stubs

**CkTests submodule** (commit `6b7ea0f`):
- Deleted 15 HGOAP SharedWS test files.
- Removed corresponding generated AutoTestActors wrappers.

**CkGameplayDebugger submodule** (commits `5dd6a68`, `3725eb3`):
- Stubbed `CkGoapDebugger_DataCollector.cpp`.
- Removed `UCk_GoapGoal_EntityScript` reference from `CkGoapDebugger_Types.h`.
- Stubbed `CkInspector_Goap.cpp` (in CkEcsDebugger).

### Phase 1 — Additive new types (committed `68ae3adf5` in CkFoundation)

- New handles: `FCk_Handle_Goap_Bundle`, `FCk_Handle_Goap_Tier`.
- New param structs: `FCk_Fragment_Goap_RootParamsData`, `_BundleParamsData`, `_TierParamsData`.
- Public condition: `FCk_GoapWS_Condition_Authored`.
- 8 per-tier USTRUCT requests (`FCk_Request_Goap_Tier_Plan` / `_CancelPlan` / `_SetGoal` / `_SetActionCost` / `_SetReplanInterval` / `_SetReplanPolicy` / `_SetSearchBudget` / `_SetCostThreshold`).
- Signal payloads + delegates: `OnActiveTiersChanged`, `OnTierActivated`, `OnTierDeactivated`.
- Bundle ECS fragments: `_Params`, `_Current`, `_ActiveTiers`, `_TierCatalogIndex`. Bundle tags: `_RequiresSetup`, `_RequiresChainUpdate`.
- Tier ECS fragments: `_Params`, `_Current`, `_ActionClasses`, `_Actions`, `_Requests`, `_ReplanThrottle`, `_PlanContext`, A* aliases. Tier tags: `_RequiresSetup`, `_RequiresInitialPlan`, `_PlanRequested`.
- 5 new signals: `OnGoap_Bundle_ActiveTiersChanged`, `OnGoap_Tier_PlanComplete`, `OnGoap_Tier_PlanFailed`, `OnGoap_Tier_Activated`, `OnGoap_Tier_Deactivated`.
- `UCk_GoapAction_EntityScript._ActionTag` + `SetActionTag` builder + `Get_ActionTag` accessor.
- `goap::FActionDef.ActionTag` field.

### Phase 2 — Old planner-per-entity API removed (committed `f511c38c1` in CkFoundation)

- Deleted `UCk_GoapGoal_EntityScript.{h,cpp}`.
- Removed `goap::FGoalDef`.
- Stripped `FCk_Fragment_Goap_ParamsData`, all 6 planner-scoped processors, all planner-scoped requests/payloads/signals/delegates/utils.
- `FCk_Handle_Goap` repurposed: now "Goap root container" (one per entity, holds `FFragment_RecordOfGoapBundles`).
- Minimal new root verbs: `Add`, `Has`, `Cast`, `Find_Bundle` — implemented and green.

### Phase 3 — Bundle + Tier utils (committed `be707461b` in CkFoundation)

- `UCk_Utils_Goap_Bundle_UE` — full surface: `AddBundle`, `Has`, `Find_Tier`, `Get_ActiveTiers`, `Get_EnableToggle`, `Get_DependencyCycles`, `Request_SetEnableToggle`, `Request_ResetActiveTiers`, `BindTo_/UnbindFrom_OnActiveTiersChanged`.
- `UCk_Utils_Goap_Tier_UE` — full surface: `AddTier`, `AddAction`, `Has`, all `Get_*` queries (PlanStatus, Plan, PlanCost, WorldStateSource, ActiveParentAction, InvalidGoal), all `Request_*` verbs (SetGoalWorldState, Plan, CancelPlan, SetActionCost, SetReplanInterval, SetReplanPolicy, SetSearchBudget, SetCostThreshold), `BindTo_/UnbindFrom_` for 4 tier signals.
- Record fragments live in **private** headers (`Bundle/CkGoap_Bundle_Record_Internal.h`, `Tier/CkGoap_Tier_Record_Internal.h`) — included only by `*_Utils.cpp` files so CkRecord doesn't leak into public headers.

**All builds verified green via `./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor`.**

---

## What's NOT yet implemented

### Phase 4 — Processors (the heavyweight remaining work)

None of the per-tier processors exist yet. The `_Tier_Requests` queue accumulates requests but no processor drains it. Tiers won't plan, chains won't update.

Five processors to write:

1. **`FProcessor_Goap_Tier_Setup`** — extract action CDOs into `FActionDef`s; register WS keys; handle root-tier initial-goal seeding from `_InitialGoal_RootOnly`. Port from the old `FProcessor_Goap_Setup` in CkGoap_Processor.cpp (git history). Key change: per-tier scope; reads `_WorldStateSource_Resolved` from the tier's `_Current` fragment.

2. **`FProcessor_Goap_Tier_AutoReplan`** — consume `FTag_Goap_Tier_RequiresInitialPlan` / `FTag_Goap_Dirty_WorldState` / `FTag_Goap_Dirty_Cost`; respect policy + throttle; enqueue `FCk_Request_Goap_Tier_Plan`.

3. **`FProcessor_Goap_Tier_HandleRequests`** — drain `FFragment_Goap_Tier_Requests`. Plan request must build a `FGoapGraph` from `_Actions`, seed A* via the tier's `_SearchState`, and update `_PlanStatus`. Other requests mutate WS/cost/policy/etc.

4. **`FProcessor_Goap_Tier_HandleResult`** — convert completed A* path → action sequence in `_Plan`; fire `OnGoap_Tier_PlanComplete` / `OnGoap_Tier_PlanFailed` via `UUtils_Signal_*::Broadcast`. The reverse step (regressive→execution order) is `Algo::Reverse` per the verified pattern in the old code.

5. **`FProcessor_Goap_Bundle_ChainUpdate`** ⭐ NEW — the truncate/extend rule per spec §4.3. Walks each bundle's ActiveTiers top-down. Reads each tier's `_Plan[0]`. Matches `_Plan[0].Get_ActionTag()` against `_TierCatalogIndex._TagToTier`. Truncates or extends. Injects goal **synchronously** at append time (per `DoInjectGoalSynchronous` pattern in spec §5.3 — use `FKeyRegistry::GetTag(Key)` to round-trip parent's keyed effect through child's registry). Defers newly-appended tier's first plan to next frame (via `FTag_Goap_Tier_RequiresInitialPlan`).

Plus: wire `TProcessor_AStar_Execute<FProcessor_Goap_Tier_Execute, ..., FFragment_Goap_Tier_SearchState, FFragment_Goap_Tier_Result>` — register via `CK_REGISTER_PROCESSOR`. Same for `TProcessor_AStar_EndPlay` if cleanup is needed.

**Files to create:**
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Processor.h` + `.cpp`
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Processor.h` + `.cpp`

The plan in `docs/superpowers/plans/2026-05-19-CkGoap-BundleTierRefactor-plan.md` Phase 4 has detailed task lists.

### Phase 5 — Subscriber/dirty plumbing retype

The WS subscriber list is currently `TArray<FCk_Handle_Goap>` (in `WorldState/CkGoap_WorldState_Fragment.h`). Tier subscribers won't fit — needs to retype to `TArray<FCk_Handle>` (generic). Same change for `Request_AddPlannerSubscriber` (rename to `Request_AddSubscriber`, take generic handle). Then wire subscribe/unsubscribe into `ChainUpdate` at activation / deactivation.

### Phase 6 — Diagnostics

- `_InvalidGoalWorldState` population (during synchronous goal injection in ChainUpdate).
- Setup-time tag-conflict + dep-cycle detection in a new `FProcessor_Goap_Bundle_Setup`.

### Phase 7 — Tests

14 new tests scoped in spec §9. Existing CkTests/Script/CkGoap is empty (all old files deleted in Phase 0/2).

### Phase 8 — Docs polish

Update `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` for the Bundle/Tier model.

---

## How to resume

1. Confirm the current `dev` tip on CkFoundation builds via `./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --output=Saved/Logs/Build-Editor.log --project=D:\Repos\CkPlugins`. (Should be green out-of-the-gate — last commit `be707461b` was build-verified.)

2. Start Phase 4 — the plan has step-by-step tasks. Reasonable execution order:
   - 4.1: `FProcessor_Goap_Tier_Setup` (CDO extraction; port from old code). Build-verify.
   - 4.2: `FProcessor_Goap_Tier_HandleRequests` (request drain; port from old code). Build-verify.
   - 4.3: `FProcessor_Goap_Tier_HandleResult` (A* path → plan; port). Build-verify.
   - 4.4: `FProcessor_Goap_Tier_AutoReplan` (port). Build-verify.
   - 4.5: `FProcessor_Goap_Bundle_ChainUpdate` (new logic). Build-verify.
   - 4.6: Wire `TProcessor_AStar_Execute` per-tier. Build-verify.

3. After Phase 4, the bundle/tier model is functionally complete but missing subscriber wiring (Phase 5) and diagnostics polish (Phase 6). Tests in Phase 7 close the loop.

---

## Things to know for the agent

- **`FActionDef.ActionTag` is populated at Setup time** from the action's CDO `Get_ActionTag()`. The chain-update reads `Plan[0].GetDefaultObject()->Get_ActionTag()`.
- **`FKeyRegistry::GetTag(FCk_GoapKey)`** does reverse lookup. ChainUpdate's `InjectGoalSynchronous` uses this to round-trip the parent's keyed `FWorldStateEffect` → raw `FGameplayTag` → child's `FCk_GoapKey`. No need for parallel raw-tag arrays.
- **`_Plan[0]` IS first-to-execute** in our ordering (verified at `CkGoap_Processor.cpp:495` — `Algo::Reverse` flips the regressive A* path). The chain-update logic reads `Plan[0]`, not `Plan.Last()`.
- **Synchronous goal injection** (decision Q3): write directly to child tier's `_Current._Goal` at append time, no request queue.
- **Defer first-plan one frame** (decision Q4): newly-appended tier gets `FTag_Goap_Tier_RequiresInitialPlan` and AutoReplan picks it up next frame.
- **The `AddTier` already validates root must have `_WorldStateSource_Override`** — warns + leaves `_WorldStateSource_Resolved` invalid. Setup processor must skip planning when resolved WS is invalid.
- **The CkInspector_Goap stub returns `CanInspect = false`** — the panel never appears. Once Phase 4 lands, you may want to detect `FFragment_Goap_Tier_Current` (or `_Params`) to make the stub conditionally visible while the redesign is pending.
- **`Plugins/CkGameplayDebugger/Source/CkGoapDebugger`** still has UI shells that read empty arrays from the stubbed data collector. They render fine but contain no content. Full rewrite in the follow-up debugger redesign spec.
- **No AS bindings refresh needed** since Phase 1 — handles are already registered. If `UCkDynamicHandleSubsystem::GenerateHandleTypeRegistry` was never run after Phase 1 commits, run it in the editor before any AS-side work in Phase 7.

---

## Suggested first message for the next session

```
Continue the CkGoap Bundle/Tier refactor. Phases 0-3 are committed and
build-green on `dev`. Spec at
docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md.
Plan at docs/superpowers/plans/2026-05-19-CkGoap-BundleTierRefactor-plan.md.

Start Phase 4 — the per-tier processors (Setup/HandleRequests/HandleResult/
AutoReplan) and the new Bundle ChainUpdate. Most of the per-tier logic ports
from the old code in git history (commits before `f511c38c1` in CkFoundation
have the planner-era processor implementations). The ChainUpdate is brand-new
logic — see spec §4.3 for the truncate/extend pseudocode.

Use `./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor` to
verify between tasks. After each processor lands and is green, commit.
```
