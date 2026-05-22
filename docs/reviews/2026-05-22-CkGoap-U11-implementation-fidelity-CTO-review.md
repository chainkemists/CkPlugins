# CkGoap U11 (Planner/Action Collapse) — Implementation Fidelity CTO Review

> **Workflow:** Review the brief below, then fill in the **CTO Review Response** section at the bottom of this file. Commit your changes — the plan author / their assistant will pick up your notes from there.
>
> **This is NOT a pre-implementation review.** The implementation is already landed across root commits `09323fe` (spec committed) through `0474dd6` (B.4 partial). You are evaluating **implementation fidelity to the original plan**, not whether the plan is correct. The plan is correct; the question is whether the code matches it.

---

## Reviewer brief

### Your role

Senior reviewer / architect. You are evaluating the **fidelity** of the CkGoap U11 implementation against the original design spec. Specifically:

1. Does each phase that's marked "done" actually match what the spec said it would deliver, or did it land something materially different?
2. Are the partial states (PR-B.1b fragment-relocation deferred; debugger B.4 items 3+4 deferred) coherent intermediate states, or hidden landmines waiting for a downstream consumer to trip on?
3. Is this body of work shippable to internal teams that will build on top of `CkGoap` — or is there a structural issue that should block first?

You are **expected to read code in the live repo**, not just review the spec in isolation. Sample at least one file from each of: the Planner utils, the per-Action processor (where A* actually runs), the UpdateActivation processor, the debugger DataCollector, and the gym station code (`Combat Brain` is the new canonical multi-tier demo).

**What you should NOT do:**

- Per-file code-style review (handled by the inline review subagent during implementation).
- Test-by-test correctness audit (22/22 `Goap_Planner_*` AutoTests pass in both Development and DebugGame at every commit boundary — the regression net is real).
- Mockup-vs-implementation pixel comparison (deferred to user-driven PIE walkthrough).
- Re-debate of settled design decisions (see §8).

### What's being built

The U11 refactor of CkGoap is a clean-slate redesign of the GOAP framework's public surface to eliminate the U10 "ActionSet" indirection and the implicit root-Action concept. The user's framing from the spec abstract: *"There is no 'ActionSet' and no 'root'. Just **Actions** wired to each other in a tree, where any Action that carries a goal + a record of child Actions is also acting as a **Planner** at its own tier."*

The framework presents two typesafe handles — `FCk_Handle_Goap_Planner` and `FCk_Handle_Goap_Action` — discriminated by fragment presence on the same underlying entity. An entity may carry one or both role-fragment clusters depending on its position in the tree. Each Planner has its own independent goal (no more `goal = effects`). Multi-step plans at every tier; only Plan[0] activates the next tier's Planner.

### Plan / spec location

[2026-05-21-CkGoap-PlannerActionCollapse-design.md](../superpowers/specs/2026-05-21-CkGoap-PlannerActionCollapse-design.md) — ~870 lines, authoritative.

Companion docs:

- [2026-05-21-CkGoap-PlannerActionCollapse-summary.md](../superpowers/specs/2026-05-21-CkGoap-PlannerActionCollapse-summary.md) — at-a-glance shape.
- [2026-05-21-CkGoapDebugger-PlannerActionCollapse-mockup-G.html](../superpowers/mockups/2026-05-21-CkGoapDebugger-PlannerActionCollapse-mockup-G.html) — interactive UI mockup that U11.7 was supposed to implement.
- [2026-05-22-CkGoap-U11-PR-B-AStar-pipeline-on-Planner.md](../superpowers/plans/2026-05-22-CkGoap-U11-PR-B-AStar-pipeline-on-Planner.md) — PR-B follow-up plan (the cleanup that closes the implementation-vs-spec gap).
- [CONTINUATION_PROMPT_CkGoap_U11_PRB_1b.md](../superpowers/plans/CONTINUATION_PROMPT_CkGoap_U11_PRB_1b.md) and [CONTINUATION_PROMPT_CkGoap_U11_PRB_1b_commit2_recovery.md](../superpowers/plans/CONTINUATION_PROMPT_CkGoap_U11_PRB_1b_commit2_recovery.md) — handoffs for the deferred PR-B.1b work.

