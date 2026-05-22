# CkGoap U11 PR-B.1b — Continuation prompt (BLOCKED at survey)

**Status:** Prior dispatch BLOCKED with full survey + design decisions captured here. No code changes were made. Root state unchanged at `2a27fb6` with submodule pointers:
- CkFoundation: `75ac296be` (PR-B.1a Planner verb shims through `_RootAction`)
- CkTests: `9c81cc0` (PR-B.1a — 57/71 callsites migrated; 14 remaining)
- CkGameplayDebugger: `304c62cc0`
- 19/19 `Goap_Planner_*` AutoTests pass in Development and DebugGame.

Read first:
1. `docs/superpowers/plans/2026-05-22-CkGoap-U11-PR-B-AStar-pipeline-on-Planner.md` — Phase B.1 plan.
2. `docs/superpowers/specs/2026-05-21-CkGoap-PlannerActionCollapse-design.md` — §2.3 fragment table, §3 API, §4 processor flow + §4.2 pseudocode.
3. `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` — current architecture overview.

---

## Why the previous dispatch blocked

Atomic B.1b is ~3000 LOC across 6 CkFoundation source files + AS test migrations + debugger DataCollector updates. The prior dispatch surveyed the full surface (captured below), made design decisions (captured below), but determined safe completion in one dispatch would risk breaking 19 tests with a half-applied refactor. Subdivision permission was exercised: produce this handoff so the next dispatch starts with a complete plan rather than re-surveying.

---

## What B.1b does (recap)

Move the A* pipeline + Planner-role fragments off the implicit-root Action and onto the Planner entity. Drop the `_RootAction` indirection introduced in PR-A as a transitional shim. After landing:
- A Planner runs A* directly over its registered child Actions.
- Atomic Actions carry only Action-role fragments (lean).
- `_RootAction` field is deleted.
- Planner-API verb shims (added in B.1a) become first-class implementations on Planner-side fragments — the `_RootAction is valid?` ensures go away.
- 14 remaining test callsites that still call `utils_goap_action::Get_PlanStatus(subAction)` (and friends) on promoted sub-Actions migrate to `utils_goap_planner::Get_PlanStatus(utils_goap_planner::Cast(subAction))`.

---

## State at start

### CkFoundation HEAD (commit `75ac296be`)

**Fragments — Action-side (`Public/CkGoap/Action/CkGoap_Action_Fragment.h`)**:
- `FFragment_Goap_Action_Definition` (Action-role discriminator) — preconditions, effects, cost, cached ActionDef.
- `FFragment_Goap_Action_Params` (= `FCk_Fragment_Goap_ActionParamsData`) — class, WS override, per-Action planner knobs (SearchBudget, CostThreshold, ReplanPolicy, MinReplanInterval, PlanOnStart).
- `FFragment_Goap_Action_Tree` — `_ParentAction`, `_ChildActions` (typed as `TArray<FCk_Handle_Goap_Action>`).
- `FFragment_Goap_Action_Current` — `_ActiveParentAction` (parent action class breadcrumb).
- `FFragment_Goap_Action_ActionClasses` — legacy collection, unused at runtime.
- `FFragment_Goap_Action_Requests` — variant queue {Plan, CancelPlan, SetGoal (Planner), SetActionCost, SetReplanInterval, SetReplanPolicy, SetSearchBudget, SetCostThreshold}.
- `FFragment_Goap_Action_ReplanThrottle` — `_SecondsSinceLastReplan`.
- `FFragment_Goap_Action_PlanContext` — `_Graph` (FGoapGraph kept alive between search + result).
- `FFragment_Goap_Action_SearchState` (alias `TFragment_AStar_SearchState<int32, FGoapGraph>`).
- `FFragment_Goap_Action_Result` (alias `TFragment_AStar_Result<int32>`).
- Tags: `FTag_Goap_Action_{RequiresSetup, PlanRequested, RequiresInitialPlan, PlanInFlight}`.
- Signals: `OnGoap_Action_PlanComplete`, `OnGoap_Action_PlanFailed`, `OnGoap_Planner_Activated`, `OnGoap_Planner_Deactivated` (all with source `FCk_Handle_Goap_Action`).

