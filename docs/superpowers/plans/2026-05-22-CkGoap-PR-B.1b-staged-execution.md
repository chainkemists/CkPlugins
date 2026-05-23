# PR-B.1b — Staged execution plan

**Supersedes:** `CONTINUATION_PROMPT_CkGoap_U11_PRB_1b.md` and `CONTINUATION_PROMPT_CkGoap_U11_PRB_1b_commit2_recovery.md`. Those captured useful survey work but bundled too much per dispatch and triggered 4 BLOCKs.

**Status:** EXECUTING. Stage 0 starts after this commit.

## Why this plan exists

PR-B.1b moves the A* pipeline + Planner-role fragments from the implicit-root Action entity onto the Planner entity itself. The work is large (~3000-5000 LOC across CkFoundation + CkGameplayDebugger + CkTests). Four prior subagent dispatches all BLOCKed at survey, recommending subdivision but never landing the subdivision.

This plan stages the work into **six commits** that individually keep `Goap_Planner_*` tests green at every commit boundary. Each commit is a focused dispatch with no design decisions left open — the subagent's job is mechanical execution.

The CTO review's A1 and A4 findings bundle in naturally. Doing them together with the relocation makes downstream consumer migration a single rewrite rather than three.

## Pre-state

- Root commit: `ef09f01` (F.E.A.R. gym committed).
- 24/24 `Goap_Planner_*` AutoTests pass in Dev + DebugGame.
- Path A is currently in effect internally. Path A workarounds in the debugger (`PlannerInfo` canonical-entity redirect, History rail collision-free key, WS rail effective-value read) are live and correct.
- Two prior continuation prompts at `728b170` and `6fcbeb4` document the survey. Worth skimming but the staging below is the authoritative execution.

## Staged commits

### Stage 0 — A1: signal source-type retype

The 4 per-Planner signals (`OnPlanComplete`, `OnPlanFailed`, `OnPlannerActivated`, `OnPlannerDeactivated`) are declared with `FCk_Handle_Goap_Action` source; spec §3.5 mandated `FCk_Handle_Goap_Planner` source. This is independent of fragment relocation.

**Why first:** the signal-type change is mechanical and touches every consumer's delegate signature. Doing it before the fragment-relocation churn means the relocation commit doesn't also have to migrate delegate signatures.

**Scope:**
- Retype the 4 signal declarations in `CkGoap_Action_Fragment.h` (move them out — actually they should live on Planner side; relocate to `CkGoap_Planner_Fragment.h`).
- Move `BindTo_*` / `UnbindFrom_*` UFUNCTIONs from `UCk_Utils_Goap_Action_UE` to `UCk_Utils_Goap_Planner_UE`.
- Update broadcast sites in processors to construct the Planner handle from the Action that's iterating. Today these broadcast `OnPlanComplete(InActionHandle, ...)`; they need to broadcast `OnPlanComplete(InPlannerHandle, ...)`. Under Path A the broadcast site is `FProcessor_Goap_Action_HandleResult` on Action entities; reach the Planner via the Action's owner walk (existing pattern).
- Migrate every `.as` test + gym delegate signature: `void OnFoo(FCk_Handle_Goap_Action& InAction, ...)` → `void OnFoo(FCk_Handle_Goap_Planner& InPlanner, ...)`.

**Verify:** 24/24 in Dev + DebugGame.

### Stage 1 — A4: disable-toggle pipeline gate

Today only `UpdateActivation` gates on `EnableToggle`. The full A* pipeline (Setup, AutoReplan, HandleRequests, Execute, HandleResult) runs unconditionally — disabled Planners can still burn A* CPU.

**Why now:** trivial, independent of relocation, closes a real correctness gap.

**Scope:**
- Add a `Get_OwnerEnableToggle(InActionHandle)` helper that walks `Action._ParentAction` chain up to the implicit-root and reads `Planner._Current.EnableToggle`. Returns `Enable` if walk fails (defensive).
- Add an early-out `if (Get_OwnerEnableToggle(InHandle) != Enable) return;` at the top of each Action-side processor's `ForEachEntity`.

**Verify:** 24/24 + add 1 new AutoTest `Goap_Planner_DisableTogglePreventsReplan` that requests a plan on a disabled Planner and asserts the A* pipeline doesn't run.

### Stage 2 — Dual-stamp Planner-role fragments

Stamp the full Planner-role cluster on the Planner entity in `Add()` and `PromoteActionToPlanner()`. KEEP the existing Action-side stamping in `DoCreateOrFindActionEntity` so the A* pipeline keeps working. After this stage, Planner entities have authoritative-but-unread Planner fragments.

**Scope:**
- In `Add()`: after current stamping, add stamps for `FFragment_AStar_Params`, `_SearchState`, `_Result`, `_PlanContext`, `_Requests`, `_ReplanThrottle` on the Planner entity. PlanState/Goal/WorldStateSource/Activation already get stamped on Planner (per U11.0c work).
- In `PromoteActionToPlanner()`: same full cluster stamping on the host Action entity (which becomes dual-role).
- The fragments on the Planner entity are now POPULATED but no processor reads from them yet.

**Verify:** 24/24. Build green. Memory ticks up slightly (~few hundred bytes per Planner) but no behavior change.

### Stage 3 — Processor rewrite: match Planner, read Planner-side fragments