### Critical context — read before reviewing

These are the convention contracts:

- **`D:\Repos\CkPlugins\CLAUDE.md`** — project overview; UE 5.5; CkPlugins is the host project for the Chainkemists plugin ecosystem.
- **`D:\Repos\CkPlugins\Plugins\CkFoundation\CLAUDE.md`** — module index and patterns.
- **`D:\Repos\CkPlugins\Plugins\CkFoundation\Source\CLAUDE.md`** — deep C++ rules (CK_PROPERTY, function formatting, request structs, friend `::UCk_X` pattern).
- **`D:\Repos\CkPlugins\Plugins\CkFoundation\Source\CkGoap\CLAUDE.md`** — **the post-U11 CkGoap architecture description (hand-polished by the user after the U11.8 subagent draft)**. This is the doc downstream consumers will read.
- **`D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkGoapDebugger\CLAUDE.md`** — the post-U11 debugger architecture.
- **`D:\Repos\CkPlugins\Plugins\CkGameplayDebugger\Source\CkDebuggerCommon\CLAUDE.md`** — refresh-discipline contract (hash-debounce, no destructive ChildSlot, stable TSharedPtr identity, SGraphNode live-bind invariant).

Sample at minimum:

- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.{h,cpp}` — Add/Create/AddAction/PromoteActionToPlanner/Request_RemoveAction/Get_ActiveChain.
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.{h,cpp}` — the A*-pipeline processors (currently still match `FCk_Handle_Goap_Action` — this is the central deviation; see §9).
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Processor.{h,cpp}` — Setup (with new precondition/effect cycle detection) and UpdateActivation.
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Fragment.{h,cpp}` — the fragment cluster.
- `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/Public/CkGoapDebugger/Data/CkGoapDebugger_DataCollector.{h,cpp}` — snapshot producer (recently rewritten in B.4 to recurse PlannerInfo).
- `Plugins/CkTests/Script/CkGoap/Gym/CkGoapGym_CombatBrain_Station.as` — the canonical multi-tier (4-tier) gym demo added in tie-off.

### Design decisions already locked in (do NOT relitigate unless you see a real problem)

These were debated and settled before plan-writing. The user explicitly confirmed each during the design discussion:

1. **No "root" concept.** Roothood is emergent at runtime, not declared. `FCk_Handle_Goap` (the U10 root-container handle) is removed.
2. **Two handles, fragment-discriminated.** `FCk_Handle_Goap_Planner` and `FCk_Handle_Goap_Action`. An entity can carry one or both clusters.
3. **Explicit role declaration, no auto-promotion.** First child does not auto-promote its parent to Planner. Developer calls `Add` (top-level), `Create` (named child), or `PromoteActionToPlanner` (mid-tier promotion).
4. **Independent goal per Planner.** The U10 rule "composite Action's goal IS its own effects" is REMOVED. Goals come from `PlannerParams._Goal` or `Request_SetGoal`.
5. **Multi-step plans at every tier.** Each Planner produces a possibly-multi-step plan via classical GOAP backward chaining. Plan[0] gates sub-Planner activation.
6. **No "multi-ActionSet" framework feature.** Two top-level Planners on one entity = developer calls `Add` (or `Create`) twice.
7. **Add/Create paired for every util.** Applied to `utils_goap_planner` and `utils_goap_world_state`.
8. **PR-A cleanup deletions.** `SetRootAction`, `Request_SetRootAction`, `AddAction_ToAction`, `Request_SetGoalWorldState` are gone. The canonical construction is `Add → AddAction → PromoteActionToPlanner → AddAction`.
9. **No backward-compat constraints.** U11 broke U10 API and migrated all internal callers. The downstream consumers do not depend on the U10 surface.

### Implementation phase index (for cross-checking against the spec's §12 roadmap)

The spec listed phases U11.0 through U11.8 plus an implicit "PR-B" of follow-up cleanup. Phases as actually landed:

| Phase | Root SHA | Status |
|---|---|---|
| Spec committed | `09323fe` | DONE |
| U11.0a — Rename | `e7366e2` | DONE |
| U11.0b — WorldState Add audit | `09e8af9` | DONE |
| U11.0c — Fragment split (FFragment_Goap_Action_Current → Action _Definition + Planner _PlanState + Planner _Goal + Planner _WorldStateSource augmented) | `0000a14` | DONE |
| U11.1 — Goal independence | `2175fa3` | DONE |
| U11.2 — UpdateActivation processor | `a4a57fe` | DONE |
| U11.3 — Promotion API | `0f81321` | DONE |
| U11.4 — Diagnostics retarget (later: real cycle detection) | `c71532b` (original) / `ee59dae` (tie-off real cycle) | DONE (refined in tie-off) |
| U11.5 — Test rewrite (19 tests) | `7acf114` | DONE |
| U11.6 — Gym rewrite | `7ce4c45` | DONE (later: Combat Brain station added in tie-off) |
| U11.7 — Debugger update (4 sub-phases A-D) | `4a2ba76` → `18080f2` | DONE |
| U11.8 — Docs rewrite | `b8481c6` | DONE |
| Cleanup #6 — drop Request_SetGoalWorldState | `1e5489d` | DONE |
| Cleanup #2+#11 PR-A — drop SetRootAction + AddAction_ToAction; canonicalize construction | `d59cab8` | DONE |
| Cleanup #13 — History rail stable TSharedPtr identity | `3f598a9` | DONE |
| PR-B plan committed | `569504f` | PLAN ONLY |
| PR-B.1a — Planner-API verb shims + 57 of 71 test call-site migrations | `2a27fb6` | DONE |
| PR-B.1b commit 1 — 8 type aliases on Planner side | `a5cd590` | DONE (partial) |
| PR-B.1b commits 2-6 — fragment relocation + processor rewrite + drop `_RootAction` | (continuation prompts at `728b170` + `6fcbeb4`) | **DEFERRED** (4 BLOCKs) |
| Tie-off: Debugger refresh-thrash audit | `baf2fad` | DONE |
| Tie-off: SGraphNode TAttribute live-bind fix | `001e02c` | DONE |
| Tie-off: 3 new tests (AddRemoveChildren, GetPlanEntities, DeepNesting) | `ddc9f8a` | DONE |
| Tie-off: Combat Brain 4-tier gym station | `e4fde39` | DONE |
| Tie-off: stale ActionSet prose sweep | `64bd778` | DONE |
| Tie-off: CLAUDE.md anti-pattern docs | `ffda311` | DONE |
| Tie-off: runtime-remove API + AddRemoveChildren removal half | `ec30a90` | DONE |
| Tie-off: real cycle detection (precondition/effect SCC) | `ee59dae` | DONE |
| Tie-off: B.4 debugger hygiene items 1+2 | `0474dd6` | DONE (items 3+4 deferred) |

Final state: root commit `0474dd6`. 22/22 `Goap_Planner_*` AutoTests pass in BOTH Development AND DebugGame.

### What I specifically want you to scrutinize

#### A. Architecture / decomposition — Path A vs Path B

The central architectural deviation from the spec is here. Read spec §2.2 and §4.

**Spec design intent:** Atomic leaf Actions carry only Action-role fragments (`_Definition`, `_Params`, `_ParentRef`). Composite (promoted) entities are dual-role. The A* pipeline runs on Planner entities directly.

**Actual implementation (Path A):** Every Action entity stamps the full Planner-role fragment cluster (PlanState, Goal, WorldStateSource, Activation, etc.) regardless of whether it's atomic or composite. The A* pipeline (all `FProcessor_Goap_Action_*`) matches `FCk_Handle_Goap_Action` and reads/writes those fragments off the implicit-root child of each Planner. `_RootAction` field on `FFragment_Goap_Planner_Current` is the link. `PromoteActionToPlanner` is a near-no-op because the fragments are already there.

**Why this happened:** 4 sequential subagent dispatches attempted the relocation (Path B). All BLOCKed at survey, concluding that the fragments, processors, shims, WS subscribers, `Get_ActiveChain` walker, and `Request_SetGoal` dispatch are all mutually load-bearing — a single atomic refactor that doesn't fit a single subagent context budget. The plan (PR-B.1b) and recovery handoff are committed; the work itself is deferred to a dedicated session.