**Fragments — Planner-side (`Public/CkGoap/Planner/CkGoap_Planner_Fragment.h`)**:
- `FFragment_Goap_Planner_Params` (= `FCk_Fragment_Goap_PlannerParamsData`) — `_PlannerTag`, `_InitialToggle`, `_Goal`, `_WorldStateSource`.
- `FFragment_Goap_Planner_Current` — `_EnableToggle`, `_DependencyCycles`, **`_RootAction` (DELETE in B.1b)**.
- `FFragment_Goap_Planner_Activation` — `_LastActivatedPlan0`, `_IsActive` (today its `_LastActivatedPlan0` is typed `FCk_Handle_Goap_Action`; stays Action because Plan[0] is always an Action handle).
- `FFragment_Goap_Planner_ActionCatalogIndex` — `_TagToAction` map.
- `FFragment_Goap_Planner_WorldStateSource` — `_WorldStateSource` (override) + `_Resolved`.
- `FFragment_Goap_Planner_PlanState` — `_PlanStatus`, `_Plan` (TArray<FCk_Handle_Goap_Action>), `_PlanCost`, `_PlanAttemptCount`.
- `FFragment_Goap_Planner_Goal` — `_GoalAuthored`, `_Goal` (resolved), `_InvalidGoal`.
- Tags: `FTag_Goap_Planner_RequiresSetup`, `FTag_Goap_Planner_RequiresChainUpdate` (unused, can delete).
- Signal: `OnGoap_Planner_ActiveChainChanged` (source `FCk_Handle_Goap_Planner`).

**Processors (`Action/CkGoap_Action_Processor.{h,cpp}` and `Planner/CkGoap_Planner_Processor.{h,cpp}`)**:
- All Action-matched (`FCk_Handle_Goap_Action`): `FProcessor_Goap_Action_Setup`, `_AutoReplan`, `_HandleRequests`, `_Execute` (`TProcessor_AStar_Execute` template), `_HandleResult`, `_EndPlay` (`TProcessor_AStar_EndPlay` template).
- Planner-matched: `FProcessor_Goap_Planner_Setup` (per-Planner Tarjan SCC); `FProcessor_Goap_Planner_UpdateActivation` (currently matches **Action** despite the name — per U11.2's transitional model — walks the entity's Plan[0] for activation).

**Construction (`Planner/CkGoap_Planner_Utils.cpp`)**:
- `Add(Owner, PlannerParams)` stamps on the Planner entity: `_Params`, `_Current`, `_ActionCatalogIndex`, `_WorldStateSource`, `_PlanState`, `_Goal`, `_Activation` (with `_IsActive=true`), `FTag_Goap_Planner_RequiresSetup`. Registers in `FFragment_RecordOfGoapPlanners` on the owner.
- `AddAction(Planner, ActionParams)`:
  - Calls `DoCreateOrFindActionEntity` which stamps full Action-role + a dual-role Planner-role cluster (PlanState, Goal, WorldStateSource, Activation) on the new Action.
  - Discriminates between top-level Planner (first AddAction creates implicit-root) and promoted mid-tier Planner (every AddAction adds a tree child of the host).
  - For top-level + first call: sets `_Current._RootAction = ActionEntity`, propagates Planner's `_Goal` → Action's `_Goal`, resolves Action's `_Resolved` WS, subscribes Action to WS, sets Action's `_IsActive=true`.
  - For top-level + subsequent: wires Action as tree child of the implicit root.
