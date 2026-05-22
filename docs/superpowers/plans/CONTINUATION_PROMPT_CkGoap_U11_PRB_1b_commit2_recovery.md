# CkGoap U11 PR-B.1b — Commit 2 (A+B) recovery handoff

**Status:** Prior dispatch attempted commit 2 (the load-bearing A+B atomic fragment-relocation + processor retarget) and **BLOCKED at survey + design**. Root state unchanged at `a5cd590` with submodule pointers:

- CkFoundation: `3af18b53a` (PR-B.1b prep — Planner-side fragment + tag aliases, commit 1 of the cadence)
- CkTests: `9c81cc0`
- CkGameplayDebugger: `304c62cc0`
- 19/19 `Goap_Planner_*` AutoTests pass in Development and DebugGame.

**Why blocked**: the commit-2 surface is larger than the prior continuation-prompt's "A+B atomic" framing implied. Honest accounting:

1. **A+B as defined cannot keep 19/19 green without also rewriting shims** (commit 3 work). Reason: today's shims (`Get_PlanStatus`, `Get_Plan`, `Get_WorldStateSource`, `Get_InvalidGoal`, all `Request_*` verbs) read/write through `_RootAction.{PlanState, Goal, WorldStateSource, Requests}`. If the A* pipeline + planner-role cluster relocates onto the Planner entity, those fragments on the implicit-root Action go stale — no processor writes them — and the shims read stale data. Tests fail.

   The dispatch contemplates "C can be deferred" but does not address the read-API rewrite that must accompany the data relocation. In practice **A + B + shim-rewrite must land together**.

2. **The "Planner runs A* over its child Actions" requires a children-source on the Planner**. The Action-side `HandleRequests` reads `InHandle.Get<FFragment_Goap_Action_Tree>().Get_ChildActions()`. After retarget to Planner-matched, the Planner must supply candidates either by:
   - Stamping `FFragment_Goap_Action_Tree` on the Planner (small change but reuses an Action-role fragment on a Planner role — semantically awkward),
   - Reading `_RootAction.Tree.ChildActions` (works for top-level Planner, but promoted mid-tier has its own Tree from its Action role — branching logic on `_RootAction` validity),
   - Reading from `FFragment_RecordOfGoapActions` on the Planner (clean but every Action in the catalog is candidate — including the implicit-root itself, which today is its own root not a candidate).

   The cleanest reading aligned with the spec is option C plus dropping the implicit-root concept entirely (every AddAction becomes a direct child of the Planner — no `_RootAction`, no two-tier construction). This is step C in the dispatch but tightly coupled to A+B.

3. **`PromoteActionToPlanner` reuses the dual-role cluster the Action already carries**. After A+B that dual-role default goes away — Actions no longer auto-stamp the planner-role cluster. So `PromoteActionToPlanner` becomes the verb that stamps the full A* pipeline (and the planner-role cluster `PlanState/Goal/WSSource/Activation`) onto the Action being promoted. Today it only stamps the discriminator (Params/Current/ActionCatalogIndex). This is mentioned in the original prompt but its full shape needs spelling out.

## What I learned by reading the code

The transitional model in commit-1 state is:

- Every Action entity carries the full A* pipeline + planner-role cluster (dual-role default, set in `DoCreateOrFindActionEntity`).
- The Planner entity carries only the planner-role discriminator (Params, Current, ActionCatalogIndex) + a partial planner-role cluster (WorldStateSource, PlanState, Goal, Activation set in `Add`). It does NOT carry the A* pipeline.
- `_RootAction` on the Planner points at the implicit-root Action — the Action entity that actually runs A* on behalf of the top-level Planner.
- The 5 Action-pipeline processors (AutoReplan, HandleRequests, Execute, HandleResult, EndPlay) match `FCk_Handle_Goap_Action` and run on every Action entity. Atomic Action entities (no children) also match these processors but their candidate set is empty, so the planner short-circuits to `PlanFound` with an empty plan. (`FProcessor_Goap_Action_Setup` builds `_CachedActionDef` and that's also fine for atomic Actions — they just don't have anything to plan over.)
- `FProcessor_Goap_Planner_UpdateActivation` matches `FCk_Handle_Goap_Action` (not Planner) — per the U11.2 transitional comment in its header. Walks Plan[0] and activates/deactivates composite sub-Planners (= Actions with children).

## Recommended cadence for the next dispatch — landed in one commit

Rather than my prior dispatch's split, treat **commit 2 as an atomic refactor that lands all of**:

1. **Fragment relocations** — Planner gets the A* pipeline (`AStar_Params`, `AStar_Debug`, `SearchState`, `Result`, `PlanContext`, `ReplanThrottle`, `Requests`). Action stops carrying these (and stops carrying the planner-role cluster `PlanState/Goal/WSSource/Activation`).
2. **Drop the implicit-root concept** — `AddAction` adds Action directly as a child of the Planner. Stamp `FFragment_Goap_Action_Tree` on the Planner (it becomes the candidate-operator set) OR use `FFragment_RecordOfGoapActions` for candidate enumeration. Recommendation: stamp Tree on Planner. It mirrors the promoted-Planner case and keeps the cycle scan in `FProcessor_Goap_Planner_Setup` working unchanged.
3. **Delete `_RootAction` from `FFragment_Goap_Planner_Current`**.
4. **Rename + retarget the 5 A* pipeline processors** to match `FCk_Handle_Goap_Planner` (drop Action-side processor registrations; new `FProcessor_Goap_Planner_{AutoReplan, HandleRequests, Execute, HandleResult, EndPlay}` matching Planner).
5. **`FProcessor_Goap_Planner_UpdateActivation` matches Planner**, walks Plan[0]; check composite via `UCk_Utils_Goap_Planner_UE::Has(NewStep0)` (the Action handle is a promoted Planner if `Has` returns true) rather than via `Tree._ChildActions.Empty`.
6. **Rewrite Planner-API shims** to read/write Planner-side fragments directly (no `_RootAction` indirection). `Get_PlanStatus(Planner)` → `Planner.Get<FFragment_Goap_Planner_PlanState>().Get_PlanStatus()`. `Request_Plan(Planner)` → enqueue on Planner's Requests queue. Etc.
7. **`Request_SetGoal(Planner, Goal)`** → enqueue on Planner's request queue (the request struct is `FCk_Request_Goap_Planner_SetGoal`, already exists).
8. **WS subscriber rewire** — `Add(Planner)` subscribes the Planner to its WS (`UCk_Utils_Goap_WorldState_UE::Request_AddSubscriber(WS, Planner)`). `PromoteActionToPlanner` ALSO needs WS subscription path — subscribe at activation time via `UpdateActivation::DoActivatePlanner` (which today subscribes an Action; switch to Planner).
9. **`Get_ActiveChain(Planner)`** rewrite — start from `Planner.PlanState._Plan[0]`, walk via `UCk_Utils_Goap_Planner_UE::Has(Plan0)` for composite check.
10. **Update Action-side `Action_Utils` `Get_PlanStatus/Get_Plan/Get_PlanCost/Get_WorldStateSource/Get_InvalidGoal`** — these read planner-role fragments off an Action handle. After commit 2 those fragments only exist on promoted (dual-role) Action+Planner entities. Either:
    - Keep them functional ONLY when the Action is also a Planner (cast to Planner and read).
    - Or guard with `UCk_Utils_Goap_Planner_UE::Has(InAction)` + ensure; emit warning if called on a bare Action.

    The 14 remaining CkTests AS-side callsites (commit 5 work) call these on promoted sub-Actions, which IS a Planner — they keep working as long as Planner-cast + Planner read.

## Concrete file edit map (next dispatch should follow this exactly)

### `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h`

- `FFragment_Goap_Action_Requests`, `FFragment_Goap_Action_ReplanThrottle`, `FFragment_Goap_Action_PlanContext` — these become the *canonical* names for Planner-role fragments (the existing aliases in `Planner_Fragment.h` already point at them). Update friend declarations to reference `FProcessor_Goap_Planner_*` names.
- `using FFragment_Goap_Action_SearchState = TFragment_AStar_SearchState<...>` — keep, the alias just maps to the AStar template.
- `using FFragment_Goap_Action_Result = TFragment_AStar_Result<int32>` — keep.

### `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Fragment.h`

- Delete `_RootAction` member field + `CK_PROPERTY_GET(_RootAction)` from `FFragment_Goap_Planner_Current`.
- Switch alias declarations (lines 303-314) into first-class type renames OR keep aliases pointing at the existing fragment types (the latter is less churn).
- Add friend declarations for the new Planner-side processors on `FFragment_Goap_Planner_*` structs as needed (PlanState, Goal, WorldStateSource).

### `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.h`

Replace each Action-pipeline processor class with a Planner-matched counterpart. Either:
- Rename to `FProcessor_Goap_Planner_*` and move to `Planner/CkGoap_Planner_Processor.{h,cpp}`, OR
- Keep them in `Action/CkGoap_Action_Processor.{h,cpp}` (file name is no longer accurate but minimizes file churn) and rename inside.