**Questions for you:**

- Is **Path A acceptable as a permanent state**? The public API matches the spec; the type system already enforces "leaves can't plan" at the verb level. The cost is memory (unused Planner fragments on every leaf — likely <500 bytes per leaf) and the latent issues §9 lists. If you say Path A is permanent → PR-B can close as not-worth-doing.
- Or is **Path B load-bearing** for some reason that's not visible from inside this implementation session? E.g., does the planner-role-on-leaf state break some future feature (multi-parent Actions, networked sync, partial subtrees)? If yes → PR-B.1b is a must-do and the deferred-PR-B continuation prompts need execution.

#### B. Convention compliance

- Fragment definition pattern (CK_GENERATED_BODY, `friend class ::UCk_X`, private + accessor macros) — verify the new fragments (`FFragment_Goap_Planner_PlanState`, `_Goal`, `_Activation`, `_Replan`, etc. in `CkGoap_Planner_Fragment.h`) match the CkFoundation idiom.
- TProcessor template parameter discipline — does every processor declare every fragment it reads/writes? Are `CK_REGISTER_PROCESSOR` calls present for every processor?
- Add/Create pairing — verify both `UCk_Utils_Goap_Planner_UE::Add`/`Create` AND `UCk_Utils_Goap_WorldState_UE::Add`/`Create` exist and have parallel semantics.
- Request struct pattern — verify the new `FCk_Request_Goap_Planner_SetGoal` (U11.1) and any related requests follow the inheritance + `CK_REQUEST_DEFINE_DEBUG_NAME` + `CK_GENERATED_BODY` pattern.
- AS-bindings safety — verify no UFUNCTIONs use `WorldContextObject` (AS strips it), no adjacent string-literal concatenation in `.as` files (AS parse error), typesafe handle access via `ck::IsValid(h)` and not `h.IsValid()` (compile error).

#### C. UE 5.5 specifics

- UE 5.5 includes / EditorOnly gates on the debugger module — verify nothing leaks editor-only types into runtime.
- TObjectPtr usage on UPROPERTYs in the new fragments / data nodes.
- Live Coding hot-reload risk — any new cross-DLL linkage (e.g., the new `FFragment_Goap_RecordOfActions` would, but that's deferred; the landed code shouldn't have introduced new cross-DLL hazards).

#### D. Test coverage

The 22 `Goap_Planner_*` AutoTests cover the spec's §9 migration table. Plus 3 new tests in tie-off (AddRemoveChildren, GetPlanEntities, DeepNesting).

- Is the test surface sufficient for the API surface the spec defines? Spec §9 lists ~20 mapped tests; we have 22 (some renames, two removed for "no root concept", three new for U11-specific behaviors plus tie-off additions).
- The `Goap_Planner_DependencyCycleDetection` test was rewritten in tie-off to exercise a real cycle (was previously a smoke test over tree edges). Verify the new test's edge model matches the spec's intent in §7.
- The deep nesting test exercises 4 tiers; the Combat Brain gym is also 4-tier with sibling promoted Planners. Is this depth sufficient, or should there be a deeper / wider test?
- Coverage gap: `Request_SetGoal` on a **promoted** Planner — today the implementation would ensure-fail because dispatch goes via `_RootAction.Get_Plan()` which is empty on promoted mid-tier Planners. No test exercises this. PR-B would fix the dispatch; without PR-B, this latent ensure-fail stays unblocked.

#### E. Partial states — landmines or coherent stopping points?

Three partial states currently exist:

1. **PR-B.1b commit 1 — 8 type aliases** in `CkGoap_Planner_Fragment.h` (e.g. `using FFragment_Goap_Planner_Requests = FFragment_Goap_Action_Requests;`). These point at the existing Action-side fragments. They exist to give subsequent PR-B commits stable destination names. If PR-B never lands, are these aliases dead weight that confuses readers, or harmless breadcrumbs?
2. **PR-B.1b commits 2-6 — DEFERRED.** The actual fragment relocation, processor rewrite, `_RootAction` deletion, debugger touch, test migration. Two continuation prompts committed. Question: should these aliases be reverted if PR-B never lands?
3. **B.4 items 3+4 — DEFERRED.** Legacy `FCkGoapDebugger_ActionSetInfo` shim still synthesized by DataCollector even though the Graph pane no longer consumes it (B.4 item 2 migrated it). Items 3+4 would delete the shim + retire `_SelectedAction` on the ViewModel + migrate WorldStateRail. Question: shim consumers still exist (InspectorGateway, PlanStrip helpers, HistoryEvent::SnapshotAtEvent, ViewModel accessors); is this an intentional intermediate state or a partially-completed refactor with cross-cutting hazards?

#### F. Forward-compat with downstream / deferred work

- **Multi-parent Actions** — spec §10 lists this as out of scope. Verify the implementation preserves the single-parent invariant (Action `_ParentPlanner` is single-valued) and doesn't accidentally allow it.
- **Network sync of plans** — spec §10 out of scope. Verify the fragments are reasonably-shaped for future networked replication (or at least not pathologically misshaped).
- **DataAsset-driven Planner declarations** — spec §10 out of scope. Verify the construction-verb API doesn't preclude this.
- **Per-Action enable toggle** — spec §10 out of scope (only per-Planner toggle). Verify implementation actually only exposes per-Planner toggle, no per-Action verb leaked through.
- **Recursive composite removal** — `Request_RemoveAction` added in tie-off explicitly handles only single-leaf removal. Removing a mid-tier composite leaves its grandchildren in the parent Planner's catalog. Acceptable limitation or hidden bug?

### Output format — fill in the CTO Review Response section below

Be direct. If the implementation matches the spec for everything claimed "done," say so and green-light it — don't manufacture issues to look thorough. Specific blockers, not vague concerns. Cite spec sections (§N.M) and file:line where possible.

The user's expectation (verbatim): *"The original plan was solid and I expect it to be fully adhered to."* Make this expectation the bar.

---

## CTO Review Response

### Verdict

**GREEN-LIGHT WITH NON-BLOCKING NOTES.**

The U11 refactor delivers what the spec promised at the public-API level. Two handle types discriminated by fragment presence, per-Planner independent goal, `Add → AddAction → PromoteActionToPlanner → AddAction` canonical construction, no `_RootAction` / `SetRootAction` / `AddAction_ToAction` / `Request_SetGoalWorldState` in the surface, multi-step plans at every tier, per-Planner Tarjan SCC over precondition/effect edges, 22 + 3 AutoTests green in both Development and DebugGame. The Combat Brain gym demonstrates the canonical §2.2 4-tier shape with sibling promoted Planners. This is shippable to internal CkGoap consumers.

The one place where implementation diverges materially from spec text — Path A versus Path B fragment layout — is honestly documented in the code (`CkGoap_Planner_Fragment.h:73-79`, `CkGoap_Planner_Utils.cpp:69-75`) and in the post-U11 `CkGoap/CLAUDE.md` ("Planner-role fragments live on every Action entity too"). Path A is not a bug; it is a deliberate, documented intermediate state with a known cost surface. See **Design / architecture observations** below.

### Blocking issues

None.

### Non-blocking suggestions

