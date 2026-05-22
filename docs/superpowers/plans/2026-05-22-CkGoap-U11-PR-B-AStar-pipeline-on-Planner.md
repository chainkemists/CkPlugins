# CkGoap U11 PR-B — Move A* pipeline + Planner-role fragments onto the Planner entity

**Status:** PLANNING — about to dispatch B.1.
**Author:** Implementation conversation, 2026-05-22.
**Companion docs:**
- U11 spec — `docs/superpowers/specs/2026-05-21-CkGoap-PlannerActionCollapse-design.md`
- U11.7 mockup — `docs/superpowers/mockups/2026-05-21-CkGoapDebugger-PlannerActionCollapse-mockup-G.html`
- Continuation prompt that scoped U11 — `docs/superpowers/plans/CONTINUATION_PROMPT_CkGoap_U11_PlannerActionCollapse.md`

---

## Why

U11 (phases U11.0 → U11.8) and the post-implementation cleanup (Request_SetGoalWorldState drop, PR-A: SetRootAction + AddAction_ToAction drop, History rail stable identity) brought the user-facing API and the docs into alignment with the U11 design. **But the underlying A* pipeline still anchors to Action entities** — every `FProcessor_Goap_Action_*` matches `FCk_Handle_Goap_Action`, reads/writes `FFragment_Goap_Planner_PlanState`/`_Goal`/`_WorldStateSource`/`_Activation` off the entity that the Planner designates as `_RootAction`. This is the U10 implementation that was preserved because relocating fragments + rewriting processors atomically wasn't possible in a single in-flight phase.