Concretely the 5 processors that change match type:

```cpp
// New: FProcessor_Goap_Planner_AutoReplan : ck_exp::TProcessor<
//      ..., FCk_Handle_Goap_Planner,
//      ck::TReadOnly<FFragment_Goap_Planner_Params>,        // discriminator
//      ck::TReadOnly<FCk_Fragment_Goap_ActionParamsData>?   // NO — knobs migrate to PlannerParams; see below
//      ck::TReadWrite<FFragment_Goap_Action_ReplanThrottle>,
//      ...>
```

**Decision required**: planner knobs (`_SearchBudgetMicroseconds`, `_CostThreshold`, `_ReplanPolicy`, `_MinReplanIntervalSeconds`, `_PlanOnStart`) currently live on `FCk_Fragment_Goap_ActionParamsData`. Per the original B.1b plan they migrate to `FCk_Fragment_Goap_PlannerParamsData`. **Recommended for commit 2: migrate them now**. The fields move from `Action_Fragment_Data.h` to `Planner_Fragment_Data.h`. `ActionParamsData` keeps only `_ActionClass` and `_WorldStateSource_Override`. This is a Blueprint-visible API break but no backward-compat is required per project policy.

Keep `FProcessor_Goap_Action_Setup` matching `FCk_Handle_Goap_Action` — per-Action CDO extract still happens per Action (its preconditions/effects/cost/_CachedActionDef are Action-role data). Setup reads `FFragment_Goap_Planner_WorldStateSource` from the **parent Planner** to register WS keys + resolve effects. To do this either: walk lifetime-owner (works for top-level Planner case), or store a `_ParentPlanner` field on the Action (cleaner long-term).

`FProcessor_Goap_Action_HandleResult` ALSO stays Action-matched in the prior plan, but actually no — HandleResult writes `FFragment_Goap_Planner_PlanState`. Post-relocation that lives on the Planner. So HandleResult → `FProcessor_Goap_Planner_HandleResult`.

Final processor list after commit 2:

| Processor | Match | Source file |
|---|---|---|
| `FProcessor_Goap_Action_Setup` | Action | Action_Processor.cpp |
| `FProcessor_Goap_Planner_Setup` | Planner | Planner_Processor.cpp |
| `FProcessor_Goap_Planner_AutoReplan` | Planner | Planner_Processor.cpp (NEW) |
| `FProcessor_Goap_Planner_HandleRequests` | Planner | Planner_Processor.cpp (NEW) |
| `FProcessor_Goap_Planner_Execute` | Planner | Planner_Processor.cpp (NEW, AStar template instantiation) |
| `FProcessor_Goap_Planner_HandleResult` | Planner | Planner_Processor.cpp (NEW) |
| `FProcessor_Goap_Planner_EndPlay` | Planner | Planner_Processor.cpp (NEW, AStar template instantiation) |
| `FProcessor_Goap_Planner_UpdateActivation` | Planner | Planner_Processor.cpp (RETARGETED) |

All `CK_REGISTER_PROCESSOR` lines must update. Forgetting any one is a silent no-op.

### `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.cpp`

- `Add(Owner, PlannerParams)`:
  - Stamp Planner-side: `FFragment_Goap_Planner_Params`, `FFragment_Goap_Planner_Current` (no `_RootAction`), `FFragment_Goap_Planner_ActionCatalogIndex`, `FFragment_Goap_Planner_WorldStateSource` (with `_Resolved = Params._WorldStateSource`), `FFragment_Goap_Planner_PlanState`, `FFragment_Goap_Planner_Goal` (with `_GoalAuthored = Params._Goal`), `FFragment_Goap_Planner_Activation` (`_IsActive=true`).
  - **NEW**: A* pipeline cluster — `FFragment_AStar_Params` (from `Params._SearchBudgetMicroseconds + Params._CostThreshold`), `FFragment_AStar_Debug`, `FFragment_Goap_Action_Requests` (the variant queue), `FFragment_Goap_Action_ReplanThrottle`, `FFragment_Goap_Action_PlanContext`, `FFragment_Goap_Action_SearchState`, `FFragment_Goap_Action_Result`.
  - **NEW**: `FFragment_Goap_Action_Tree` (empty `_ChildActions`) — Planner becomes its own candidate-operator container. `_ParentAction` invalid.
  - `FTag_Goap_Planner_RequiresSetup`.
  - If `Params._PlanOnStart` → `FTag_Goap_Action_RequiresInitialPlan` (alias for `FTag_Goap_Planner_RequiresInitialPlan`).
  - Subscribe Planner to its WS via `Request_AddSubscriber(WS, Planner)` (was previously called on the implicit-root Action inside AddAction).