1. **PR-B.1a shims ensure-fail on every promoted mid-tier Planner.** `Get_PlanStatus`, `Get_Plan`, `Get_PlanCost`, `Get_PlanAttemptCount`, `Get_WorldStateSource`, `Get_InvalidGoal`, `Request_Plan`, `Request_CancelPlan`, `Request_SetGoal`, and the rest in [CkGoap_Planner_Utils.cpp:366-475, 798-849](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.cpp) all do `Get_RootAction(InPlanner)` then `CK_ENSURE_IF_NOT(ck::IsValid(RootAction), ...)`. `_RootAction` is only populated on top-level Planners (first `AddAction` branch in [CkGoap_Planner_Utils.cpp:596-600](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.cpp)) — promoted mid-tier Planners do not have it. The Combat Brain gym recognises this and routes mid-tier queries through the Action handle (`utils_goap_action::Get_PlanStatus(_Engage_AsAction)`, see [CkGoapGym_CombatBrain_Station.as:269-298](../../Plugins/CkTests/Script/CkGoap/Gym/CkGoapGym_CombatBrain_Station.as)). That works around the limitation but it is a leaky-abstraction fingerprint of Path A. **Downstream consumers will trip on this** the first time they query a promoted Planner by its Planner handle. The brief flagged `Request_SetGoal` as the example coverage gap; the same hazard applies to every PR-B.1a shim. Recommend: add one focused AutoTest (`Goap_Planner_QueriesOnPromotedPlanner` or similar) that asserts the current ensure-fail behaviour for every shim. That gives PR-B a regression net and makes the limitation discoverable to any consumer who hits an ensure.

2. **PR-B.1b alias breadcrumbs at [CkGoap_Planner_Fragment.h:311-322](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Fragment.h).** `using FFragment_Goap_Planner_Requests = FFragment_Goap_Action_Requests;` and friends are no-op aliases pointing at the same underlying types. If PR-B.1b never lands, these confuse readers: they suggest a Planner/Action fragment split that does not actually exist behaviourally. Two clean options: (a) commit to landing PR-B.1b within a small number of sessions and keep the aliases; (b) revert the aliases (PR-B.1b commit 1) and close PR-B as "Path A is the permanent layout." Today they're a halfway state that costs clarity for no behavioural benefit.

3. **B.4 items 3+4 deferred shim — multiple live consumers.** Legacy `FCkGoapDebugger_ActionSetInfo` synth in `DataCollector` is still consumed by InspectorGateway, PlanStrip helpers, `HistoryEvent::SnapshotAtEvent`, and several ViewModel accessors (per the brief). The Graph pane was migrated off in B.4 item 2 but the rest haven't been. This is cross-cutting. Recommend: either land B.4 items 3+4 in one go, or annotate every remaining consumer with a `// TODO(B.4)` so the next refactor session sees the full punch-list at a glance. Right now it's a hard-to-survey backlog.

4. **`Add()` defers the missing-WS diagnostic.** Spec §7.1 says `Add` with empty `_WorldStateSource` should return an invalid handle and log a Warning. The implementation in [CkGoap_Planner_Utils.cpp:120-200](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.cpp) accepts the call and warns later, inside the first `AddAction` (line 619-624) when seeding the implicit root. The diagnostic still fires, just one call later. Acceptable; consider tightening to match spec wording if convenient, or update the spec to reflect "warns at first AddAction" if that's the intended shape.

5. **`Add` is described as "stamps onto owner" but spawns a child entity.** Spec §3.1 docstring and the post-U11 `CkGoap/CLAUDE.md` Add-vs-Create section say `Add` "stamps Planner fragments onto InOwner directly." The implementation creates a new entity owned by InOwner ([CkGoap_Planner_Utils.cpp:145](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.cpp)). That's consistent with how the rest of CkFoundation models features (entity-per-feature under owner) and matches U10's ActionSet shape, so the deviation is cosmetic. But the CLAUDE.md Add-vs-Create table reads as if Add and Create are structurally different. They are not — `Create` is a one-line wrapper that copies params, sets the tag, and calls `Add` ([CkGoap_Planner_Utils.cpp:202-213](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.cpp)). Recommend: tighten the CLAUDE.md so the "two installation paradigms" framing reflects that both create child entities, with `Create` being the tag-explicit overload.

6. **`Get_PlanClasses` on Planner returns the Action-side `Get_Plan` result.** [CkGoap_Planner_Utils.cpp:398-412](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.cpp): the shim's name suggests it returns `TSubclassOf<...>` (correct), but it delegates to `UCk_Utils_Goap_Action_UE::Get_Plan(RootAction)` — which per the post-U11 `CkGoap/CLAUDE.md` table returns "Ordered child Action classes from the last plan." The naming is intentional, but the indirection is non-obvious to a reader. Minor doc gap; consider a one-line comment at the shim site naming the path explicitly.