- `PromoteActionToPlanner(Action, PlannerParams)` stamps Planner-role discriminator (`_Params`, `_Current`, `_ActionCatalogIndex`) on the Action; reuses the existing dual-role Planner-role cluster (PlanState, Goal, WorldStateSource, Activation) the Action was already given when created.

**Planner-API shims (`Planner/CkGoap_Planner_Utils.cpp`, added in PR-B.1a)** — currently delegate through `Get_RootAction(Planner)`:
- `Get_PlanStatus`, `Get_Plan`, `Get_PlanClasses`, `Get_PlanCost`, `Get_PlanAttemptCount`, `Get_WorldStateSource`, `Get_InvalidGoal`.
- `Request_Plan`, `Request_CancelPlan`, `Request_SetReplanInterval`, `Request_SetReplanPolicy`, `Request_SetSearchBudget`, `Request_SetCostThreshold`, `Request_SetChildActionCost`.
- `Request_SetGoal` is already half-rewired (writes `_GoalAuthored` on Planner too, but still routes the `SetGoal` request struct to the root Action).

### CkTests HEAD (commit `9c81cc0`)

14 callsites in 6 files still use `utils_goap_action::*` on sub-Actions (promoted dual-role entities) — these were left for B.1b because the verbs needed Planner-API equivalents that work on the entity directly, not via `_RootAction`:

```
Script/CkGoap/CkAutoTest_Goap_Planner_DeferOneFrame.as: 3 callsites (Get_PlanStatus, Get_PlanStatus, Get_Plan)
Script/CkGoap/CkAutoTest_Goap_Planner_IndependentGoalDoesNotEqualEffects.as: 2 (Get_Plan, Get_PlanStatus)
Script/CkGoap/CkAutoTest_Goap_Planner_PromoteActionToPlanner.as: 2 (Get_Plan, Get_PlanStatus)
Script/CkGoap/CkAutoTest_Goap_Planner_WSInheritance.as: 1 (Get_WorldStateSource)
Script/CkGoap/CkAutoTest_Goap_Planner_WSOverride.as: 1 (Get_WorldStateSource)
Script/CkGoap/Gym/CkGoapGym_Patrol_Station.as: 5 (Get_PlanStatus, Get_Plan, Get_PlanStatus, Get_Plan, + 1 more)
```

After B.1b each callsite migrates via `utils_goap_planner::Get_PlanStatus(utils_goap_planner::Cast(subAction))` (the Action handle is castable to Planner because the Action was promoted; cast is a registered ScriptMixin so AS sees it).

### CkGameplayDebugger references