This is the gap PR-B closes. Once landed:
- Atomic leaf Actions carry only Action-role fragments (Definition, Params, ParentRef). No wasted PlanState/AStar memory.
- Planner entities (top-level + promoted) carry the planning state and run A* directly. No more "host child" indirection.
- `_RootAction` field deletes; the U10-vestige is gone.
- `Request_SetGoal` on a promoted Planner works uniformly (no ensure-fail).
- The 4 latent issues PR-A surfaced (`_RootAction` semantic drift, `Request_SetGoal` dispatch via root, `Get_ActiveChain` starts from root, `WorldStateSource` resolution flow) all resolve.
- `FFragment_Goap_RecordOfActions` becomes the Planner-role discriminator per spec §2.1 (folded #9).
- ~10 query/request verbs migrate from `UCk_Utils_Goap_Action_UE` to `UCk_Utils_Goap_Planner_UE`. The type system enforces "only Planners plan."

## What this plan does NOT do

- ❌ No new spec features. PR-B is pure architectural cleanup.
- ❌ No real-cycle-detection redesign (concern #8 — fresh design work, separate effort).
- ❌ No `AddRemoveChildren` test (concern #1).
- ❌ No PIE visual verification (user-driven, post-PR-B).

---

## Pre-state (at start of PR-B)

- Root commit: `3f598a9`.
- Submodules:
  - CkFoundation: `cafbf6b76` (PR-A: SetRootAction + AddAction_ToAction deleted)
  - CkTests: `0e5de6277` (PR-A: tests migrated)
  - CkGameplayDebugger: `304c62cc0` (cleanup #13: History rail stable identity)
- Tests: 19/19 `Goap_Planner_*` AutoTests passing in Development. DebugGame: green per U11.5.

## Constraints

- **19/19 stays green** at every phase boundary. No phase commits red.
- **Editor must be closed before each toolbox build.** Probe before each build:
  ```powershell
  try { $f = [System.IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Read','None'); $f.Close(); 'editor-not-running' } catch [System.IO.IOException] { 'editor-running' } catch { 'no-log' }
  ```
- **No backward-compat constraints** (user has explicitly authorized breaking U10-shaped APIs).
- **Do not push.** All commits stay local.

---

## Phasing (5 phases)

Each phase produces a verified commit set; the next phase doesn't start until tests pass.

### Phase B.1 — Atomic relocation: fragments + processors

**The load-bearing change.** This phase MUST be atomic because the processors and the fragments they match are tightly coupled — change one without the other and matching breaks.

**Changes:**

1. **`Add(Owner, PlannerParams)`** in `CkGoap_Planner_Utils.cpp`: stamp Planner-role fragments on the Owner entity:
   - `FFragment_Goap_Planner_PlanState` (PlanState, PlanCost, PlanStatus, PlanAttemptCount)
   - `FFragment_Goap_Planner_Goal` (GoalAuthored, Goal, InvalidGoalAuthored, InvalidGoal)
   - `FFragment_Goap_Planner_WorldStateSource` (Override + Resolved)
   - `FFragment_Goap_Planner_Activation` (LastActivatedPlan0, IsActive)
   - `FFragment_Goap_Planner_Replan` (policy, throttle, interval) — currently on Action
   - `FFragment_Goap_Planner_Requests` (rename from `FFragment_Goap_Action_Requests` — see #9 below)
   - `FFragment_Goap_Planner_SearchState` / `_Result` / `_PlanContext`
   - `FFragment_AStar_Params`
   - All existing Planner-side fragments (`_Params`, `_Current`, `_ActionCatalogIndex`, etc.)

2. **`AddAction(Planner, ActionParams)`**: stamp ONLY Action-role fragments on the new entity:
   - `FFragment_Goap_Action_Definition` (Preconditions, Effects, Cost)
   - `FFragment_Goap_Action_Params` (ActionClass, WS override — but NOT planning policy fields)
   - `FFragment_Goap_Action_Tree` or new `_ParentRef` (parent Planner handle + sibling list)
   - **NO Planner-role fragments.** Leaf Actions are lean.
   - Register child in the parent Planner's RecordOfActions discriminator (see #9 below).

3. **`PromoteActionToPlanner(Action, PlannerParams)`**: stamp the Planner-role fragments listed in (1) onto the existing Action entity. The entity becomes dual-role. Add `FTag_Goap_Planner_RequiresSetup`.

4. **Rewrite every `FProcessor_Goap_Action_*` to match `FCk_Handle_Goap_Planner`:**
   - `FProcessor_Goap_Action_Setup` → `FProcessor_Goap_Planner_Setup`. Matches Planner. Iterates the Planner's RecordOfActions to extract CDOs, populate operator graph, run Tarjan SCC, validate goal against WS key registry.
   - `FProcessor_Goap_Action_AutoReplan` → `FProcessor_Goap_Planner_AutoReplan`. Per-Planner dirty-tag consumption.
   - `FProcessor_Goap_Action_HandleRequests` → `FProcessor_Goap_Planner_HandleRequests`. Drain per-Planner request queue. Parent-plan gating (U9 mechanism, `FTag_Goap_Planner_PlanInFlight`) checks parent Planner's status.
   - `TProcessor_AStar_Execute<...>` template instantiation switches to Planner handle.
   - `FProcessor_Goap_Action_HandleResult` → `FProcessor_Goap_Planner_HandleResult`. Write Plan to Planner's `_PlanState`. Broadcast `OnPlanComplete`/`OnPlanFailed` from Planner handle.
   - `FProcessor_Goap_Planner_UpdateActivation` already matches Planner per U11.2's design — but today its TProcessor template uses `FCk_Handle_Goap_Action` (per U11.2 report). Switch to `FCk_Handle_Goap_Planner` and update the Plan[0] walk to alternate `Planner → Plan[0]Action → Action.HasPlannerRole? → recurse`.

5. **Delete `_RootAction` field from `FFragment_Goap_Planner_Current`**. With the A* pipeline on the Planner itself, there's no implicit-root child to designate. Anywhere that read `_RootAction` (Get_ActiveChain walks, Request dispatch indirection, debugger DataCollector queries) needs migration — see #5 below.

6. **Rewire WS subscribers**: today, when a child Action's resolved WS changes, the dirty hooks fire on the Action. After relocation, they fire on the Planner. Find the WS subscriber-registration code (likely in `CkGoap_WorldState_Utils` or in the Planner Setup processor) and rewire.

7. **Migrate `Get_ActiveChain`**: today it starts from `_RootAction`. After relocation, starts from the Planner itself. Walks Plan[0] (which is an Action handle from the Planner's PlanState); if that Action is dual-role (carries Planner fragments), recurse into its Plan[0]; etc. Build the ordered chain.

8. **Migrate `Request_SetGoal`** (and similar) dispatch: today they enqueue on `_RootAction`'s request queue. After relocation, enqueue on the Planner's request queue directly. The `RootAction.IsValid()` ensure goes away.

**Test patching:**
- Every test that calls `utils_goap_action::Get_PlanStatus(rootAction)` etc. needs to switch to `utils_goap_planner::Get_PlanStatus(planner)`. Most tests already save the Planner handle from `Add`; verify and migrate. **This step bleeds into B.2 — pragmatically do it inline with B.1 to keep tests green.**

**Verification:**
- Build green.
- `--test-pattern Goap_Planner --config=Development` → 19/19 pass.
- `--test-pattern Goap_Planner --config=DebugGame` → 19/19 pass.
- Log: `Saved/Logs/U11_PRB_1.log`.

**Honest scope estimate:** ~3000-4000 LOC across CkFoundation. Plus test migration for AS files that touched moved verbs. Single-session attempt; may need to split if the test migration alone is a heavy lift.

**Subdivision fallback:** If B.1 looks like it'll overrun, split into B.1a (stamp fragments on Planner, leave Action's fragments duplicated with shimmed reads — keep both paths working) and B.1b (drop Action's planner-role fragments + cut over to Planner-only paths). The shimming is non-trivial but each sub-phase is safer.

---

### Phase B.2 — Migrate query/request verbs from utils_goap_action to utils_goap_planner

After B.1, the verbs already work on Planners. B.2 removes them from `UCk_Utils_Goap_Action_UE` entirely. Tests + gyms migrate to the Planner-API forms.

**Verbs to migrate** (decommission from Action utils, keep only on Planner utils):
- `Get_PlanStatus`
- `Get_Plan`
- `Get_PlanCost`
- `Get_PlanAttemptCount`
- `Get_WorldStateSource`
- `Get_InvalidGoal`
- `Request_Plan`
- `Request_CancelPlan`
- `Request_SetReplanInterval`
- `Request_SetReplanPolicy`
- `Request_SetSearchBudget`
- `Request_SetCostThreshold`

**Verbs that legitimately stay on Action utils:**
- `Has` (Action role discriminator check)
- `Get_ActiveParentAction` (debugger breadcrumb — derives from parent walk)
- Possibly `Get_Preconditions`/`Get_Effects`/`Get_Cost` (the Action-role contract for downstream code that reads operator metadata).

**Test/gym patching:** mechanical find/replace driven by AS compile errors.

**Verification:** 19/19 pass in both configs.

**Estimated:** ~half-day. Mostly mechanical.

---

### Phase B.3 — Introduce FFragment_Goap_RecordOfActions as the discriminator (fold #9)

Per spec §2.1, the Planner-role discriminator should be a `FFragment_Goap_RecordOfActions` that holds the planner's direct child Action handles. Currently the discriminator is `FFragment_Goap_Planner_Params + _Current` (renamed from U10's ActionSet fragments).

**Changes:**
1. New fragment: `FFragment_Goap_RecordOfActions { TArray<FCk_Handle_Goap_Action> _Children; }` in `CkGoap_Planner_Fragment.h`.
2. Promote it to be THE discriminator for the Planner role:
   - `UCk_Utils_Goap_Planner_UE::Has(handle)` checks for this fragment.
   - `UCk_Utils_Goap_Planner_UE::Cast(handle)` succeeds iff this fragment is present.
   - Every processor's TProcessor template param list includes this fragment.
3. Retire `FFragment_Goap_Planner_Params` and `FFragment_Goap_Planner_Current` OR repurpose them for purely non-discriminating state (enable-toggle, dependency-cycles, planner-tag — these are still real state, just not the discriminator).
4. Migrate `RecordOfActions`'s `_Children` to be the canonical "this Planner's direct child Actions" list. Update all read sites: AddAction appends; Setup iterates for graph build; UpdateActivation walks Plan[0]; Get_ActiveChain walks; etc.

**Verification:** 19/19 pass in both configs.

**Estimated:** ~half-day.

---

### Phase B.4 — Debugger hygiene (concerns #3, #4, #5)

After B.1-B.3, the debugger's DataCollector can drop the legacy `FCkGoapDebugger_ActionSetInfo` shim. The new types (`PlannerInfo`, `ActionInfo`, `EntitySnapshot.TopLevelPlanners[]`) become self-sufficient.

**Changes:**
1. Migrate `UCkGoapDebugGraph::RebuildFromSnapshot` and `SCkGoapDebugger_GraphPane::RefreshFromViewModel` to consume `EntitySnapshot.TopLevelPlanners[]` directly. Drop dependence on `ActionSets[]`.
2. Recurse into `ChildPlanners` and `ChildActions` properly in `DataCollector::BuildPlannerInfo` so the tree is fully populated (today it's flat per U11.7-A report).
3. Migrate `SCkGoapDebugger_WorldStateRail` to use Planner selection instead of `_SelectedAction`. Then retire `_SelectedAction` on `CkGoapDebugger_ViewModel`.
4. Delete `FCkGoapDebugger_ActionSetInfo`, `EntitySnapshot.ActionSets`, the synthesis logic in DataCollector.
5. Verify the existing UI (sidebar tree, primary pane, plan strip, breadcrumb, graph) all still render correctly.

**Verification:**
- Build green.
- 19/19 tests pass.
- (PIE visual verification deferred to user post-PR-B.)

**Estimated:** ~half-day.

---

### Phase B.5 — Docs rewrite

Rewrite the two CLAUDE.mds to reflect the post-PR-B architecture:
1. `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` — emphasize that A* runs on Planners, atomic Actions are lean, fragments are role-segregated, `RecordOfActions` is the discriminator.
2. `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/CLAUDE.md` — emphasize the per-Planner data shape with no legacy shim.

**Verification:** docs only. No build needed.

**Estimated:** ~1-2 hours.

---

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| B.1 cannot land atomically in one dispatch | Subdivide into B.1a (dual-stamp, shimmed reads) and B.1b (cut over). Verify between. |
| Processor rewrites break test invariants subtly (e.g., signal broadcast source changes from Action to Planner, tests subscribing to wrong handle) | Migrate signal subscriptions in tests during B.1's own dispatch; don't defer. |
| Stale `.patch_*` DLLs in submodule Binaries cause `__fastfail` on cross-DLL changes | If `__fastfail` / exit `-1073741521`: delete `Plugins/*/Binaries/Win64/*.patch_*` and retry. |
| Editor lock contention from other Claude sessions or manual editor use | Probe before every build; wait 60s and retry if `editor-running`. |
| DebugGame test discovery regresses | Verify both configs pass at every phase. If `__fastfail`, apply the D7 patch_* cleanup. |
| Subagent BLOCKS partway through B.1 | The plan IS the recovery — read this doc, see what's left, dispatch a continuation. |

---

## What "done" looks like

- Root commit lands all 5 phases on `dev`. Not pushed.
- `git grep '_RootAction'` returns no hits in `Plugins/CkFoundation/Source/CkGoap/`.
- `git grep 'utils_goap_action::Get_PlanStatus\|utils_goap_action::Request_Plan'` returns no hits in `Plugins/CkTests/`.
- 19/19 tests pass in Development AND DebugGame.
- 5 phases × ≥1 commit each in CkFoundation; CkTests has migration commits; CkGameplayDebugger has B.4 cleanup.
- Both CLAUDE.mds updated.
- User PIE-verifies all gyms.

---

## Suggested first move

Dispatch B.1 with the spec sections (§2.3 fragment table, §3 API, §4 processor flow) pre-loaded into the prompt + this plan doc as the migration order. Allow the subagent to subdivide into B.1a / B.1b if scope demands. Use the Opus model — this is the heaviest dispatch in the entire U11 effort.