- `AddAction(Planner, ActionParams)`:
  - Drop implicit-root branching. Every AddAction adds the new Action as a direct tree child of the Planner.
  - The new Action gets the bare Action-role fragments: `FFragment_Goap_Action_Params`, `FFragment_Goap_Action_Current`, `FFragment_Goap_Action_ActionClasses` (legacy), `FFragment_Goap_Action_Definition`, `FFragment_Goap_Action_Tree`. **NOT** the A* pipeline or planner-role cluster.
  - Stamp `FFragment_Goap_Planner_WorldStateSource` on the Action — needed by Action_Setup to register WS keys + resolve _CachedActionDef. Resolve from override OR parent Planner's resolved WS at AddAction time.
  - Wire as child: `ActionEntity.Tree._ParentAction = invalid (it's a child of the Planner, not an Action); Planner.Tree._ChildActions.AddUnique(ActionEntity)`.
  - **Promoted mid-tier case**: same code path — Planner is the promoted Action+Planner entity; its own Tree.ChildActions becomes the candidate set.

- `PromoteActionToPlanner(Action, PlannerParams)`:
  - Stamp full planner-role cluster + A* pipeline (same set as `Add`).
  - The Action already has its own `FFragment_Goap_Action_Tree` — that becomes the candidate-operator set for the promoted Planner. No duplicate Tree fragment needed.
  - Subscribe — at activation time, via `UpdateActivation::DoActivatePlanner` (which today subscribes Actions; switch to Planners).

- `Get_RootAction`: delete entirely. Replace any caller (debugger) with derivation from `Planner.PlanState._Plan[0]` or null.

- All `Get_*` / `Request_*` shims: read/write Planner-side fragments directly. Delete the `Get_RootAction(Planner)` + delegate-to-Action pattern.

- `Get_ActiveChain`: start from `Planner.PlanState._Plan[0]`. Each step is an Action handle; check `UCk_Utils_Goap_Planner_UE::Has(Step)` for composite; if yes recurse into `Step.PlanState._Plan[0]`.

### `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Utils.cpp`

- `Get_PlanStatus`, `Get_Plan`, `Get_PlanCost`, `Get_WorldStateSource`, `Get_InvalidGoal`: today read `FFragment_Goap_Planner_*` off the Action. After commit 2 these fragments only exist on promoted Actions. Either:
  - Guard: `if (NOT UCk_Utils_Goap_Planner_UE::Has(InAction)) { warn + return default; }`. Then cast and read.
  - Or delete these from Action_Utils entirely — the Action-side verb no longer makes sense post-relocation. (B.2 in the broader PR cadence retires `utils_goap_action` verbs.)
  - **Pragmatic**: keep them but route through Planner-cast for now. The 14 CkTests callsites still call these on promoted Actions; they keep working. After B.2 the verbs go away.

### `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Processor.cpp`

- `FProcessor_Goap_Planner_Setup`: simplify — direct children are always `InHandle.Get<FFragment_Goap_Action_Tree>().Get_ChildActions()` (no `_RootAction` branch). Defer if any child has `FTag_Goap_Action_RequiresSetup`.
- `FProcessor_Goap_Planner_UpdateActivation`: match Planner. `ForEachEntity` template params change. Activation logic: composite check via `UCk_Utils_Goap_Planner_UE::Has(NewStep0)`. `DoActivatePlanner` now takes Planner handles (or stays Action-typed since Plan[0] is always an Action handle and activation flips it into Planner-role behavior — cleanup at activation time).
- `DoSubscribeActionToWorldState` → `DoSubscribePlannerToWorldState(FCk_Handle_Goap_Planner&)`.

### `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.cpp`

Most logic moves to Planner_Processor.cpp. What remains:

- `FProcessor_Goap_Action_Setup` — unchanged behavior (CDO extract + WS key register). Reads parent Planner's `_Resolved` (via lifetime owner walk or new `_ParentPlanner` field).

### `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/Public/CkGoapDebugger/Data/CkGoapDebugger_DataCollector.cpp`

- L556 `Info.RootActionHandle = Current.Get_RootAction()` — delete or switch to `Info.RootActionHandle = Plan.Num() > 0 ? Plan[0] : FCk_Handle_Goap_Action{}`. Cosmetic for UI continuity; B.4 will properly retire the field.

### `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/Public/CkGoapDebugger/Data/CkGoapDebugger_Types.h`

- L232 comment references `_RootAction` — update or leave as historical breadcrumb.

## What to test after the atomic commit