The atomic flip. Every A*-pipeline processor switches its template `HandleType` from `FCk_Handle_Goap_Action` to `FCk_Handle_Goap_Planner` and reads/writes Planner-side fragments. The Action-side stamps (still present from Stage 2) become unread duplicates.

**Scope:**
- `FProcessor_Goap_Action_Setup` → `FProcessor_Goap_Planner_Setup_Extended` (or merge into existing `FProcessor_Goap_Planner_Setup`). Match Planner. Iterate `Planner.RecordOfActions` for child Actions.
- `FProcessor_Goap_Action_AutoReplan` → `FProcessor_Goap_Planner_AutoReplan`. Match Planner.
- `FProcessor_Goap_Action_HandleRequests` → `FProcessor_Goap_Planner_HandleRequests`. Match Planner. Drain Planner's request queue.
- `FProcessor_Goap_Action_Execute` → `FProcessor_Goap_Planner_Execute` (TProcessor_AStar_Execute instantiated on Planner).
- `FProcessor_Goap_Action_HandleResult` → `FProcessor_Goap_Planner_HandleResult`. Match Planner. Write to Planner's PlanState. Broadcast OnPlanComplete from Planner handle (already retyped in Stage 0).
- `FProcessor_Goap_Action_EndPlay` → `FProcessor_Goap_Planner_EndPlay`.
- `FProcessor_Goap_Planner_UpdateActivation` already matches Action per U11.2 — switch to matching Planner. Implement per spec §4.2 pseudocode.
- Rewire WS subscriber registration in `AddAction` from Action handle to Planner handle.
- Update `Get_ActiveChain` walker to start from Planner directly.
- Update `Request_SetGoal` dispatch to enqueue on Planner's request queue.

**Verify:** 24/24. This is the load-bearing commit; expect to iterate.

### Stage 4 — Drop Action-side Planner-role stamps

Now that processors read from Planner-side fragments, the Action-side stamps in `DoCreateOrFindActionEntity` are dead weight. Remove them.

**Scope:**
- Remove from `DoCreateOrFindActionEntity`: `FFragment_AStar_Params/_Debug`, `_SearchState/_Result/_PlanContext`, `_Requests`, `_ReplanThrottle`, `Planner_PlanState/_Goal/_WorldStateSource/_Activation` (these last four are still legitimate on Action entities IF the entity is dual-role from `PromoteActionToPlanner`; but they're stamped freshly by Promote, so DoCreateOrFindActionEntity doesn't need to stamp them).
- Atomic leaf Actions are now lean (Action-role only: `_Definition`, `_Params`, `_Tree`/`_ParentRef`).

**Verify:** 24/24. If anything reads from a now-missing Action-side fragment, it'll fail; this commit forces all readers to come through the Planner-side path.

### Stage 5 — Delete `_RootAction` and Path A workarounds

Final cleanup. With A* on the Planner entity itself, there's no implicit-root child to designate.

**Scope:**
- Delete `_RootAction` field from `FFragment_Goap_Planner_Current`.
- Remove the canonical-entity redirect in DataCollector (`PlannerInfo` now reads directly from `InPlannerHandle`).
- Delete the 8 type aliases at `CkGoap_Planner_Fragment.h:311-322` (the PR-B.1b breadcrumbs — they were placeholders for fragments that now exist for real on Planner).
- Rename `FFragment_Goap_Action_Requests` → `FFragment_Goap_Planner_Requests` (it lives on Planner entity now), `FFragment_Goap_Action_ReplanThrottle` → `FFragment_Goap_Planner_ReplanThrottle`, etc. Per the alias list — the aliases become canonical names.
- `AddAction` implicit-root branch goes away. Every AddAction creates a direct child; no "first child is special."
- Update tests that referenced `_RootAction` (likely just one or two assertion paths).

**Verify:** 24/24. `git grep '_RootAction'` returns no hits in `Plugins/CkFoundation/Source/CkGoap/`.

### Stage 6 — Docs

- `CkGoap/CLAUDE.md`: update to reflect Planner-runs-A* model. Remove Path A caveats.
- `CkGoapDebugger/CLAUDE.md`: remove canonical-entity-redirect note.
- Spec §2.5 / §4.1 unchanged (they already describe the target architecture). The spec was always describing Path B; the implementation now matches.

## Constraints

- **24/24 tests at every commit boundary.** No commit can leave the tree red.
- **Editor must be closed before each toolbox build.**
- **No backward-compat constraints** — break U11 internals freely.
- **Don't push.** All commits stay local.
- **No new framework features** beyond what each stage's scope dictates.

## Dispatch strategy

Each stage is one subagent dispatch with the stage's exact scope as the prompt. The subagent does NOT survey — survey work is captured in the two prior continuation prompts and this plan. The subagent's job: apply the edits, build, verify tests, commit, bump pointer.

If a stage's subagent BLOCKs, the next dispatch picks up with whatever the BLOCK left committed. Stages build on each other; intermediate states have tests green.

## What's NOT in this plan

- No new tests beyond Stage 1's disable-toggle test. The existing 24 cover the regression net.
- No debugger UI changes beyond Stage 5's redirect removal. The flicker fixes, override stack, etc. all stay.
- No spec edits except removing Path A caveats from the CLAUDE.mds in Stage 6.
- No `FFragment_Goap_RecordOfActions` discriminator rename (CTO #9). That's a separate cleanup if you ever want it — folded into nothing here.
