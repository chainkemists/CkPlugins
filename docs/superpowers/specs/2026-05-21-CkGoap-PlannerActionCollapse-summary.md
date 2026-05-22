# CkGoap Planner / Action Collapse — At-a-Glance Summary

**Companion to:** `2026-05-21-CkGoap-PlannerActionCollapse-design.md`

This is the short version. For full rationale, fragment tables, processor pseudocode, migration mapping, and implementation roadmap, read the design spec.

---

## The model in one paragraph

There is no "ActionSet" and no "root". Just **Actions** wired to each other in a tree, where any Action that carries a goal + a record of child Actions is also acting as a **Planner** at its own tier. The framework presents two typesafe handles — `FCk_Handle_Goap_Planner` and `FCk_Handle_Goap_Action` — discriminated by fragment presence on the same underlying entity. An entity may carry one or both role-fragment clusters depending on its position in the tree. Each Planner has its own independent goal (no more goal = effects). Each Planner produces a multi-step plan; only Plan[0] activates the next tier's Planner. The "active chain" emerges from runtime Plan[0] walks; no fragment stores it.

---

## Role distribution on an entity tree

```
Owner entity                                       (just any FCk_Handle the dev manages)
 └── Planner_Alive               PLANNER ONLY      ← top-level: dev called utils_goap_planner::Add on Owner.
                                                     No parent picks it; no Action role.
       │
       ├── Action_Engage         ACTION+PLANNER    ← dev called utils_goap_planner::AddAction on Planner_Alive,
       │                                             then utils_goap_planner::PromoteActionToPlanner on this
       │                                             handle to make it a planner with children.
       │
       │     ├── Action_LightAttacks   ACTION+PLANNER   (same pattern: AddAction then Promote)
       │     │     ├── Action_Light1   ACTION ONLY     ← leaf: AddAction only, no promotion. No children.
       │     │     ├── Action_Light2   ACTION ONLY
       │     │     └── Action_Light3   ACTION ONLY
       │     ├── Action_HeavyAttacks   ACTION+PLANNER
       │     │     ├── Action_Heavy1   ACTION ONLY
       │     │     └── Action_Heavy2   ACTION ONLY
       │     └── Action_WalkToEnemy    ACTION+PLANNER
       │           ├── Action_Walk     ACTION ONLY
       │           └── Action_Run      ACTION ONLY
       │
       ├── Action_Survive        ACTION+PLANNER
       │     └── ... (similar)
       └── Action_Idle           ACTION+PLANNER
             └── ... (similar)
```

Casting:

```cpp
// Planner_Alive:           Cast as Planner → ✓.  Cast as Action → ✗ (no Action_Definition fragment).
// Action_Engage:           Cast as Planner → ✓.  Cast as Action → ✓.   (dual-role)
// Action_Light1:           Cast as Planner → ✗.  Cast as Action → ✓.   (leaf)
```

The Planner-role discriminator fragment is `FFragment_Goap_RecordOfActions`. The Action-role discriminator is `FFragment_Goap_Action_Definition`.

---

## How a plan flows through the tiers