```powershell
# Editor lock probe
try { $f = [System.IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Read','None'); $f.Close(); 'editor-not-running' } catch [System.IO.IOException] { 'editor-running' } catch { 'no-log' }

# Development build + test
./CkAuto/UnrealToolbox.exe --build --config=Development --target=Editor --test --test-pattern Goap_Planner --output=Saved/Logs/U11_PRB_1b_c2_dev.log --project="D:\Repos\CkPlugins"

# DebugGame after Dev green
./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern Goap_Planner --output=Saved/Logs/U11_PRB_1b_c2_dbg.log --project="D:\Repos\CkPlugins"
```

Target: 19/19 pass in both configs.

If `__fastfail` / exit `-1073741521`: `rm Plugins/*/Binaries/Win64/*.patch_*` and retry.

## Gotchas surfaced during analysis

- **`AddAction` on a promoted Planner today reads `InPlanner.Has<FFragment_Goap_Action_Tree>()` to discriminate**. After commit 2, **every** Planner has a Tree fragment (it's how candidates are tracked). The branch goes away — both top-level and promoted Planners get a uniform "AddAction adds to my Tree" path.

- **`FProcessor_Goap_Action_HandleRequests` (now `FProcessor_Goap_Planner_HandleRequests`) gates Plan requests on parent-plan-in-flight**. The "parent" relationship today is via `Tree._ParentAction` (Action-to-Action). After commit 2 a promoted Planner's parent is its Action's `_ParentAction` (still valid — promoted entities are dual-role). Top-level Planners have no parent — `_ParentAction` invalid. Gating logic continues to work.

- **`Add` warns if a top-level Planner has no WS source** (currently the warning lives inside `AddAction`'s implicit-root branch). Move the warning to `Add` (the natural moment of decision).

- **`PromoteActionToPlanner` currently no-ops if the entity already has Planner role** (warns and returns existing cast). Keep this behavior — promotes the same Action twice should be idempotent.

- **`Request_SetGoal` rewrite** — enqueue `FCk_Request_Goap_Planner_SetGoal` on the Planner's request queue. `HandleRequests` then writes Planner's Goal and re-arms `FTag_Goap_Action_RequiresInitialPlan` on the Planner.

- **No anonymous-namespace collision risk** if you're careful — the existing `ResolveCondition`/`ResolveEffect`/`BuildConstraintSet` helpers in Action_Processor.cpp anonymous namespace get reused by the new Planner_Processor.cpp counterparts. **Move them or rename them** to avoid duplicate symbol when unity build collapses things.

## Effort estimate

- **Real time**: 4-6 hours focused editing for a single dispatch. Lots of mechanical rename + small semantic updates. The risk is in the WS subscriber + activation flow + shim rewrites — those have subtle ordering implications.
- **Compile cycles**: budget 3-5 build-fix passes. UnrealToolbox build takes 60-120 seconds.
- **Test debugging**: a few tests will likely break first pass. Common culprits: missing fragment stamp, missing processor registration, stale `_RootAction` reference, wrong handle type at a cast site.

## Definitely-NOT in commit 2

- AS test migrations (commit 5 in original cadence; the 14 CkTests AS callsites).
- Deeper debugger reshape (commit 4).
- `utils_goap_action` verb decommission (B.2).
- `FFragment_Goap_RecordOfActions` discriminator promotion (B.3).
- CLAUDE.md rewrite (B.5).

## Subdivision options if commit 2 is still too large

If even this atomic shape is too large mid-dispatch, the only meaningful subdivision is:

- **Commit 2a**: dual-stamp the planner-role cluster + A* pipeline on the Planner (in addition to Action). No processor retarget. No `_RootAction` deletion. Compiles cleanly. Tests still pass (Action-side processors still run; Planner-side data is just inert). Zero behavior change.
- **Commit 2b**: retarget processors to Planner. Remove Action-side stamps. Rewrite shims. Delete `_RootAction`. Now Planner-side is live and Action-side is gone.

The dual-stamp intermediate is the "safety harness" pattern — same data lives on both entities until the next commit removes one side.

## Begin the next dispatch with

1. Read this doc end-to-end.
2. Skim `2026-05-22-CkGoap-U11-PR-B-AStar-pipeline-on-Planner.md` (the PR-B plan).
3. Skim `2026-05-21-CkGoap-PlannerActionCollapse-design.md` §2.3 + §4.2.
4. Confirm editor closed via the lock probe.
5. Plan to commit 1 CkFoundation atomic + 1 CkGameplayDebugger small + 1 root pointer-bump.
6. Do NOT push.
