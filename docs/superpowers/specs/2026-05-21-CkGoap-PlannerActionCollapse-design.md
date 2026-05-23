# CkGoap Planner / Action Collapse — Design Spec

**Date:** 2026-05-21
**Status:** Approved (design phase)
**Author:** Claude, continuing the architecture discussion with @Sulfur-CK that revealed the ActionSet abstraction was a leaked Bundle/Tier vestige.
**Supersedes:** `2026-05-19-CkGoap-ActionSetUnification-design.md`. That spec's `ActionSet` and `Action` types collapse here into a single Action-shaped entity that may simultaneously play the **Planner role** and the **Action role**. The runtime fact "this Action has children registered to it" is no longer expressed via a separate ActionSet container — it's just a property of the entity itself.
**Implementation status of superseded spec:** Phases U0–U10 of the ActionSetUnification refactor are committed and green (19 AutoTests + 3 gyms + working debugger). This refactor reshapes the API and fragment layout, replaces ChainUpdate with per-Planner UpdateActivation, and removes the goal=effects coupling from composite Actions — but it preserves most of the processor logic and search machinery.

---

## Quick index

1. [Rationale — why collapse ActionSet](#1-rationale--why-collapse-actionset)
2. [Data model — handles, fragments, dual-role entities](#2-data-model--handles-fragments-dual-role-entities)
3. [API surface](#3-api-surface)
4. [Per-Planner processor flow](#4-per-planner-processor-flow)
5. [WorldState resolution & dirty/replan](#5-worldstate-resolution--dirtyreplan)
6. [Lifecycle invariants](#6-lifecycle-invariants)
7. [Diagnostics](#7-diagnostics)
8. [Migration from ActionSetUnification](#8-migration-from-actionsetunification)
9. [Test surface](#9-test-surface)
10. [Out of scope (v1)](#10-out-of-scope-v1)
11. [Open questions / risks](#11-open-questions--risks)
12. [Implementation roadmap](#12-implementation-roadmap)
13. [Appendix A — Glossary](#appendix-a--glossary)
14. [Appendix B — Naming changes from ActionSetUnification](#appendix-b--naming-changes-from-actionsetunification)

---

## 1. Rationale — why collapse ActionSet

The ActionSetUnification spec inherited a Bundle/Tier vestige: an "ActionSet" container holding a root Action plus a flat catalog of all Actions, sitting between the Goap-root entity and the individual Action entities. This made three things awkward:

1. **There were three handle types** (`FCk_Handle_Goap` for the root container, `FCk_Handle_Goap_ActionSet` for the container of decisions, `FCk_Handle_Goap_Action` for the units of work) when in fact only **two distinct roles** exist:
   - **Planner role**: "I produce plans against a goal using my registered children as candidate operators."
   - **Action role**: "I am a unit of work that some other planner can include in its plan."

   Top-level entry points carry only the Planner role; leaf operators carry only the Action role; everything in between (the "composites") carries **both**, on the same entity. The framework should let the type system reflect that, with one handle per role and the underlying entity carrying whichever fragment clusters apply.

2. **A composite Action's goal was forced to equal its own declared effects** (§5.2 of the ActionSetUnification spec). This coupled two things that are actually independent: the *interface* the parent's planner sees (this Action's effects — what does it accomplish from outside) versus the *implementation* this Action plans toward (this Action's own goal — what does this tier try to achieve internally). Real GOAP designs benefit from being able to set those independently — e.g., `ChooseAttack` might tell its parent "I produce `PlayerHurtSomehow=true`" via its effect while internally planning toward `Combat.LightHit.Landed=true` for tactical reasons.

3. **The "root" concept implied a fixed top tier**, but the user's `tuq` architecture treats every tier symmetrically: any Action with children is a planner at its own level; any planner can be added as a child of an even-higher planner later. There is no special root type. Roots *emerge* from runtime topology — exactly the way Entity in ECS has no "root Entity" type. The framework should reflect that emergence rather than baking in a designated top.

This refactor addresses all three:

- One unified entity shape carrying one or both role-fragment clusters; two typesafe handles (`FCk_Handle_Goap_Planner`, `FCk_Handle_Goap_Action`) discriminated by fragment presence.
- Per-Planner independent goal, set in PlannerParams at registration, queryable + mutable at runtime via `Request_SetGoal`.
- No root concept. Whether an Action is "the top" is observable (no parent Planner pointing at it) but not declared.

The data model carries through largely unchanged — the parent/child wiring, the per-Planner A* state, the parent-plan gating from U9, the in-plan tinting derivation from D10 all still apply. What changes is **how the framework presents itself** at the API surface and **how the role-fragment clusters are laid out** in the ECS.

---

## 2. Data model — handles, fragments, dual-role entities

### 2.1 Typesafe handles

Two handles, both wrapping `FCk_Handle` (the generic ECS entity handle), each discriminated by a marker fragment:

| Handle | Discriminator | Means |
|---|---|---|
| `FCk_Handle_Goap_Planner` | `FFragment_Goap_RecordOfActions` | This entity is a planner: it has a goal and a record of child Actions to plan over. |
| `FCk_Handle_Goap_Action` | `FFragment_Goap_Action_Definition` | This entity is a unit of work: it has CDO-extracted preconditions, effects, and cost that some parent planner can include in its plan. |

`FCk_Handle_Goap` (the old root container handle) **is removed**. There is no separate root container entity.

An entity can carry **one or both** role-fragment clusters, depending on its position in the tree:

| Position | Action role | Planner role |
|---|---|---|
| Top-level Planner (no parent picks it as a step) | ✗ | ✓ |
| Leaf Action (no children registered) | ✓ | ✗ |
| Mid-tier composite (picked by parent, plans over children) | ✓ | ✓ |

Both casts succeed for mid-tier composites:

```cpp
// e.g., Engage is a child of Alive (Action role) AND has LightAttacks/HeavyAttacks
// children of its own (Planner role).
auto EngageAsAction  = UCk_Utils_Goap_Action_UE::Cast(EngageEntity);   // valid
auto EngageAsPlanner = UCk_Utils_Goap_Planner_UE::Cast(EngageEntity);  // valid (same underlying handle)
```

### 2.2 Entity hierarchy (illustrative)

```
Owner entity (e.g., enemy NPC)
  └── Planner_Alive                       Planner role only
        ├── Action_Engage                  Action + Planner roles
        │     ├── Action_LightAttacks      Action + Planner roles
        │     │     ├── Action_Light1      Action role only
        │     │     ├── Action_Light2      Action role only
        │     │     └── Action_Light3      Action role only
        │     ├── Action_HeavyAttacks      Action + Planner roles
        │     │     ├── Action_Heavy1      Action role only
        │     │     └── Action_Heavy2      Action role only
        │     └── Action_WalkToEnemy       Action + Planner roles
        │           ├── Action_Walk        Action role only
        │           └── Action_Run         Action role only
        ├── Action_Survive                 Action + Planner roles
        │     └── ... (similar shape)
        └── Action_Idle                    Action + Planner roles
              └── ...
```

Notes on this shape:

- `Planner_Alive` carries Planner fragments only. No parent picks it as a step (it's the top), so it has no effects/cost/preconditions to declare.
- Every node beneath it carries the Action role at minimum. If it also has children, it carries the Planner role.
- The shape is purely emergent from `utils_goap_planner::Add` + `AddAction` calls. The framework doesn't declare any node "root" — it just observes that nothing references `Planner_Alive` as a child, so it's the top of this entity's tree.

### 2.3 Fragment table

#### Action role fragments

| Fragment | Purpose |
|---|---|
| `FFragment_Goap_Action_Definition` | CDO-extracted `_Preconditions`, `_Effects`, `_Cost`. Discriminator for the Action role. |
| `FFragment_Goap_Action_Params` | `_ActionClass` (the `UCk_GoapAction_EntityScript` subclass), per-Action WS override, runtime cost overrides. |
| `FFragment_Goap_Action_ParentRef` | `_ParentPlanner` handle — the Planner that picks this Action as a plan step. Invalid for top-level entities. |

#### Planner role fragments

| Fragment | Purpose |
|---|---|
| `FFragment_Goap_RecordOfActions` | The record of child Action handles. Discriminator for the Planner role. May be empty briefly (planner is set up but no children yet) — that's a valid intermediate state at construction time. |
| `FFragment_Goap_Planner_Goal` | `TArray<FCk_GoapWS_Condition_Authored>` — what this planner is planning toward at its own tier. |
| `FFragment_Goap_Planner_PlanState` | `_Plan` (`TArray<FCk_Handle_Goap_Action>`), `_PlanStatus`, `_PlanCost`, `_PlanAttemptCount`. |
| `FFragment_Goap_Planner_Replan` | Replan policy, throttle accumulator, min replan interval. |
| `FFragment_Goap_Planner_Requests` | std::variant queue for `Plan` / `CancelPlan` / `SetCost` / `SetGoal` / etc. |
| `FFragment_Goap_Planner_WorldStateSource` | `_WorldStateSource_Override` (settable) + `_WorldStateSource_Resolved` (computed at activation). |
| `FFragment_Goap_Planner_SearchState` / `_Result` / `_PlanContext` | A* state (same as today). |
| `FFragment_AStar_Params` | Underlying CkAStar config — search budget, cost threshold. |

#### Tags

| Tag | On | Purpose |
|---|---|---|
| `FTag_Goap_Planner_RequiresSetup` | Planner | One-shot setup gate. |
| `FTag_Goap_Planner_RequiresInitialPlan` | Planner | Drives first plan after activation. |
| `FTag_Goap_Planner_PlanRequested` | Planner | Request-flow gate. |
| `FTag_Goap_Planner_PlanInFlight` | Planner | Parent-plan gating from U9, retained verbatim. |
| `FTag_Goap_Planner_Dirty_WorldState` | Planner | WS dirty trigger for AutoReplan. |
| `FTag_Goap_Planner_Dirty_Cost` | Planner | Child-cost dirty trigger. |

Note: the Action role carries no tags of its own. All planner activity (RequiresSetup, Dirty, etc.) is per-Planner.

### 2.4 The goal field — no more goal=effects coupling

Each Planner has its own `_Goal` (in `FFragment_Goap_Planner_Goal`), settable at construction via `PlannerParams._Goal` and at runtime via `Request_SetGoal`. The Planner's goal is **completely independent** of:

- Any Action role this same entity may carry (its effects, preconditions, cost).
- Any parent Planner's goal.
- Any descendant Planner's goal.

The Action role's effects are what the parent's planner consumes when deciding "should I include this Action in my plan?". They are not what this entity's own planner plans toward. Those are different layers of meaning.

### 2.5 No more "root" concept

There is no `_RootAction` field on a Planner. The "top" of any Planner tree is observable at runtime by walking the `_ParentPlanner` ref upward until you hit an entity that has no Action role (i.e., is a Planner-only entity). The framework offers convenience helpers (`Find_TopLevelPlanner(Entity)`) but no fragment declares roothood.

---

## 3. API surface

The API splits into two utility classes: `UCk_Utils_Goap_Planner_UE` for everything Planner-shaped, and `UCk_Utils_Goap_Action_UE` for everything Action-shaped. A third class, `UCk_Utils_Goap_WorldState_UE`, remains unchanged (WS values are orthogonal to the planner/action split).

### 3.1 Construction verbs (Planner-side)

```cpp
// Create a top-level Planner on an existing entity. The owner becomes a Planner
// (gains FFragment_Goap_RecordOfActions etc.). Returns the typesafe handle.
//
// Use this when you want the Goap "feature" to live directly on an entity that
// you already manage (NPC pawn, enemy actor, etc.).
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Planner",
          DisplayName = "[Ck][Goap|Planner] Add")
static FCk_Handle_Goap_Planner Add(
    UPARAM(ref) FCk_Handle& InOwner,
    const FCk_Fragment_Goap_PlannerParamsData& InParams);

// Convenience overload: promote an existing Action entity to also carry the
// Planner role. The entity now carries both fragment clusters; the action role
// remains intact (the parent that picked this Action still references it the
// same way). After promotion you can AddAction on this handle to register
// children.
//
// This is the "Action that's also a Planner" case the user described.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Planner",
          DisplayName = "[Ck][Goap|Planner] Promote Action To Planner")
static FCk_Handle_Goap_Planner PromoteActionToPlanner(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction,
    const FCk_Fragment_Goap_PlannerParamsData& InParams);

// Spawn a child entity that hosts a new Planner. Use when you want multiple
// independent planners on a single owner (e.g., a "combat planner" + a
// "dialogue planner" on the same NPC, ticking at different frequencies).
// The owner gets a record of these named planners; lookup by tag.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Planner",
          DisplayName = "[Ck][Goap|Planner] Create")
static FCk_Handle_Goap_Planner Create(
    UPARAM(ref) FCk_Handle& InOwner,
    FGameplayTag InPlannerTag,
    const FCk_Fragment_Goap_PlannerParamsData& InParams);

// Register a child Action under a Planner. The new entity carries the Action
// role. It does NOT carry the Planner role unless you subsequently call
// PromoteActionToPlanner on it (explicit — see rationale).
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Planner",
          DisplayName = "[Ck][Goap|Planner] Add Action")
static FCk_Handle_Goap_Action AddAction(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner,
    const FCk_Fragment_Goap_ActionParamsData& InParams);
```

### 3.2 Params structs

```cpp
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_PlannerParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_PlannerParamsData);

private:
    // The goal this planner plans toward at its own tier.
    // Required for top-level Planners. May be empty for sub-Planners that
    // intend to set their goal later via Request_SetGoal — but we log a
    // Verbose hint at Setup if a Planner has children but no goal.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FCk_GoapWS_Condition_Authored> _Goal;

    // The WorldState this planner reads. Required for top-level Planners.
    // For Planners promoted from existing Actions: optional — if unset, the
    // planner inherits the parent's resolved WS at activation time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Handle_Goap_WorldState _WorldStateSource;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_Goap_ReplanPolicy _ReplanPolicy = ECk_Goap_ReplanPolicy::OnWorldStateDirty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    float _MinReplanIntervalSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    int64 _SearchBudgetMicroseconds = 0;   // 0 = unbounded

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    float _CostThreshold = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _PlanOnStart = true;

public:
    CK_PROPERTY(_Goal);
    CK_PROPERTY(_WorldStateSource);
    CK_PROPERTY(_ReplanPolicy);
    CK_PROPERTY(_MinReplanIntervalSeconds);
    CK_PROPERTY(_SearchBudgetMicroseconds);
    CK_PROPERTY(_CostThreshold);
    CK_PROPERTY(_PlanOnStart);

    CK_DEFINE_CONSTRUCTORS(FCk_Fragment_Goap_PlannerParamsData, _Goal, _WorldStateSource);
};

USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_ActionParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_ActionParamsData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSubclassOf<UCk_GoapAction_EntityScript> _ActionClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Handle_Goap_WorldState _WorldStateSource_Override;

public:
    CK_PROPERTY_GET(_ActionClass);
    CK_PROPERTY_SET(_ActionClass);
    CK_PROPERTY(_WorldStateSource_Override);
    CK_DEFINE_CONSTRUCTORS(FCk_Fragment_Goap_ActionParamsData, _ActionClass);
};
```

Note `ActionParamsData` no longer carries `_InitialGoal_RootOnly`. Goal is purely a Planner concern.

### 3.3 Runtime mutation (Planner-side)

```cpp
// Force a fresh plan.
static FCk_Handle_Goap_Planner Request_Plan(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner);

// Cancel an in-flight plan.
static FCk_Handle_Goap_Planner Request_CancelPlan(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner);

// Set the goal at runtime. Triggers a replan.
static FCk_Handle_Goap_Planner Request_SetGoal(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner,
    const TArray<FCk_GoapWS_Condition_Authored>& InGoal);

// Adjust a child Action's cost (affects this planner's plan, not the child's
// own planning if the child is also a Planner).
static FCk_Handle_Goap_Planner Request_SetChildActionCost(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner,
    TSubclassOf<UCk_GoapAction_EntityScript> InChildClass,
    float InCost);

static FCk_Handle_Goap_Planner Request_SetReplanInterval(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner, float InSeconds);

static FCk_Handle_Goap_Planner Request_SetReplanPolicy(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner, ECk_Goap_ReplanPolicy InPolicy);

static FCk_Handle_Goap_Planner Request_SetSearchBudget(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner, int64 InMicroseconds);

static FCk_Handle_Goap_Planner Request_SetCostThreshold(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner, float InThreshold);

// Set the WS source. Resolves through the parent chain on next activation.
static FCk_Handle_Goap_Planner Request_SetWorldStateSource(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner,
    UPARAM(ref) FCk_Handle_Goap_WorldState& InWS);

// Deactivate the entire sub-tree under this Planner: any sub-Planner that's
// currently active (via Plan[0] chains) gets deactivated. Useful after a hard
// world-state change or a scripted teleport.
static FCk_Handle_Goap_Planner Request_DeactivateChildren(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner);

// Enable/disable the Planner. Disabled Planners don't replan and don't
// activate their children. Same semantics as ActionSet.EnableToggle was.
static FCk_Handle_Goap_Planner Request_SetEnableToggle(
    UPARAM(ref) FCk_Handle_Goap_Planner& InPlanner,
    ECk_EnableDisable InToggle);
```

### 3.4 Query verbs

```cpp
// Planner-side
static bool Has(const FCk_Handle& InHandle);
static FCk_Handle_Goap_Planner Find_Planner(
    const FCk_Handle& InOwner, FGameplayTag InPlannerTag);
static ECk_GoapPlanStatus Get_PlanStatus(const FCk_Handle_Goap_Planner& InPlanner);
static TArray<FCk_Handle_Goap_Action> Get_Plan(const FCk_Handle_Goap_Planner& InPlanner);
static TArray<TSubclassOf<UCk_GoapAction_EntityScript>>
       Get_PlanClasses(const FCk_Handle_Goap_Planner& InPlanner);
static float Get_PlanCost(const FCk_Handle_Goap_Planner& InPlanner);
static TArray<FCk_GoapWS_Condition_Authored>
       Get_Goal(const FCk_Handle_Goap_Planner& InPlanner);
static FCk_Handle_Goap_WorldState
       Get_WorldStateSource(const FCk_Handle_Goap_Planner& InPlanner);
static ECk_EnableDisable Get_EnableToggle(const FCk_Handle_Goap_Planner& InPlanner);
static TArray<FCk_Handle_Goap_Action>
       Get_ChildActions(const FCk_Handle_Goap_Planner& InPlanner);

// Active chain — implicit, derived on demand by walking Plan[0]→Plan[0]→…
// starting from this Planner. Stops at a leaf Action or at an inactive sub-
// Planner. Each element of the returned array is the Planner at that tier (if
// the entity carries the Planner role) plus the Action that's currently Plan[0].
static TArray<FCk_Handle_Goap_Planner>
       Get_ActiveChain(const FCk_Handle_Goap_Planner& InPlanner);

// Diagnostics
static TArray<FCk_GoapWS_Condition_Authored>
       Get_InvalidGoal(const FCk_Handle_Goap_Planner& InPlanner);
static TArray<FCk_GoapDiagnostic_DependencyCycle>
       Get_DependencyCycles(const FCk_Handle_Goap_Planner& InPlanner);

// Action-side
static FCk_Handle_Goap_Planner
       Get_ParentPlanner(const FCk_Handle_Goap_Action& InAction);
static TSubclassOf<UCk_GoapAction_EntityScript>
       Get_ActionClass(const FCk_Handle_Goap_Action& InAction);
static FGameplayTag Get_ActionTag(const FCk_Handle_Goap_Action& InAction);  // class-derived
static TArray<FCk_GoapWS_Condition_Authored>
       Get_Preconditions(const FCk_Handle_Goap_Action& InAction);
static TArray<FCk_GoapWS_Condition_Authored>
       Get_Effects(const FCk_Handle_Goap_Action& InAction);
static float Get_Cost(const FCk_Handle_Goap_Action& InAction);
```

### 3.5 Signals

```cpp
// Per-Planner signals
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanComplete,
    FCk_Delegate_Goap_OnPlanComplete,
    FCk_Handle_Goap_Planner,
    TArray<FCk_Handle_Goap_Action>,
    float);

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanFailed,
    FCk_Delegate_Goap_OnPlanFailed,
    FCk_Handle_Goap_Planner);

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlannerActivated,
    FCk_Delegate_Goap_OnPlannerActivated,
    FCk_Handle_Goap_Planner);

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlannerDeactivated,
    FCk_Delegate_Goap_OnPlannerDeactivated,
    FCk_Handle_Goap_Planner);
```

`OnPlannerActivated` fires when a sub-Planner becomes active because its parent's Plan[0] picked it. `OnPlannerDeactivated` fires when the parent's Plan[0] changes away or the parent itself deactivates. The runner subscribes to `OnPlanComplete` on the deepest active Planner to read the leaf plan.

---

## 4. Per-Planner processor flow

All processors run per-Planner (per entity with `FFragment_Goap_RecordOfActions`). They are in `FGroup_Gameplay_AI` and ordered:

```
Setup → AutoReplan → HandleRequests → AStar_Execute → HandleResult → UpdateActivation
```

### 4.1 The six processors

1. **`FProcessor_Goap_Planner_Setup`** — One-shot per Planner with `FTag_Goap_Planner_RequiresSetup`. Extracts CDOs from each child Action, builds the candidate operator set, resolves WS source against the parent chain, validates the goal against the WS key registry, populates `_InvalidGoal` and `_DependencyCycles`. Clears the tag.

2. **`FProcessor_Goap_Planner_AutoReplan`** — Per Planner. Consumes `FTag_Goap_Planner_Dirty_*` tags (set by WS-dirty hooks and child-cost-dirty hooks), enqueues `Plan` requests respecting the throttle window.

3. **`FProcessor_Goap_Planner_HandleRequests`** — Per Planner. Drains the request queue. For a `Plan` request:
   - Check parent-plan gating (U9 mechanism): if my parent Planner has `FTag_Goap_Planner_PlanInFlight` set OR my parent's `_PlanStatus` is not in a terminal state, leave the request queued and re-arm `RequiresInitialPlan` for next frame.
   - Otherwise: seed the A* search state from this Planner's children, add `FTag_Goap_Planner_PlanInFlight` to self.

4. **`TProcessor_AStar_Execute<...>`** — Per Planner with active A* state. Time-sliced search, unchanged from CkAStar.

5. **`FProcessor_Goap_Planner_HandleResult`** — Per Planner with completed A* state. Converts path edges to child-Action handle list (using each candidate's class-derived tag for lookup), populates `_PlanState`, removes `FTag_Goap_Planner_PlanInFlight`, broadcasts `OnPlanComplete` or `OnPlanFailed`.

6. **`FProcessor_Goap_Planner_UpdateActivation`** (replaces ChainUpdate) — Per Planner. Looks at this Planner's previous Plan[0] (cached from last tick) and current Plan[0]:
   - If the previous Plan[0] was a Planner AND it is no longer Plan[0] (either because the plan is empty, or because Plan[0] changed to a different child) → deactivate the old sub-Planner: clear its planning state, drop its `FTag_Goap_Planner_RequiresInitialPlan` if pending, broadcast `OnPlannerDeactivated`.
   - If the new Plan[0] is itself a Planner AND it is not yet active → activate it: add `FTag_Goap_Planner_RequiresInitialPlan`, resolve its WS source, broadcast `OnPlannerActivated`.
   - Cache the new Plan[0] for next tick's comparison.

   This is per-Planner state mutation; there is no top-down "chain walk" anymore. The active chain emerges naturally from the recursion.

### 4.2 The activation rule — exact pseudocode

```cpp
// FProcessor_Goap_Planner_UpdateActivation::ForEachEntity
// (runs per Planner each frame, in FGroup_Gameplay_AI after HandleResult)

if (InPlanner.Get<FFragment_Goap_Planner_EnableToggle>()._Toggle == Disable) return;

auto& PlanState = InPlanner.Get<FFragment_Goap_Planner_PlanState>();
if (PlanState._PlanStatus != ECk_GoapPlanStatus::PlanFound &&
    PlanState._PlanStatus != ECk_GoapPlanStatus::PlanFailed)
{
    return;   // mid-decision; don't churn activation
}

auto& Activation = InPlanner.Get<FFragment_Goap_Planner_Activation>();
auto OldStep0 = Activation._LastActivatedPlan0;
auto NewStep0 = PlanState._Plan.IsEmpty()
    ? FCk_Handle_Goap_Action{}
    : PlanState._Plan[0];

// Deactivate the old Step0 if it changed.
if (ck::IsValid(OldStep0) && OldStep0 != NewStep0)
{
    auto OldStep0AsPlanner = UCk_Utils_Goap_Planner_UE::Cast(OldStep0);
    if (ck::IsValid(OldStep0AsPlanner))
    {
        DoDeactivatePlanner(OldStep0AsPlanner);   // clear plan state, broadcast
    }
}

// Activate the new Step0 if it's a Planner and isn't already active.
if (ck::IsValid(NewStep0))
{
    auto NewStep0AsPlanner = UCk_Utils_Goap_Planner_UE::Cast(NewStep0);
    if (ck::IsValid(NewStep0AsPlanner))
    {
        auto& ChildActivation = NewStep0AsPlanner.Get<FFragment_Goap_Planner_Activation>();
        if (!ChildActivation._IsActive)
        {
            DoActivatePlanner(NewStep0AsPlanner, InPlanner);  // resolve WS, set tag, broadcast
            ChildActivation._IsActive = true;
        }
    }
}

Activation._LastActivatedPlan0 = NewStep0;
```

The recursion happens implicitly: a deeper Planner's UpdateActivation processor runs the same logic on its own Plan[0], activating/deactivating further down. The walk is one tier per frame from the top, just like the old chain extension was — but now it falls out of each Planner's own UpdateActivation rather than being a single top-down walk on the ActionSet.

### 4.3 Multi-step plans at every tier — explicit

The plan at each Planner is a multi-Action sequence (classical GOAP). `Plan[0]` is the "current step" — the only one that drives sub-Planner activation. As `Plan[0]` completes (runner-side: gameplay observes the action's effects and flips WS), the Planner replans on WS-dirty; the new plan typically has the next step at index 0, and the activation moves to that step's sub-Planner.

There is no special framework support for "step through the plan"; it falls out of:

- Plan is multi-step (Planner does that as part of normal A*).
- WS reflects reality (runner updates it as actions complete).
- Replan on WS-dirty (already wired).
- Activation reads Plan[0] (per the rule above).

---

## 5. WorldState resolution & dirty/replan

### 5.1 Resolution chain

For each Planner at activation time:

```
_WorldStateSource_Resolved =
    _WorldStateSource_Override (if set on Planner params or via Request_SetWorldStateSource)
    ELSE parent Planner's _WorldStateSource_Resolved (if this entity has the Action role with a valid ParentPlanner ref)
    ELSE error — top-level Planners MUST provide a WS source in their params.
```

The "ActionSet's default WS source" from the old spec is gone. Top-level Planners specify WS explicitly. Promoted Planners inherit from parent unless explicitly overridden.

### 5.2 Subscription plumbing

Unchanged in shape. A Planner subscribes to its resolved WS at activation time. Only WS keys referenced by this Planner's children's preconditions/effects + this Planner's goal trigger replans. The WS holds a `TArray<FCk_Handle>` subscriber list (typed as the generic handle, since both Planner and Action role handles cast to it cleanly).

### 5.3 Replan policy semantics

Same per-Planner: `Explicit` / `OnWorldStateDirty` / `OnCostDirty` / `OnEitherDirty`. Throttle (`_MinReplanIntervalSeconds`) coalesces multiple dirty events. Same shape as today; per-Planner instead of per-Action.

### 5.4 Override stack

A WS entity carries a stack of named override layers (`FFragment_Goap_WorldState_OverrideStack`). Reads walk the stack top-down, falling through to the base store. Writes via `Set_Value` always mutate the base. Push/pop fire dirty signals only for keys whose effective value changed. A* snapshots the flattened view at seed time so the inner loop stays single-indirection.

Layers are named. The same name can be pushed multiple times — re-push replaces the layer's contents idempotently (same effective view = no dirty fire). The debugger UI uses a fixed layer named `"DebugUI"`; AI deliberation code typically uses anonymous ad-hoc names.

Override layers reset on PIE start. Persisting them across sessions is out of scope.

---

## 6. Lifecycle invariants

### 6.1 Planner state machine

```
                    ┌──────────┐
                    │ Inactive │  ← exists in catalog but no parent Plan[0] picks it
                    └─────┬────┘
                          │ parent's Plan[0] == me → DoActivatePlanner
                          ▼
                    ┌──────────┐
                ┌──→│  Active  │ ← _PlanStatus cycles: Idle → Planning → PlanFound/Failed
                │   │ Planning │
                │   └─────┬────┘
                │         │ parent's Plan[0] != me → DoDeactivatePlanner
                │         ▼
                │   ┌──────────┐
                │   │ Inactive │
                └───┴──────────┘
                          │ entity destroyed
                          ▼
                    ┌──────────┐
                    │ Destroyed│
                    └──────────┘
```

Top-level Planners are always Active (no parent above to deactivate them). They idle their `_PlanStatus` when nothing dirties their goal.

### 6.2 Owner-cascade destroy

```
Owner destroyed
  → top-level Planner destroyed (if it was on the owner)
    → all child Actions destroyed (via RecordOfActions)
      → recursively any sub-Planners destroyed
        → A* state, subscribers, signals all cleaned up
```

Cascade is via the standard ECS entity-record mechanism.

### 6.3 Tree invariants

- Each Action has at most one `_ParentPlanner` ref (single-parent tree, same as ActionSetUnification v1).
- A Planner's `RecordOfActions` is the source of truth for its children.
- An Action class appears at most once as a direct child of a given Planner (catalog uniqueness within one Planner's children).

---

## 7. Diagnostics

### 7.1 Authoring-time validations

| Check | Trigger | Outcome |
|---|---|---|
| Action-class duplicate within a Planner's children | Second `AddAction` with the same class on the same Planner | Returns the existing handle, logs `ck::goap::Verbose` |
| Top-level Planner without WS | `Add` with empty `_WorldStateSource` | Returns invalid handle, logs `ck::goap::Warning` |
| Planner with children but no goal | Setup time | Logs `ck::goap::Verbose` hint; Planner stays Idle until `Request_SetGoal` |
| Action's effects reference unregistered WS keys | Setup time after WS keys resolved | `_InvalidGoal` populated on the Planner that contains this Action as a child; `ck::goap::Verbose` |
| Dependency cycle in a Planner's child graph | Setup time, Tarjan SCC | Recorded in `_DependencyCycles` on this Planner |
| Promoting an Action that has no parent | `PromoteActionToPlanner` on a top-level entity | Logs `ck::goap::Warning` — top-level should use `Add(Owner, ...)` directly |

### 7.2 Per-Planner cycles

`_DependencyCycles` is per-Planner. Each Planner runs Tarjan SCC over its own direct children's preconditions/effects graph at Setup. Cycles are typically only meaningful at the leaf-tier (where multiple atomic actions might reference each other's effects); at higher tiers, the "candidate operators" rarely form cycles because they're meant to be alternative high-level decisions.

---

## 8. Migration from ActionSetUnification

This refactor reshapes the ActionSetUnification (U0-U10) work. Phases U0-U10 are committed; we do NOT revert them. Instead, U11 is a new phase pass that:

1. Renames `ActionSet` → `Planner` at the type level (handle, fragment, util class).
2. Drops `FCk_Handle_Goap` (the root container) — collapses it into Planner.
3. Removes the "composite Action's goal = effects" rule from processors.
4. Replaces `ChainUpdate` with per-Planner `UpdateActivation`.
5. Replaces `_InitialGoal_RootOnly` with `_Goal` on PlannerParams (every Planner has its own goal, settable independently).
6. Rewires the 19 AutoTests + 3 gyms + debugger against the new types.

Since nothing has been pushed and no downstream consumer depends on the ActionSetUnification API yet, this is a contained migration. The U7-U10 test surface is preserved as the verification harness for U11.

### 8.1 API mapping table — full

| ActionSetUnification API | PlannerActionCollapse equivalent |
|---|---|
| `FCk_Handle_Goap` | (removed) |
| `FCk_Handle_Goap_ActionSet` | `FCk_Handle_Goap_Planner` |
| `FCk_Handle_Goap_Action` | `FCk_Handle_Goap_Action` (unchanged name) |
| `utils_goap::Add(Owner, RootParams)` | (removed — start with `utils_goap_planner::Add`) |
| `utils_goap::Has(Handle)` | `utils_goap_planner::Has(Handle)` |
| `utils_goap_action_set::AddActionSet(Goap, Params)` | `utils_goap_planner::Add(Owner, PlannerParams)` |
| `utils_goap_action_set::SetRootAction(ActionSet, RootParams, WS)` | (removed — goal + WS go directly in PlannerParams) |
| `utils_goap_action_set::AddAction_ToActionSet(ActionSet, Params)` | `utils_goap_planner::AddAction(Planner, Params)` |
| `utils_goap_action::AddAction_ToAction(ParentAction, Params)` | `utils_goap_planner::AddAction(PromotedActionAsPlanner, Params)` |
| `utils_goap_action_set::Find_Action(ActionSet, Tag)` | `utils_goap_planner::Find_ChildAction(Planner, Tag)` |
| `utils_goap_action_set::Find_ActionByClass(...)` | `utils_goap_planner::Find_ChildActionByClass(...)` |
| `utils_goap_action_set::Get_ActiveChain(ActionSet)` | `utils_goap_planner::Get_ActiveChain(Planner)` |
| `utils_goap_action_set::Get_RootAction(ActionSet)` | (removed — there is no root) |
| `utils_goap_action_set::Request_SetEnableToggle(ActionSet, ...)` | `utils_goap_planner::Request_SetEnableToggle(Planner, ...)` |
| `utils_goap_action_set::Request_ResetActiveChain(ActionSet)` | `utils_goap_planner::Request_DeactivateChildren(Planner)` |
| `utils_goap_action_set::Request_SetRootAction(ActionSet, ...)` | (removed) |
| `utils_goap_action::Get_PlanStatus(Action)` | `utils_goap_planner::Get_PlanStatus(Planner)` — only Planners have plans |
| `utils_goap_action::Get_Plan(Action)` | `utils_goap_planner::Get_Plan(Planner)` |
| `utils_goap_action::Get_PlanCost(Action)` | `utils_goap_planner::Get_PlanCost(Planner)` |
| `utils_goap_action::Get_WorldStateSource(Action)` | `utils_goap_planner::Get_WorldStateSource(Planner)` |
| `utils_goap_action::Get_ActiveParentAction(Action)` | `utils_goap_action::Get_ParentPlanner(Action)` |
| `utils_goap_action::Get_InvalidGoal(Action)` | `utils_goap_planner::Get_InvalidGoal(Planner)` |
| `utils_goap_action::Request_Plan(Action)` | `utils_goap_planner::Request_Plan(Planner)` |
| `utils_goap_action::Request_SetActionCost(Action, ClassRef, Cost)` | `utils_goap_planner::Request_SetChildActionCost(Planner, ClassRef, Cost)` |
| `_InitialGoal_RootOnly` (on ActionParamsData) | `_Goal` (on PlannerParamsData) |
| `OnPlanComplete` (per-Action signal) | `OnPlanComplete` (per-Planner signal) |
| `OnActionActivated` / `OnActionDeactivated` | `OnPlannerActivated` / `OnPlannerDeactivated` |
| `OnActiveChainChanged` (per-ActionSet) | `OnPlannerPlanChanged` (per-Planner — fires when this Planner's plan changes, replaces the per-ActionSet chain signal) |

---

## 9. Test surface

The 19+5 ActionSetUnification tests (U7+U10) are the verification harness for U11. Each test is rewritten against the Planner/Action API but tests the same behavioral property. Examples:

| Old test | New test | What it validates |
|---|---|---|
| `Goap_ActionSet_RootOnly` | `Goap_Planner_MinimalPlan` | Top-level Planner with one child, plan resolves |
| `Goap_ActionSet_AtomicLeaf` | `Goap_Planner_AtomicLeaf` | Plan[0] is an atomic Action (no Planner role); no sub-activation |
| `Goap_ActionSet_ChainGrowth` | `Goap_Planner_NestedActivation` | Plan[0] is a Planner; sub-Planner gets RequiresInitialPlan + OnPlannerActivated fires |
| `Goap_ActionSet_ChainTruncation` | `Goap_Planner_DeactivateOnStep0Change` | Parent Plan[0] flips to a different child; old sub-Planner gets deactivated + OnPlannerDeactivated fires |
| `Goap_ActionSet_GoalIsEffects` | `Goap_Planner_IndependentGoals` | Sub-Planner's goal can be different from its action-role effects (key validation of U11.5) |
| `Goap_ActionSet_MultiActionSet` | `Goap_Planner_TwoPeerPlanners` | Two top-level Planners on one entity, plan independently |
| `Goap_ActionSet_SwapRootAction` | (removed — no root concept) |
| `Goap_ActionSet_SiblingActions` | (removed — no root concept; replaced by `Goap_Planner_AddRemoveChildren`) |
| `Goap_ActionSet_OnActiveChainChangedSignal` | `Goap_Planner_OnPlanChangedSignal` |
| `Goap_ActionSet_CancelInflightPlan` | `Goap_Planner_CancelInflight` |
| `Goap_ActionSet_DependencyCycleDetection` | `Goap_Planner_DependencyCycleDetection` (per-Planner) |
| `Goap_ActionSet_Toggle` | `Goap_Planner_EnableToggle` |
| `Goap_ActionSet_ResetChain` | `Goap_Planner_DeactivateChildren` |
| `Goap_ActionSet_DirtyPropagation` | `Goap_Planner_DirtyPropagation` |
| `Goap_ActionSet_DeferOneFrame` | `Goap_Planner_DeferOneFrame` (U9 parent-plan gating preserved) |
| `Goap_ActionSet_WSInheritance` | `Goap_Planner_WSInheritance` |
| `Goap_ActionSet_WSOverride` | `Goap_Planner_WSOverride` |
| `Goap_ActionSet_OwnerCascadeDestroy` | `Goap_Planner_OwnerCascadeDestroy` |
| `Goap_ActionSet_InvalidGoal` | `Goap_Planner_InvalidGoal` |
| `Goap_ActionSet_MultiActionSet` | `Goap_Planner_TwoPeerPlanners` |

New tests for U11-specific behavior:

| Test | What it validates |
|---|---|
| `Goap_Planner_PromoteActionToPlanner` | Promoting an existing Action to also carry the Planner role; both casts work on the same handle |
| `Goap_Planner_IndependentGoalDoesNotEqualEffects` | Sub-Planner declares effect E, sets goal G ≠ E; planner plans toward G; parent sees effect E |
| `Goap_Planner_TopLevelEmergence` | Multiple Planners chained; identifying the top is derivable from runtime topology (no fragment declares it) |

Target: ~22 tests post-U11.

---

## 10. Out of scope (v1)

- Network sync of plans / Planner state.
- Plan persistence across save/load.
- Multi-parent Actions (each Action has at most one parent Planner — single-parent tree invariant preserved).
- Numeric / hierarchical world state.
- DataAsset-driven Planner declarations (everything is imperative).
- Per-Action enable toggle (only per-Planner toggle).

---

## 11. Open questions / risks

| Question | Disposition |
|---|---|
| Does promoting an Action to a Planner reset its action-role state (cost, effects)? | No — they're orthogonal fragment clusters. Promotion stamps Planner fragments without touching Action fragments. |
| What happens if you `AddAction` to a Planner during its A* search? | Reject (request queued, replan after A* finishes). Same shape as ActionSetUnification handled mid-search mutations. |
| Can a Planner's children be Planners that share no WS source with the parent? | Yes — Planner WS resolution falls back to the explicit override if set; if unset, inherits from parent. If you want a child Planner to read a completely different WS, set `_WorldStateSource` on its params. |
| Should `OnPlannerActivated` fire for the top-level Planner at first Setup? | Yes, once at construction. Top-level Planners are conceptually always "activating into the world" at startup. |
| What if a child Action is added to two different Planners on the same entity? | Not supported — the framework enforces single-parent (the Action's `_ParentPlanner` field is single-valued). The second registration logs a Warning and rejects. |

---

## 12. Implementation roadmap

This is a sizable refactor. Each phase produces a verified commit set; each phase's tests must pass before moving on.

### Phase U11.0 — Rename + fragment split (no semantics change)

- Rename `FCk_Handle_Goap_ActionSet` → `FCk_Handle_Goap_Planner` everywhere.
- Rename `utils_goap_action_set` → `utils_goap_planner`.
- Drop `FCk_Handle_Goap` (the root container). Collapse its functionality into the top-level Planner.
- Split `FFragment_Goap_Action_Current` into Action-role (`_Definition`) and Planner-role (`_PlanState`, `_Goal`, etc.) fragments.
- Build green, 19 tests still pass with the renamed API.

### Phase U11.1 — Goal independence

- Rename `_InitialGoal_RootOnly` → `_Goal` on PlannerParamsData.
- Add `Request_SetGoal(Planner, NewGoal)` API.
- Remove the "composite Action's goal = effects" hardwiring in processors. Every Planner's goal comes from its `FFragment_Goap_Planner_Goal`, set at construction or via Request.
- Update the spec's GoalIsEffects test to verify the independence (sub-Planner can declare effect E and goal G ≠ E).

### Phase U11.2 — UpdateActivation processor

- Replace `FProcessor_Goap_ActionSet_ChainUpdate` with `FProcessor_Goap_Planner_UpdateActivation`.
- Per-Planner: cache Plan[0] across frames, fire `OnPlannerActivated`/`OnPlannerDeactivated` on changes.
- Verify `Goap_Planner_NestedActivation` and `Goap_Planner_DeactivateOnStep0Change` tests.

### Phase U11.3 — Promotion API

- Add `utils_goap_planner::PromoteActionToPlanner(Action, PlannerParams)`.
- Verify the resulting entity casts both ways.
- Add `Goap_Planner_PromoteActionToPlanner` test.

### Phase U11.4 — Diagnostics retarget

- `_InvalidGoal` and `_DependencyCycles` move to per-Planner.
- Per-Planner cycle detection (Tarjan SCC over each Planner's children).
- Update tests.

### Phase U11.5 — Test rewrite

- All 19+5 ActionSetUnification tests rewritten against the Planner/Action API.
- All new U11-specific tests authored.
- Full Goap_Planner pattern green in both Development and DebugGame.

### Phase U11.6 — Gym rewrite

- 3 gyms rewritten to use Planner-shaped trees. The Patrol station becomes the canonical "tree of Planners" example.

### Phase U11.7 — Debugger update

- `SCkGoapDebuggerWindow` updated for Planner entity tree.
- Sidebar shows Planners (with their children unfolded). Active chain visualization is per-Planner Plan[0] chains.
- Graph pane gets tree edges (D11 work — Planner → child Action visualized as tree edge in addition to dependency edges).
- Per-tier plan strip + goal display: each Planner row in the breadcrumb is queryable.

### Phase U11.8 — Docs

- Rewrite `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` for Planner/Action model.
- Rewrite `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/CLAUDE.md` for the per-Planner debugger UI.
- Add SUPERSEDED banner to `2026-05-19-CkGoap-ActionSetUnification-design.md`.

---

## Appendix A — Glossary

| Term | Meaning |
|---|---|
| **Planner** | An ECS entity carrying `FFragment_Goap_RecordOfActions` and the planner-role fragments. Has its own goal, plan, and set of child Actions. The unit of *deciding what to do at this tier*. |
| **Action** | An ECS entity carrying `FFragment_Goap_Action_Definition` and the action-role fragments. Has preconditions, effects, cost. The unit of *being a step in some parent's plan*. |
| **Dual-role entity** | An entity carrying both Planner and Action role fragments. Both typesafe casts succeed for it. This is how a mid-tier composite is represented. |
| **Top-level Planner** | A Planner with no `_ParentPlanner` ref — nothing references it as a child Action. Emerges from runtime topology; no special fragment declares roothood. |
| **Leaf Action** | An Action that carries no Planner-role fragments. Has no children, no plan. Just a step. |
| **Active chain** | The current path of activated Planners, derived by walking Plan[0] from a top-level Planner downward through nested Planners until reaching a leaf Action or an inactive sub-Planner. Implicit; not stored as a fragment. |
| **Plan** | The output of one Planner's A* — an ordered list of child Action class identities that satisfy this Planner's goal. Stored in `_PlanState._Plan`. |
| **Plan[0]** | The first step of a Planner's plan. The only step that gates sub-Planner activation. As gameplay progresses and WS changes, the planner replans; Plan[0] becomes the next step in the queue. |
| **Sub-Planner activation** | The act of a Planner's UpdateActivation processor detecting that its Plan[0] now references an entity that also carries the Planner role; it activates that sub-Planner by setting `RequiresInitialPlan` and broadcasting `OnPlannerActivated`. |
| **Goal** | A list of WS conditions a Planner is planning toward. Per-Planner. Set at construction or via `Request_SetGoal`. Independent of effects of any Action role this same entity may carry. |
| **Resolved WS source** | The WS handle this Planner actually consumes. Override > parent's resolved > error-on-missing-for-top-level. |
| **Promotion** | The act of attaching Planner-role fragments to an existing Action entity, making it also a Planner. Explicit via `PromoteActionToPlanner`. |

---

## Appendix B — Naming changes from ActionSetUnification

| ActionSetUnification | PlannerActionCollapse |
|---|---|
| `FCk_Handle_Goap` | (removed) |
| `FCk_Handle_Goap_ActionSet` | `FCk_Handle_Goap_Planner` |
| `FCk_Handle_Goap_Action` | `FCk_Handle_Goap_Action` (unchanged) |
| `FCk_Fragment_Goap_RootParamsData` | (removed) |
| `FCk_Fragment_Goap_ActionSetParamsData` | `FCk_Fragment_Goap_PlannerParamsData` |
| `FCk_Fragment_Goap_ActionParamsData` | `FCk_Fragment_Goap_ActionParamsData` (unchanged, but loses `_InitialGoal_RootOnly`) |
| `_BundleTag` / `_ActionSetTag` | `_PlannerTag` (for the `Create` flow's named child planner case) |
| `_InitialGoal_RootOnly` | `_Goal` on PlannerParamsData |
| `_TierTag` | (gone — already removed in ActionSetUnification) |
| `_ActionTag` | class-derived identity tag on Action (unchanged) |
| `FFragment_RecordOfGoapActionSets` | (gone — Planners aren't aggregated under a separate Goap root) |
| `FFragment_RecordOfGoapActions` | `FFragment_Goap_RecordOfActions` (on Planner, holds child Actions; serves as Planner-role discriminator) |
| `FFragment_Goap_ActionSet_ActiveChain` | (gone — active chain is implicit) |
| `FFragment_Goap_ActionSet_WorldStateSource` | `FFragment_Goap_Planner_WorldStateSource` |
| `FFragment_Goap_Action_Current` | split into `FFragment_Goap_Planner_PlanState`, `FFragment_Goap_Planner_Goal`, `FFragment_Goap_Planner_WorldStateSource` |
| `UCk_Utils_Goap_UE` | (gone) |
| `UCk_Utils_Goap_ActionSet_UE` | `UCk_Utils_Goap_Planner_UE` |
| `UCk_Utils_Goap_Action_UE` | `UCk_Utils_Goap_Action_UE` (unchanged) |
| `OnActionActivated` / `OnActionDeactivated` | `OnPlannerActivated` / `OnPlannerDeactivated` |
| `OnActiveChainChanged` | `OnPlannerPlanChanged` (per-Planner; fires when this Planner's plan changes) |
| `Request_ResetActiveChain` | `Request_DeactivateChildren` |
| `Request_SetRootAction` | (removed) |
| `Request_SetActionCost(Action, ChildClass, Cost)` | `Request_SetChildActionCost(Planner, ChildClass, Cost)` |
| `Get_RootAction` | (removed) |
| `Get_ActiveParentAction(Action) → SubclassOf` | `Get_ParentPlanner(Action) → FCk_Handle_Goap_Planner` |

---

*End of spec.*