```
Tier 0  Planner_Alive    goal: EnemyDead=true
        plan:    [WalkToEnemy, Engage, KillEnemy]      ← multi-step. Plan[0] = WalkToEnemy.
        Plan[0]:  WalkToEnemy                          ← this is a Planner; activates Tier 1.
                       │
                       ▼
Tier 1  Planner_WalkToEnemy  goal: PlayerInRange=true  (its own goal — independent from above)
        plan:    [Run]                                 ← single-step, because Run alone satisfies the goal.
        Plan[0]:  Run                                  ← Run is action-only (leaf); no further activation.
                       │
                       ▼
                  (runner executes Run — gameplay physically runs the entity toward the player)
                       │
                       │  as the entity closes distance, gameplay flips PlayerInRange=true
                       ▼
                  Tier 1 replans → plan empty (goal satisfied). Plan[0] = invalid.
                  Tier 0's UpdateActivation sees its old Plan[0] (WalkToEnemy) is no longer chosen
                  (because Tier 0 also replans now that PlayerInRange is true) and flips to Engage.
                       │
                       ▼
Tier 0  Planner_Alive    plan: [Engage, KillEnemy]    ← Plan[0] is now Engage.
        Plan[0]:  Engage  ← also a Planner; activates Tier 1 (now Engage instead of WalkToEnemy).
                       │
                       ▼
Tier 1  Planner_Engage   goal: PlayerHurt=true        (its own goal again — different from Tier 0's)
        plan:    [LightAttacks]
        Plan[0]:  LightAttacks  ← Planner; activates Tier 2.
                       │
                       ▼
Tier 2  Planner_LightAttacks  goal: HitLanded=true
        plan:    [Light2]                              ← Light1 just landed last frame, so its
                                                         LastAttack.WasLight precondition fails;
                                                         Light2 wins this tick.
        Plan[0]:  Light2        ← Action only (no Planner role). End of activation chain.
                       │
                       ▼
                  Runner executes Light2.
```

**Key properties on display here:**

- Each tier has its own goal. Goals are not the same across tiers and don't have to be.
- Each tier's plan is a possibly-multi-step sequence. Only `Plan[0]` activates the next tier.
- As gameplay progresses and WS changes, each tier replans on its own schedule. Plan[0] naturally advances.
- The chain depth at any moment is the count of currently-active Planners walking Plan[0] downward. It's not stored; it's derived.

---

## API call sequence (building the 3-tier brain above)

```cpp
// 1. Top-level Planner on the enemy entity. Owner becomes a Planner.
auto Alive = utils_goap_planner::Add(EnemyEntity,
    FCk_Fragment_Goap_PlannerParamsData(
        Goal: {EnemyDead=true},
        WS:   EnemyWorldStateHandle));

// 2. Register child Actions under Alive.
auto Engage      = utils_goap_planner::AddAction(Alive, ActionParams(UCk_Engage_Class));
auto WalkToEnemy = utils_goap_planner::AddAction(Alive, ActionParams(UCk_WalkToEnemy_Class));
auto KillEnemy   = utils_goap_planner::AddAction(Alive, ActionParams(UCk_KillEnemy_Class));
auto Survive     = utils_goap_planner::AddAction(Alive, ActionParams(UCk_Survive_Class));
auto Idle        = utils_goap_planner::AddAction(Alive, ActionParams(UCk_Idle_Class));

// 3. Promote Engage to be a Planner (it now plans over its own children).
auto Engage_AsPlanner = utils_goap_planner::PromoteActionToPlanner(Engage,
    PlannerParams(Goal: {PlayerHurt=true}));   // WS inherited from parent (Alive's WS)

// 4. Register Engage's children.
auto LightAttacks = utils_goap_planner::AddAction(Engage_AsPlanner, ActionParams(UCk_LightAttacks_Class));
auto HeavyAttacks = utils_goap_planner::AddAction(Engage_AsPlanner, ActionParams(UCk_HeavyAttacks_Class));

// 5. Promote LightAttacks; register its children (the actual atomic attacks).
auto LightAttacks_AsPlanner = utils_goap_planner::PromoteActionToPlanner(LightAttacks,
    PlannerParams(Goal: {HitLanded=true}));
utils_goap_planner::AddAction(LightAttacks_AsPlanner, ActionParams(UCk_Light1_Class));
utils_goap_planner::AddAction(LightAttacks_AsPlanner, ActionParams(UCk_Light2_Class));
utils_goap_planner::AddAction(LightAttacks_AsPlanner, ActionParams(UCk_Light3_Class));

// 6. Promote WalkToEnemy; register Walk + Run.
auto WalkToEnemy_AsPlanner = utils_goap_planner::PromoteActionToPlanner(WalkToEnemy,
    PlannerParams(Goal: {PlayerInRange=true}));
utils_goap_planner::AddAction(WalkToEnemy_AsPlanner, ActionParams(UCk_Walk_Class));
utils_goap_planner::AddAction(WalkToEnemy_AsPlanner, ActionParams(UCk_Run_Class));

// Same pattern for HeavyAttacks, Survive, Idle subtrees.
```