`Source/CkGoapDebugger/Public/CkGoapDebugger/Data/CkGoapDebugger_DataCollector.cpp`:
- L556 reads `Current.Get_RootAction()` (delete with `_RootAction` removal — derive from Planner's RecordOfActions or Plan instead).
- L245 walks `Tree.Get_ChildActions()` — same data is still in `_ChildActions` (we keep `_Tree` on Actions for parent-pointer use), or migrate to RecordOfActions.

`Source/CkGoapDebugger/Public/CkGoapDebugger/Graph/CkGoapDebugGraph.cpp` L54, L97, L535, L539 — `RootActionHandle` field on `FCkGoapDebugger_PlannerInfo`. Delete after `_RootAction` deletion (or repurpose to surface Plan[0] for the UI).

`Source/CkGoapDebugger/Public/CkGoapDebugger/Window/SCkGoapDebugger_InspectorGateway.cpp` L83, L86 — same.

**Note:** debugger hygiene is officially B.4, but `_RootAction` removal in B.1b forces touching the `RootActionHandle` debugger field. Either keep the field as cosmetic (set it to Plan[0]) or delete it. Recommendation: keep as `RootActionHandle = Plan[0]` for visual continuity in the UI; B.4 will properly retire it.

---

## Design decisions for B.1b

### Path: atomic, not subdivided

The prior dispatch's instinct was that B.1b-i (dual-stamp + shim) adds complexity that B.1b-iii then has to unwind. Going **directly atomic** is cleaner:
- Drop `_RootAction` field.
- Change `AddAction` semantics so every Action is a direct child of the Planner (no implicit-root concept).
- Retarget all six A*-pipeline processors to match `FCk_Handle_Goap_Planner`.
- Move planner-role state writes to the Planner entity (not the Action).
- Rewrite Planner-API shims as first-class implementations.

The 19 tests serve as the regression net.

### Fragment relocation

**On Action entity (after B.1b):**
- `FFragment_Goap_Action_Definition` — unchanged.
- `FFragment_Goap_Action_Params` — drop the planner-knob fields (`_SearchBudgetMicroseconds`, `_CostThreshold`, `_ReplanPolicy`, `_MinReplanIntervalSeconds`, `_PlanOnStart`) — those are now Planner-only. Keep `_ActionClass`, `_WorldStateSource_Override`.
- `FFragment_Goap_Action_Tree` — keep, but only `_ParentAction` is meaningful (atomic Actions have an empty `_ChildActions`). Decision: leave `_ChildActions` in place for the dependency-cycle scan in Setup-on-Planner (it walks each child Action's `_ChildActions` to find loops) — alternatively, store children only in the Planner's `RecordOfActions` and drop `_ChildActions` from `_Tree`. **Recommendation for B.1b: keep `_ChildActions` on `_Tree` for now**; B.3 introduces `FFragment_Goap_RecordOfActions` as the discriminator and migrates child enumeration there.
- `FFragment_Goap_Action_Current` — keep `_ActiveParentAction` for breadcrumb.
- `FFragment_Goap_Action_ActionClasses` — delete (truly unused now).

**On Planner entity (after B.1b):**
- All the above Planner-side fragments PLUS:
- `FFragment_Goap_Planner_Requests` (new name; was `FFragment_Goap_Action_Requests`, same shape) — variant queue.
- `FFragment_Goap_Planner_ReplanThrottle` (was `FFragment_Goap_Action_ReplanThrottle`).
- `FFragment_Goap_Planner_PlanContext` (was `FFragment_Goap_Action_PlanContext`).
- `FFragment_Goap_Planner_SearchState` (was `FFragment_Goap_Action_SearchState` — typedef rename).
- `FFragment_Goap_Planner_Result` (was `FFragment_Goap_Action_Result`).
- `FFragment_AStar_Params` and `FFragment_AStar_Debug` — stamped on Planner in B.1b (currently on Action).

**Delete from `FFragment_Goap_Planner_Current`:**
- `_RootAction` field. The remaining members (`_EnableToggle`, `_DependencyCycles`) stay.

**Migration of planner knobs from Action to Planner:**
- `FCk_Fragment_Goap_PlannerParamsData` already has `_PlannerTag`, `_InitialToggle`, `_Goal`, `_WorldStateSource`. ADD: `_SearchBudgetMicroseconds`, `_CostThreshold`, `_ReplanPolicy`, `_MinReplanIntervalSeconds`, `_PlanOnStart`. The `ActionParamsData` keeps `_ActionClass` and `_WorldStateSource_Override` only.
- This is a Blueprint-visible API break — but no backward-compat constraints per the prompt.

### Tags

| Old (Action-scoped) | New (Planner-scoped) | Notes |
|---|---|---|
| `FTag_Goap_Action_RequiresSetup` | unchanged — per-Action CDO extract | Setup-on-Action still fires once per Action |
| `FTag_Goap_Action_RequiresInitialPlan` | `FTag_Goap_Planner_RequiresInitialPlan` | Drives first plan |
| `FTag_Goap_Action_PlanRequested` | `FTag_Goap_Planner_PlanRequested` | If still used (verify) |
| `FTag_Goap_Action_PlanInFlight` | `FTag_Goap_Planner_PlanInFlight` | Parent-plan gating |
| `FTag_Goap_Planner_RequiresSetup` | unchanged | Planner SCC scan |
| `FTag_Goap_Planner_RequiresChainUpdate` | delete (unused) | |
| `FTag_Goap_Dirty_WorldState` / `FTag_Goap_Dirty_Cost` | unchanged | WS processor adds to subscribers (which are now Planners post-B.1b) |

### Processor retargeting

| Processor | Match (before) | Match (after) | Notes |
|---|---|---|---|
| `FProcessor_Goap_Action_Setup` | Action | Action | Stays — extracts each Action's CDO. But reads WS from **parent Planner**'s `_Resolved`, not the Action's own. The Action's `_WorldStateSource_Override` is consulted first; else parent's resolved is used. |
| `FProcessor_Goap_Action_AutoReplan` | Action | DELETE, replaced by `FProcessor_Goap_Planner_AutoReplan` | Per-Planner dirty consumption |
| `FProcessor_Goap_Action_HandleRequests` | Action | DELETE, replaced by `FProcessor_Goap_Planner_HandleRequests` | Per-Planner request drain. Reads child Actions from Planner's `RecordOfGoapActions` |
| `FProcessor_Goap_Action_Execute` | Action | DELETE, replaced by `FProcessor_Goap_Planner_Execute` | TProcessor_AStar_Execute template instantiation moves to Planner |
| `FProcessor_Goap_Action_HandleResult` | Action | DELETE, replaced by `FProcessor_Goap_Planner_HandleResult` | Maps A* path → child Action handles via Planner's catalog |
| `FProcessor_Goap_Action_EndPlay` | Action | DELETE, replaced by `FProcessor_Goap_Planner_EndPlay` | A* search-state cleanup |
| `FProcessor_Goap_Planner_Setup` | Planner | Planner | Direct-child SCC scan — now walks `RecordOfGoapActions` (top-level Planner) OR own Tree (promoted dual-role). Drop the `_RootAction` branch. |
| `FProcessor_Goap_Planner_UpdateActivation` | Action (per U11.2 transitional) | Planner | Per spec §4.2: cast NewStep0 via `UCk_Utils_Goap_Planner_UE::Cast` (Action handle cast to Planner). Dual-role check = "does this Action also have Planner role?". If yes, activate. Recurses naturally because each Planner runs its own UpdateActivation per frame. |

All Planner-side processors include `FFragment_Goap_RecordOfActions` (the Planner's child catalog — already exists as `FFragment_RecordOfGoapActions` in `CkGoap_Action_Record_Internal.h`) in their fragment match list. Discriminator stays `FFragment_Goap_Planner_Params` (B.3 will switch it to `FFragment_Goap_RecordOfActions`).

### Construction flow

**`Add(Owner, PlannerParams)`** — stamps on the Planner entity:
- `_Params`, `_Current` (no `_RootAction` anymore), `_ActionCatalogIndex`, `_Activation` (`_IsActive=true` for top-level).
- `_WorldStateSource` (with `_WorldStateSource` = Params._WorldStateSource; `_Resolved` set immediately = same value for top-level).
- `_PlanState`, `_Goal` (with `_GoalAuthored = Params._Goal`).
- `_Requests`, `_ReplanThrottle`, `_PlanContext`, `_SearchState`, `_Result`.
- `_AStar_Params` (from Params._SearchBudget + Params._CostThreshold), `_AStar_Debug`.
- `FTag_Goap_Planner_RequiresSetup`.
- If `Params._PlanOnStart`: `FTag_Goap_Planner_RequiresInitialPlan`.
- Subscribe Planner to its WS via `Request_AddSubscriber(WS, Planner)`.

**`AddAction(Planner, ActionParams)`** — stamps on a new Action entity:
- `_Params`, `_Definition`, `_Tree` (with `_ParentAction = invalid` — Action is a leaf-from-Planner-perspective; if you want to express "this Action's parent is the Planner", use a new `_ParentPlanner` field or just rely on the lifetime-owner walk).
- `_Current` (just `_ActiveParentAction`).
- `FTag_Goap_Action_RequiresSetup`.
- Adds itself to Planner's `FFragment_RecordOfGoapActions` and `_ActionCatalogIndex`.
- Triggers Planner's `FTag_Goap_Planner_RequiresSetup` (catalog mutation → re-run SCC).
- **NO `_RootAction` indirection. NO implicit-root semantics. Top-level and promoted Planners behave identically.**

**`PromoteActionToPlanner(Action, PlannerParams)`** — stamps the full Planner-role cluster on the existing Action entity:
- All fragments listed in `Add` above.
- `_Activation` starts `_IsActive=false` (waiting for parent to activate).
- WS resolution: if `Params._WorldStateSource` valid, use it. Else `_Resolved` will be set at activation time from parent Planner's resolved WS.
- `FTag_Goap_Planner_RequiresSetup`.
- **Do NOT auto-subscribe to WS** — that happens at activation time (per `DoActivatePlanner`).

### Test migration — the 14 callsites

After B.1b, the sub-Actions referenced in the 14 callsites are dual-role (promoted) entities. They carry both Action role and Planner role. To query plan status / plan / etc., cast to Planner and call planner-utils:

```as
// BEFORE
auto MidStatus = utils_goap_action::Get_PlanStatus(MidHandle);

// AFTER
auto MidAsPlanner = utils_goap_planner::Cast(MidHandle);
auto MidStatus = utils_goap_planner::Get_PlanStatus(MidAsPlanner);
```

For the WSInheritance / WSOverride callsites that query `Get_WorldStateSource` on a sub-Action: the sub-Action is a promoted Planner; cast and use the Planner verb. After B.1b the Planner-side `Get_WorldStateSource` returns the Planner's `_Resolved` WS — same data, different entity reading from.

For the Patrol gym callsites: same pattern. The two sub-Action handles (`_GoToWaypoint_AsAction`, `_Observe_AsAction`) are promoted Planners; cast and use Planner verbs.

### WS subscriber rewire

Today: `DoSubscribeActionToWorldState` registers the **Action** handle as the subscriber. After B.1b: subscribers are **Planner** handles. Touch:
- `Planner/CkGoap_Planner_Utils.cpp` `AddAction` implicit-root branch — DELETE; top-level Planner subscribes itself at `Add` time (in B.1b's new `Add`).
- `Planner/CkGoap_Planner_Processor.cpp` `DoSubscribeActionToWorldState` / `DoUnsubscribeActionFromWorldState` — rename to `DoSubscribePlannerToWorldState` / `DoUnsubscribePlannerFromWorldState`; take `FCk_Handle_Goap_Planner` instead of `FCk_Handle_Goap_Action`.
- `WorldState/CkGoap_WorldState_Processor.cpp` — no changes needed; the subscriber list is typed as generic `FCk_Handle` and the dirty tag is a generic tag.

### Get_ActiveChain rewrite

Currently walks from `_RootAction` and recurses via `_ChildActions` non-empty + `_IsActive`. After B.1b:
- Start at the Planner. Read its `PlanState._Plan[0]` — an Action handle.
- If valid, push it onto the chain.
- Check if the Action is a Planner via `UCk_Utils_Goap_Planner_UE::Has(Action)`. If yes:
  - Read `_Activation._IsActive` on the casted Planner; if not active, terminate.
  - Recurse: read the sub-Planner's `PlanState._Plan[0]`, etc.
- Cap depth defensively (e.g. 64) and use a Seen set to break cycles.

The result remains `TArray<FCk_Handle_Goap_Action>` for backward compatibility with the existing payload type and test assertions.

### Request flow — Request_SetGoal

Currently: writes Planner's `_GoalAuthored`, then enqueues `FCk_Request_Goap_Planner_SetGoal` on the **root Action**'s request queue. The Action's HandleRequests handler then writes the action-side `_Goal` and triggers replan.

After B.1b: writes Planner's `_GoalAuthored`, enqueues the same request on the **Planner**'s request queue. The Planner's `HandleRequests` reads it, writes Planner's `_Goal`, sets `FTag_Goap_Planner_RequiresInitialPlan` on the Planner.

### Active chain change signal

`OnGoap_Planner_ActiveChainChanged` already broadcasts from the top-level Planner handle. The processor signature changes only in the way it discovers the top-level Planner from a sub-Planner (currently walks `Get_LifetimeOwner(Action)`; after B.1b walks `_ParentPlanner` chain or finds via lifetime owner of the Planner).

---

## Suggested commit cadence

1. **CkFoundation commit 1: fragment relocations + tag renames + new struct names.** No processor logic changes yet — compile errors will guide processor rewrite. Add new `FFragment_Goap_Planner_{Requests, ReplanThrottle, PlanContext, SearchState, Result}` and `FFragment_AStar_Params` stamping on Planner. Keep Action-side versions in place for now (don't delete yet — they'd break processor compile).
2. **CkFoundation commit 2: processor rewrites.** Rename Action-side A* pipeline processors to Planner-side. Update fragment match lists. Update read/write targets to use Planner-side fragments. Delete Action-side fragment stamps in `DoCreateOrFindActionEntity`. Delete `_RootAction`.
3. **CkFoundation commit 3: shim rewrites + Get_ActiveChain rewrite + Request_SetGoal rewrite.**
4. **CkGameplayDebugger commit: remove `RootActionHandle` field uses, derive from Plan[0].**
5. **CkTests commit: 14 callsite migrations.**
6. **Root commit: bump submodule pointers.**

Test after each CkFoundation commit. Editor closed before each build. Run with `--test-pattern Goap_Planner` in both Development and DebugGame.

If `__fastfail` / exit `-1073741521` after build: `rm Plugins/*/Binaries/Win64/*.patch_*` and retry.

---

## Build/test commands

```powershell
# Editor lock probe
try { $f = [System.IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Read','None'); $f.Close(); 'editor-not-running' } catch [System.IO.IOException] { 'editor-running' } catch { 'no-log' }

# Development build + test
./CkAuto/UnrealToolbox.exe --build --config=Development --target=Editor --test --test-pattern Goap_Planner --output=Saved/Logs/U11_PRB_1b_dev.log --project="D:\Repos\CkPlugins"

# DebugGame build + test (after Development green)
./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern Goap_Planner --output=Saved/Logs/U11_PRB_1b_debug.log --project="D:\Repos\CkPlugins"
```

Target: 19/19 pass in both configs.

---

## What this does NOT do

(Same as the original prompt.)

- No `utils_goap_action::*` verb deletion (those decommission in B.2).
- No `FFragment_Goap_RecordOfActions` discriminator promotion (B.3).
- No deep debugger hygiene (B.4).
- No CLAUDE.md rewrites (B.5).

---

## Pre-flight checklist for the next dispatch

- [ ] Read this doc end-to-end.
- [ ] Skim `2026-05-22-CkGoap-U11-PR-B-AStar-pipeline-on-Planner.md`.
- [ ] Read spec §2.3, §3, §4 (especially §4.2 pseudocode).
- [ ] Open `Action/CkGoap_Action_Fragment.h`, `Planner/CkGoap_Planner_Fragment.h`, `Action/CkGoap_Action_Processor.cpp` (650 LOC), `Planner/CkGoap_Planner_Utils.cpp` (987 LOC) — these are the four heaviest edit targets.
- [ ] Plan time: budget ~3-4 hours of focused editing + 2-3 build/test cycles.
- [ ] If overrun: BLOCK with state + handoff to a B.1b-recovery prompt.

Begin with the fragment relocations (the smallest unit of work that produces compile errors guiding the rest).