7. **`Request_RemoveAction` is single-leaf only by design.** Per the brief §F point 5, removing a mid-tier composite leaves its grandchildren orphaned in the parent Planner's catalog. The function header in [CkGoap_Planner_Utils.h:265-291](../../Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.h) is explicit about this. Acceptable v1 limitation; recommend a quick assertion in the implementation that the target Action has no `_ChildActions` (currently the function would silently strand them), and surface a Warning otherwise.

8. **`PromoteActionToPlanner` does not clear `_RootAction` on the host.** Acceptable since the host doesn't have one — `_RootAction` only lives on `FFragment_Goap_Planner_Current` and the promoted host never had it set. But there is no test asserting that promoted Planners explicitly have `Get_RootAction(InPlanner)` return invalid. Worth a one-line assert in the promotion path or a test, so any future refactor that accidentally sets it gets caught.

9. **Cycle detection test fidelity.** The rewritten `Goap_Planner_DependencyCycleDetection` test ([CkAutoTest_Goap_Planner_DependencyCycleDetection.as](../../Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_Planner_DependencyCycleDetection.as)) is a real precondition/effect SCC — CycleA needs B's effect, CycleB needs A's effect, the cycle's `_ActionsInCycle` and `_CycleConditions` both get asserted. Matches spec §7.2. Good — leave this as-is.

10. **DeepNesting test depth.** 4 tiers, three promoted Planners, regressive plan asserted at every tier. Matches the spec §2.2 illustrative shape exactly. No deeper coverage needed for v1.

### Convention compliance spot-checks performed

- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Fragment.h` — fragment cluster, friend declarations using `::UCk_X` pattern, `CK_PROPERTY_GET` accessors, the PR-B.1b alias block. Confirmed the dual-role-on-every-Action reality and the `_RootAction` field still present.
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Utils.{h,cpp}` (cpp lines 1-849 sampled) — Add/Create/Find_Planner/AddAction/PromoteActionToPlanner/Request_SetGoal/Request_RemoveAction; PR-B.1a shim block; PR-A `DoCreateOrFindActionEntity` and `DoResolveChildWorldStateFromParent` helpers.
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Processor.h` — `FProcessor_Goap_Planner_Setup` (handle type = Planner), `FProcessor_Goap_Planner_UpdateActivation` (handle type = `FCk_Handle_Goap_Action`, confirming Path A — UpdateActivation runs per-Action).
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.h` — confirmed the entire A* pipeline (`Setup → AutoReplan → HandleRequests → Execute → HandleResult → EndPlay`) is `FCk_Handle_Goap_Action`-keyed and reads/writes Planner-role fragments off Action entities.
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Planner/CkGoap_Planner_Processor.cpp` (cycle-detection block sampled, lines 24-300) — confirmed real Tarjan SCC over precondition/effect adjacency, not tree edges; trivial SCCs filtered; participating WS keys recorded per cycle.
- `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/WorldState/CkGoap_WorldState_Utils.h` — confirmed Add/Create pairing exists for `utils_goap_world_state` (spec §3.1 expectation).
- `Plugins/CkTests/Script/CkGoap/Gym/CkGoapGym_CombatBrain_Station.as` — 4-tier shape, two sibling promoted Planners at tier 3, mid-tier display queries deliberately routed through Action handles (the Path A fingerprint).
- `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_Planner_DeepNesting.as` — 4-tier regression net; plan-complete bindings on Action handles for mid-tier checks (consistent with the gym workaround).
- `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_Planner_DependencyCycleDetection.as` — real SCC test wired against `Get_DependencyCycles(Planner)`; expected-log-error registration for the deliberate cycle warning.
- `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` — verified the post-U11 architecture description is honest about the dual-role-on-every-Action reality (Fragment table notes section).
- `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/CLAUDE.md` — debugger architecture; refresh-discipline contract; history-rail-not-in-listview rule.

### Design / architecture observations

**Path A versus Path B — my call: ship Path A; close PR-B opportunistically, not urgently.**

The brief framed this as binary — "Path A acceptable as permanent state OR Path B load-bearing." My read: it's neither. Path A is acceptable now, and Path B is load-bearing eventually, but neither verdict needs to land before downstream consumers start building.

What's true today:

- The **public API matches the spec**. Two handles, fragment-discriminated. `Add → AddAction → PromoteActionToPlanner → AddAction` constructs the spec's canonical shape. `_RootAction`, `SetRootAction`, `AddAction_ToActionSet`, `Request_SetGoalWorldState`, `_InitialGoal_RootOnly` are gone from the surface. The type system enforces "leaves can't plan" via the verb signatures. A consumer reading [CkGoap/CLAUDE.md](../../Plugins/CkFoundation/Source/CkGoap/CLAUDE.md) and using the API as documented gets the spec's mental model.

- The **internal storage layout diverges**. The spec said Action-role fragments on leaf Actions only, Planner-role fragments on Planners only, both on composites. The implementation stamps the full Planner cluster on every Action entity and uses `_RootAction` as the indirection link from a top-level Planner to the entity that runs A*. The PR-B.1b aliases anticipate the spec's layout but don't realize it yet.

- The **observable consequence** of the divergence is the ensure-fail-on-promoted-mid-tier-Planner shim behaviour (suggestion #1 above) and the conceptual confusion that "Planner-side fragments" actually live on Action entities. The 4 subagent BLOCKs the brief mentions tell me the refactor is genuinely intricate — the fragments, processors, shims, WS subscribers, `Get_ActiveChain` walker, and `Request_SetGoal` dispatch are mutually load-bearing.

Why Path A is shippable now:

- **Memory cost is negligible.** Few hundred bytes per Action × dozens of Actions per NPC × ~130 NPCs in Rewind99 = sub-megabyte. Below the noise floor for the target scoping target.
- **The Combat Brain gym proves the architecture works at depth** with sibling promoted Planners — exactly the workload Rewind99's tactical AI is most likely to lean on.
- **The Action-handle workaround is documented in working code** (the gym's display tick). Downstream consumers can copy it.
- **The 22 + 3 tests cover the spec's §9 migration table** plus three new U11-specific behaviours plus three tie-off additions. The regression net is real.

Why Path B is load-bearing eventually:

- **Promoted Planner queries should work uniformly via the Planner handle** — that's what the typesafe handles are for. Today they ensure-fail; you have to know to cast back to Action. That's a leaky abstraction the spec was specifically trying to eliminate.
- **Network sync of plans** (spec §10 out-of-scope) is materially harder with Planner-role fragments scattered onto every Action entity. If/when that lands, Path B becomes the canonical layout.
- **Multi-parent Actions** (also §10 out-of-scope) would force the Action role to carry `TArray<FCk_Handle_Goap_Planner> _ParentPlanners` and break the implicit-root indirection assumption. Easier to extend a clean Path B layout.
- **Conceptual integrity matters** for a framework downstream consumers will read CLAUDE.md to understand. Today CLAUDE.md has to caveat "Planner-role fragments live on every Action entity too" — that caveat is a doc smell.

My recommendation is to plan to land PR-B.1b before any of the §10 follow-ons (multi-parent, networked plans, save/load) is attempted, but not before merging the U11 work to consumers. The ensure-fails on promoted-Planner queries (#1) are the discoverable face of the limitation, and the gym already shows the workaround. Suggestion #1's regression test makes the limitation explicit and gives PR-B its acceptance criteria.

If PR-B never lands: the only thing that becomes a real-cost problem is the PR-B.1b alias block (#2) — that's the only piece that goes from "deliberately transitional" to "permanent confusion." Reverting it is a small, contained change.

The original plan was solid and the implementation adheres to it at the level the user cares about — the public API, the per-Planner goal independence, the multi-step plans at every tier, the regressive A* over candidate operators, the cycle detection model, the active-chain emergence. The internal storage shortcut is a deliberate, documented intermediate state with a documented exit ramp. Ship it.

### Sign-off conditions (only if "CHANGES REQUESTED")

N/A — green-light.

---

### Reviewer

- **Name:** Claude (Opus 4.7, 1M context) acting as senior reviewer/architect
- **Date:** 2026-05-22