**Type-system enforcement points:**

- Step 2's `AddAction` requires a `FCk_Handle_Goap_Planner` as its first arg. Compile error if you tried `AddAction(Engage, ...)` before Engage is promoted.
- Step 3 returns a `FCk_Handle_Goap_Planner` distinct from the `FCk_Handle_Goap_Action` returned by step 2 — same underlying entity, two handles.
- Step 4 uses `Engage_AsPlanner` (the planner-handle), so the compile-time guard holds.

---

## What changes from ActionSetUnification (U0–U10)

| Concept | Before (U0–U10) | After (U11) |
|---|---|---|
| Top-level container | `FCk_Handle_Goap` root entity holding `FCk_Handle_Goap_ActionSet`s | (gone — top-level Planner IS the entry point) |
| Decision container | `FCk_Handle_Goap_ActionSet` | `FCk_Handle_Goap_Planner` (same role, renamed + collapsed with root) |
| Decision-and-Action | A composite `FCk_Handle_Goap_Action` (children record + goal = effects) | Dual-role entity: `FCk_Handle_Goap_Planner` AND `FCk_Handle_Goap_Action` casts |
| Goal source for composites | Goal = own effects (hardwired) | Goal = independent `_Goal` field, settable per Planner |
| Active chain | Stored on ActionSet | Implicit; derived by Plan[0] walk |
| Chain extension processor | `FProcessor_Goap_ActionSet_ChainUpdate` walks chain top-down | `FProcessor_Goap_Planner_UpdateActivation` runs per-Planner; activation emerges from recursion |
| Multi-domain on one entity | Multiple ActionSets under one Goap root | Multiple top-level Planners (`utils_goap_planner::Create` for named child planners) |
| Hierarchy depth | Bundle / Tier / Action (3 levels max per ActionSet) | Arbitrary depth — Planners all the way down |

---

## What stays unchanged

- Parent-plan gating (U9 mechanism) — the `FTag_Goap_Planner_PlanInFlight` retains the same semantics, just on Planners instead of Actions.
- Per-Planner replan policies (`Explicit` / `OnWorldStateDirty` / `OnCostDirty` / `OnEitherDirty`) + throttle.
- A* search machinery + time-slicing.
- WorldState entity, key registry, dirty-tag plumbing.
- Single-parent tree invariant — each Action has at most one `_ParentPlanner` reference.
- Diagnostics shape (`_InvalidGoal`, `_DependencyCycles`) — but per-Planner now.

---

## Implementation roadmap

8 phases. Each phase ends with a verified commit set; tests pass before the next phase starts.

| Phase | What lands |
|---|---|
| U11.0 | Rename ActionSet → Planner; collapse Goap root; split Action_Current fragment into Action/Planner role clusters. Build green. |
| U11.1 | Independent per-Planner goal (`_Goal` on PlannerParams, `Request_SetGoal`); remove goal = effects coupling. |
| U11.2 | Replace ChainUpdate with per-Planner UpdateActivation; preserve OnPlannerActivated/Deactivated semantics. |
| U11.3 | Add `PromoteActionToPlanner` API; verify dual-cast on the promoted entity. |
| U11.4 | Retarget diagnostics (`_InvalidGoal`, `_DependencyCycles`) per-Planner. |
| U11.5 | Rewrite the 19+5 ActionSetUnification AutoTests against the new API; add 3 new U11-specific tests. |
| U11.6 | Rewrite the 3 gyms (boolean / autoreplan / empire) for Planner-shaped trees. |
| U11.7 | Update the debugger UI: per-Planner active-chain rendering, per-tier plan strips, tree edges in the graph (D11 work folded in). |
| U11.8 | Rewrite CLAUDE.mds for runtime + debugger; add SUPERSEDED banner on the ActionSetUnification spec. |

Target: U11 complete + 22+ tests green + 3 gyms working + debugger renders multi-tier brains cleanly.

---

*End of summary. Full design in the companion spec.*
