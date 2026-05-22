> ⚠️ **SUPERSEDED** — This spec described the U0–U10 ActionSetUnification model, which has been replaced by the U11 Planner/Action Collapse model. The U11 spec is at
> [`2026-05-21-CkGoap-PlannerActionCollapse-design.md`](2026-05-21-CkGoap-PlannerActionCollapse-design.md)
> (with summary [`2026-05-21-CkGoap-PlannerActionCollapse-summary.md`](2026-05-21-CkGoap-PlannerActionCollapse-summary.md)).
> Historical reference only — do not implement against this spec.

# CkGoap ActionSet / Action Unification — Design Spec

**Date:** 2026-05-19
**Status:** Approved (design phase)
**Author:** Claude (continuation of brainstorm with @Sulfur-CK)
**Supersedes:** `2026-05-19-CkGoap-BundleTierRefactor-design.md` — that spec's *Bundle / Tier / Action* trichotomy collapses into the *ActionSet / Action* dichotomy described here.
**Implementation status of superseded spec:** Phases 0–6 of the Bundle/Tier refactor are already committed and green. This unification reorganises those fragments; parts of Phases 1, 3, 4, 6 will be redone.
**Companion spec (next session):** Debugger redesign — paused mid-brainstorm. Resumes after this unification lands, with terminology updated.

---

## Quick index

1. [Rationale — why unify](#1-rationale--why-unify)
2. [Data model — entities, fragments, handles](#2-data-model--entities-fragments-handles)
3. [API surface](#3-api-surface)
4. [Per-frame processor flow](#4-per-frame-processor-flow)
5. [WorldState injection & dirty/replan plumbing](#5-worldstate-injection--dirtyreplan-plumbing)
6. [Lifecycle invariants](#6-lifecycle-invariants)
7. [Diagnostics](#7-diagnostics)
8. [Migration story (from Bundle/Tier)](#8-migration-story-from-bundletier)
9. [Test surface](#9-test-surface)
10. [Out of scope (v1)](#10-out-of-scope-v1)
11. [Open questions / risks](#11-open-questions--risks)
12. [Implementation roadmap](#12-implementation-roadmap)
13. [Appendix A — Glossary](#appendix-a--glossary)
14. [Appendix B — Naming changes from Bundle/Tier refactor](#appendix-b--naming-changes-from-bundletier-refactor)

---

## 1. Rationale — why unify

The Bundle/Tier refactor introduced three concepts: **Bundle** (top-level decision domain), **Tier** (decision level inside a bundle), **Action** (a planner step). At runtime, a Tier and an Action share more than they differ:

- Both have an identity (the Tier's `_TierTag`, the Action's class-derived tag).
- Both can be the parent of further planning — a Tier's `Plan[0]` selects a child Action, whose `_ActionTag` then activates a child Tier.
- Both have declared *effects* — a Tier's `_Goal` (its desired effects) is literally the `_Effects` of the parent Action that activated it.

In other words, **a Tier is a composite Action — an Action whose planner expands into a sub-plan**. The distinction in the previous refactor was an artifact of preserving the tuq reference's vocabulary, not a load-bearing piece of the model.

Unification produces a simpler model:

- One first-class concept: **Action**.
- A grouping container: **ActionSet** (renamed from Bundle).
- Composite vs atomic is determined by whether the Action has children registered under it (known at registration time, read by the chain-extension processor each frame).
- Chain extension is direct: `Plan[0]` IS the next active Action — no tag matching layer.

Authoring becomes:

```cpp
// Before (Bundle/Tier):
AddBundle(Goap, BundleParams);
AddTier(Bundle, TierParams);                     // implicit root if first
AddTier(Bundle, OtherTierParams);
AddAction(RootTier, MyActionClass);              // belongs to root tier's planner
AddAction(OtherTier, MyOtherActionClass);
// Each Action manually calls SetActionTag(...) inside DefineAction
// pointing at the _TierTag of the tier it delegates to.

// After (ActionSet / Action):
AddActionSet(Goap, ActionSetParams);
SetRootAction(ActionSet, RootActionClass, InitialWS);
AddAction(Root, ChildActionClass);               // child Action — Root's planner can pick it
AddAction(Root, OtherChildActionClass);
AddAction(ChildHandle, GrandchildClass);         // arbitrary depth
// No SetActionTag — identity is class-derived; chain extension is implicit
// (a chosen child Action with its own children becomes the next chain link).
```

---

## 2. Data model — entities, fragments, handles

Two typesafe handles: `FCk_Handle_Goap_ActionSet`, `FCk_Handle_Goap_Action`. The existing `FCk_Handle_Goap` is unchanged — still "the Goap root container."

### 2.1 Entity hierarchy

```
                              FCk_Handle              (owner — NPC, ACk_Pawn, etc.)
                                  │ owner-chain
                                  ▼
                           ┌──────────────────────┐
                           │  FCk_Handle_Goap     │  ← root container (one per entity)
                           │  · RecordOfActionSets│
                           └──────────────────────┘
                                  │ record
                                  ▼
              ┌───────────────────┴───────────────────┐
              ▼                                       ▼
   ┌──────────────────────┐                ┌──────────────────────┐
   │ FCk_Handle_Goap_     │                │ FCk_Handle_Goap_     │
   │     ActionSet        │                │     ActionSet        │
   │  · ActionSetTag      │                │  · ActionSetTag      │
   │  · RecordOfActions   │                │  · RecordOfActions   │
   │    (catalog, all)    │                │    (catalog, all)    │
   │  · RootAction        │                │  · RootAction        │
   │  · ActiveChain       │                │  · ActiveChain       │
   │    (ordered)         │                │    (ordered)         │
   │  · EnableToggle      │                │  · EnableToggle      │
   │  · WorldStateSource  │                │  · WorldStateSource  │
   └──────────────────────┘                └──────────────────────┘
              │ record (catalog — all registered Actions, regardless of tree position)
              ▼
   ┌────────────────────────────────────────┐
   │  FCk_Handle_Goap_Action                │  ← one entity per registered action
   │  · ActionTag (class-derived, debug ID) │
   │  · ParentAction (handle; invalid for   │
   │       root and for siblings-of-root)   │
   │  · ChildActions (TArray<Action>)       │
   │  · ActionClass (TSubclassOf)           │
   │  · ActionDef (preconditions, effects,  │
   │       cost — CDO-extracted)            │
   │  · _WorldStateSource_Override (opt)    │
   │  · _WorldStateSource_Resolved          │
   │  · _Goal (= _Effects when active)      │
   │  · _Plan (TArray<FCk_Handle_Goap_Action│
   │       — child handles, in plan order)  │
   │  · _PlanCost / _PlanStatus             │
   │  · _PlanAttemptCount                   │
   │  · _ActiveParent (the parent that      │
   │       currently has this Action in its │
   │       active sub-plan, or invalid)     │
   │  · ReplanThrottle                      │
   │  · A* SearchState/Result/PlanContext   │
   └────────────────────────────────────────┘
              │ uses
              ▼
   ┌────────────────────────────────────────┐
   │  FCk_Handle_Goap_WorldState  (existing)│  ← one per WS context; default-shared
   │  · Values / KeyRegistry                │    across an ActionSet's active chain
   │  · Subscribers                         │
   │  · Dirty tag                           │
   └────────────────────────────────────────┘
```

### 2.2 Fragment table

| Fragment | Lives on | Purpose |
|---|---|---|
| `FFragment_Goap_Root_Params` | Goap root | Reserved for future global tuning (no required fields in v1) |
| `FFragment_RecordOfGoapActionSets` | Goap root | Record of ActionSet entities |
| `FFragment_Goap_ActionSet_Params` | ActionSet | `ActionSetTag`, initial enable toggle |
| `FFragment_Goap_ActionSet_Current` | ActionSet | Current enable state, `_RootAction` handle, `_DependencyCycles` diagnostic |
| `FFragment_RecordOfGoapActions` | ActionSet | Record of Action entities (the **catalog** — all Actions in this ActionSet, regardless of tree depth) |
| `FFragment_Goap_ActionSet_ActiveChain` | ActionSet | `TArray<FCk_Handle_Goap_Action>` — ordered active chain, `[0]` = root |
| `FFragment_Goap_ActionSet_WorldStateSource` | ActionSet | Default WS handle for the ActionSet (inherited by Actions unless overridden) |
| `FFragment_Goap_Action_Params` | Action | `ActionClass` (TSubclassOf), class-derived `ActionTag`, declared `_WorldStateSource_Override`, planning-frequency, replan-policy, search budget, cost threshold, plan-on-start |
| `FFragment_Goap_Action_Definition` | Action | CDO-extracted action def: `Preconditions`, `Effects`, `Cost` |
| `FFragment_Goap_Action_Tree` | Action | `_ParentAction` handle (invalid for actionset-level Actions), `_ChildActions: TArray<FCk_Handle_Goap_Action>` |
| `FFragment_Goap_Action_Current` | Action | `_WorldStateSource_Resolved`, `_Goal`, `_InvalidGoal`, `_Plan` (ordered child handles), `_PlanCost`, `_PlanStatus`, `_PlanAttemptCount`, `_ActiveParent` |
| `FFragment_Goap_Action_Requests` | Action | std::variant queue of per-Action requests |
| `FFragment_Goap_Action_ReplanThrottle` | Action | Per-Action throttle accumulator |
| `FFragment_Goap_Action_SearchState` / `_Result` / `_PlanContext` | Action | A* fragments (per-Action — only meaningful when Action is active) |
| `FFragment_AStar_Params` | Action | The underlying CkAStar config (search budget, cost threshold) |
| `FTag_Goap_ActionSet_RequiresChainUpdate` | ActionSet | Set after any Action in the set plans; consumed by chain-update processor |
| `FTag_Goap_Action_RequiresSetup` | Action | One-shot setup gate |
| `FTag_Goap_Action_RequiresInitialPlan` | Action | Drives first plan after activation |
| `FTag_Goap_Action_PlanRequested` | Action | Request-flow gate |
| `FTag_Goap_Dirty_WorldState` / `FTag_Goap_Dirty_Cost` | Action | Per-Action dirty tracking |

**Why a flat catalog instead of a tree-only record:** the ActionSet keeps a flat record of every registered Action (regardless of tree position) so:

- Setup-time CDO scanning iterates each Action once.
- Cross-tree diagnostic walks (cycle detection, "is this action class registered anywhere") are O(catalog).
- The hierarchy lives in `FFragment_Goap_Action_Tree` per-Action — that gives the parent / children navigation needed at runtime.

### 2.3 No more `_ActionTag` matching layer

Today's `_ActionTag` (and `SetActionTag` builder) is **gone**. The action's class-derived identity tag survives only as a debug ID — it's not used for chain matching. Chain extension reads `Plan[0]` (which is already a child Action handle) directly.

### 2.4 ActionSet-level enable/disable, per-Action overrides

`EnableToggle` lives on the ActionSet (matches today's bundle semantics). Per-Action toggles are not in v1; if you need to suppress a sub-tree, override the parent's planner outputs or remove the Action from registration.

---

## 3. API surface

The API is **imperative all the way down**. No DataAssets declaring ActionSet/Action structure.

### 3.1 Construction verbs

```cpp
// Root container — one per entity. Unchanged from Bundle/Tier.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add")
static FCk_Handle_Goap Add(
    UPARAM(ref) FCk_Handle& InOwner,
    const FCk_Fragment_Goap_RootParamsData& InParams);

// ActionSet — top-level decision domain (was: AddBundle).
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add ActionSet")
static FCk_Handle_Goap_ActionSet AddActionSet(
    UPARAM(ref) FCk_Handle_Goap& InGoap,
    const FCk_Fragment_Goap_ActionSetParamsData& InParams);

// Designate the entry-point Action of an ActionSet. Must be called once per
// ActionSet before any planning can happen. The InitialWorldState becomes
// the ActionSet's _WorldStateSource. The chosen ActionClass is created as
// an Action entity, marked as the root, and added to the catalog.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Set Root Action")
static FCk_Handle_Goap_Action SetRootAction(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InRootActionClass,
    UPARAM(ref) FCk_Handle_Goap_WorldState& InInitialWorldState);

// Add an Action at the top level of an ActionSet (a sibling of the root,
// available as a candidate for *future* roots after Request_SetRootAction).
// In v1, additional top-level Actions besides the root are uncommon — most
// authoring puts everything under the root's tree.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add Action (To ActionSet)")
static FCk_Handle_Goap_Action AddAction_ToActionSet(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);

// Add an Action as a child of another Action. The parent's planner can now
// pick this child when planning. Arbitrary depth is supported.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add Action (To Action)")
static FCk_Handle_Goap_Action AddAction_ToAction(
    UPARAM(ref) FCk_Handle_Goap_Action& InParentAction,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);
```

Both `AddAction_*` verbs return the new Action's handle. In C++ both are accessible via overloaded `AddAction(...)`; BP/AS see two distinct entries that share the "Add Action" display verb. The user-visible distinction is "(To ActionSet)" vs "(To Action)".

### 3.2 Param structs

```cpp
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_RootParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_RootParamsData);
    // Reserved for future global tuning. Empty in v1.
};

USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_ActionSetParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_ActionSetParamsData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories = "Goap.ActionSet"))
    FGameplayTag _ActionSetTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _InitialToggle = ECk_EnableDisable::Enable;

public:
    CK_PROPERTY_GET(_ActionSetTag);
    CK_PROPERTY(_InitialToggle);
    CK_DEFINE_CONSTRUCTORS(FCk_Fragment_Goap_ActionSetParamsData, _ActionSetTag);
};

// Per-Action params are no longer set via a separate AddAction(InParams) — the
// Action's authoring lives entirely on its UCk_GoapAction_EntityScript subclass
// (DefineAction sets preconditions/effects/cost). Optional runtime tuning uses
// Request_* verbs after creation.
```

Per-Action tuning (search budget, cost threshold, replan policy, WS override) lives in `FFragment_Goap_Action_Params` and is set via `Request_*` verbs at runtime — defaults come from the action's CDO if the author defines them.

### 3.3 Runtime mutation verbs (per-Action targeted)

```cpp
// Force a fresh plan on this Action.
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_Plan(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_CancelPlan(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction);

// Adjust a child Action's cost (read by InAction's planner).
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_SetChildActionCost(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction,
    TSubclassOf<UCk_GoapAction_EntityScript> InChildClass,
    float InCost);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_SetReplanInterval(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction, float InSeconds);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_SetReplanPolicy(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction, ECk_Goap_ReplanPolicy InPolicy);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_SetSearchBudget(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction, float InMicroseconds);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_SetCostThreshold(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction, float InThreshold);

// Override the WS source this Action plans against. Defaults to inherited from
// parent (or ActionSet's source for top-level Actions).
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_SetWorldStateSource(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction,
    UPARAM(ref) FCk_Handle_Goap_WorldState& InWorldStateSource);
```

### 3.4 ActionSet-level verbs

```cpp
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_ActionSet Request_SetEnableToggle(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet,
    ECk_EnableDisable InToggle);

// Reset the active chain back to just the root. Useful after a hard world-state
// change or scripted teleport.
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_ActionSet Request_ResetActiveChain(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet);

// Swap the root Action of an ActionSet at runtime. Truncates the active chain,
// destroys the old root's planner state, designates InNewRootClass as the new
// root (creating the entity if not yet in the catalog).
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_ActionSet Request_SetRootAction(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InNewRootClass);
```

### 3.5 Query verbs

```cpp
// Goap root.
static bool Has(const FCk_Handle& InHandle);

// ActionSet lookup.
static FCk_Handle_Goap_ActionSet Find_ActionSet(
    const FCk_Handle_Goap& InGoap, FGameplayTag InActionSetTag);

// Action lookup (within an ActionSet's catalog — by class-derived tag).
static FCk_Handle_Goap_Action Find_Action(
    const FCk_Handle_Goap_ActionSet& InActionSet, FGameplayTag InActionTag);

static FCk_Handle_Goap_Action Find_ActionByClass(
    const FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InClass);

// Active chain — ordered, root at [0].
static TArray<FCk_Handle_Goap_Action> Get_ActiveChain(
    const FCk_Handle_Goap_ActionSet& InActionSet);

// Per-Action plan state.
static ECk_GoapPlanStatus Get_PlanStatus(const FCk_Handle_Goap_Action& InAction);
static TArray<FCk_Handle_Goap_Action> Get_Plan(
    const FCk_Handle_Goap_Action& InAction);   // ← Plan elements ARE Action handles now
static float Get_PlanCost(const FCk_Handle_Goap_Action& InAction);

// Per-Action navigation.
static FCk_Handle_Goap_WorldState Get_WorldStateSource(
    const FCk_Handle_Goap_Action& InAction);
static FCk_Handle_Goap_Action Get_ParentAction(
    const FCk_Handle_Goap_Action& InAction);   // invalid handle for root
static FCk_Handle_Goap_Action Get_ActiveParent(
    const FCk_Handle_Goap_Action& InAction);   // currently-activating parent
static TArray<FCk_Handle_Goap_Action> Get_ChildActions(
    const FCk_Handle_Goap_Action& InAction);

// Diagnostics.
static TArray<FCk_GoapWS_Condition_Authored> Get_InvalidGoal(
    const FCk_Handle_Goap_Action& InAction);
static TArray<FString> Get_DependencyCycles(
    const FCk_Handle_Goap_ActionSet& InActionSet);
```

### 3.6 Signals

```cpp
// Per-Action — fires when an Action's plan completes / fails.
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanComplete,
    FCk_Delegate_Goap_OnPlanComplete,
    FCk_Handle_Goap_Action,                            // Source: action
    TArray<FCk_Handle_Goap_Action>,                    // Chosen child actions, in plan order
    float);                                            // Plan cost

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanFailed,
    FCk_Delegate_Goap_OnPlanFailed,
    FCk_Handle_Goap_Action);

// ActionSet-level — fires whenever the active chain mutates.
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnActiveChainChanged,
    FCk_Delegate_Goap_OnActiveChainChanged,
    FCk_Handle_Goap_ActionSet,                         // Source
    TArray<FCk_Handle_Goap_Action>);                   // Old chain
    // (New chain readable via Get_ActiveChain in the handler.)

// Action-level — fires once when an Action enters / exits the active chain.
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnActionActivated,
    FCk_Delegate_Goap_OnActionActivated,
    FCk_Handle_Goap_Action);

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnActionDeactivated,
    FCk_Delegate_Goap_OnActionDeactivated,
    FCk_Handle_Goap_Action);
```

`OnActionActivated` is the right hook for "start running this Action" code (kick off animation, claim resources). `OnActionDeactivated` is the cleanup pair. `OnPlanComplete` on the leaf Action (the chain tip whose plan is empty or contains only atomic children) is what an action-runner consumer subscribes to.

---

## 4. Per-frame processor flow

Six processors, all in `FGroup_Gameplay_AI`:

1. `FProcessor_Goap_Action_Setup` — extract CDOs into action defs, register WS keys
2. `FProcessor_Goap_Action_AutoReplan` — consume per-Action dirty tags, enqueue Plan requests
3. `FProcessor_Goap_Action_HandleRequests` — drain per-Action request queue, build A* graph
4. `TProcessor_AStar_Execute<...>` — time-sliced A* (reused from CkAStar, unchanged)
5. `FProcessor_Goap_Action_HandleResult` — convert A* path to child-Action sequence, fire `OnPlanComplete`
6. `FProcessor_Goap_ActionSet_ChainUpdate` ⭐ — walk each ActionSet's ActiveChain, apply truncate/extend rule

### 4.1 Ordering and rationale

Order within `FGroup_Gameplay_AI`:

```
Setup → AutoReplan → HandleRequests → AStar_Execute → HandleResult → ChainUpdate
```

ChainUpdate runs LAST so every Action in the chain has fresh `Plan[0]` before chain mutation decisions.

### 4.2 The chain-update rule — exact pseudocode

The matching layer is gone; `Plan[0]` IS the next child handle directly. The truncate/extend rule reduces to:

```cpp
// FProcessor_Goap_ActionSet_ChainUpdate::ForEachEntity (per ActionSet, each frame)

if (ActionSetEnableToggle == Disable) return;

auto& ActiveChain = InActionSet.Get<FFragment_Goap_ActionSet_ActiveChain>()._Chain;
auto OldChainSnapshot = ActiveChain;
auto bChainChanged = false;

auto i = 0;
while (i < ActiveChain.Num())
{
    const auto& CurrAction = ActiveChain[i];
    const auto& CurrCurrent = CurrAction.Get<FFragment_Goap_Action_Current>();

    if (CurrCurrent._PlanStatus != ECk_GoapPlanStatus::PlanFound)
    {
        return;   // mid-decision, don't mutate past this point
    }

    const auto& Plan = CurrCurrent._Plan;
    if (Plan.IsEmpty())
    {
        // Goal already satisfied / no actions chosen — truncate everything past.
        if (i + 1 < ActiveChain.Num())
        {
            TruncateChainFrom(InActionSet, ActiveChain, i + 1);
            bChainChanged = true;
        }
        break;
    }

    const auto NextChild = Plan[0];   // already a handle to a child Action

    if (i + 1 < ActiveChain.Num())
    {
        const auto& ExistingChild = ActiveChain[i + 1];
        if (ExistingChild == NextChild)
        {
            ++i;
            continue;
        }

        // Mismatch — truncate, then fall through to append the new choice.
        TruncateChainFrom(InActionSet, ActiveChain, i + 1);
        bChainChanged = true;
    }

    // Is the chosen child a composite Action (has children of its own)?
    const auto& ChildTree = NextChild.Get<FFragment_Goap_Action_Tree>();
    if (ChildTree._ChildActions.IsEmpty())
    {
        // Atomic — chain stops at i. The chosen Action is in the parent's Plan
        // but doesn't extend the chain (it's a leaf, executed by gameplay code).
        break;
    }

    // Composite — extend the chain.
    ActiveChain.Add(NextChild);
    bChainChanged = true;

    auto& ChildCurrent = NextChild.Get<FFragment_Goap_Action_Current>();
    ChildCurrent._ActiveParent = CurrAction;

    // Goal = this Action's own declared effects, re-keyed against the child's
    // resolved WS (no injection from parent). The Definition fragment stores
    // _Effects as raw key/value pairs; Setup pre-resolved them against the
    // ActionSet's WS source. Here, ChildCurrent._Goal is set to that resolved
    // form (the same FCk_GoapWS_Conditions value computed at Setup time).
    ChildCurrent._Goal = NextChild.Get<FFragment_Goap_Action_Definition>()
                                  .Get_GoalFromEffects();

    // Resolve WS source.
    ResolveAndAssignWorldStateSource(NextChild, CurrAction, InActionSet);

    SubscribeActionToWorldState(NextChild);

    NextChild.AddOrGet<FTag_Goap_Action_RequiresInitialPlan>();

    UUtils_Signal_Goap_OnActionActivated::Broadcast(NextChild, MakePayload(...));

    break;   // newly-appended Action plans next frame
}

if (bChainChanged)
{
    UUtils_Signal_Goap_OnActiveChainChanged::Broadcast(
        InActionSet, MakePayload(InActionSet, OldChainSnapshot));
}
```

**Plan[0] vs class-derived tag matching.** In the unified model, an Action's planner is given its children as candidate "operators." The A* search picks among those children to satisfy the Action's `_Goal` (which is its own declared `_Effects`). The chosen plan is a list of child handles, in execution order. No tag matching layer; the handle IS the chosen Action.

### 4.3 Truncation semantics — unchanged shape

```cpp
void TruncateChainFrom(
    FCk_Handle_Goap_ActionSet& InActionSet,
    TArray<FCk_Handle_Goap_Action>& InActiveChain,
    int32 InStartIndex)
{
    for (int32 i = InActiveChain.Num() - 1; i >= InStartIndex; --i)
    {
        auto& Action = InActiveChain[i];

        UnsubscribeActionFromWorldState(Action);

        auto& Current = Action.Get<FFragment_Goap_Action_Current>();
        Current._Goal = {};
        Current._InvalidGoal = {};
        Current._ActiveParent = FCk_Handle_Goap_Action{};
        Current._WorldStateSource_Resolved = {};
        Current._Plan.Reset();
        Current._PlanStatus = ECk_GoapPlanStatus::Idle;

        UUtils_Signal_Goap_OnActionDeactivated::Broadcast(Action, MakePayload(Action));

        InActiveChain.RemoveAt(i);
    }
}
```

**Catalog entries are NOT destroyed by truncation.** A truncated Action goes dormant; its registration in the ActionSet's catalog and its tree position (parent / children) persist.

---

## 5. WorldState injection & dirty/replan plumbing

### 5.1 Resolution chain

For each Action at activation time:

```
_WorldStateSource_Resolved =
    _WorldStateSource_Override (if user-declared via Request_SetWorldStateSource on this Action)
    ELSE ParentAction._WorldStateSource_Resolved (if Action has a ParentAction)
    ELSE ActionSet._WorldStateSource (the ActionSet's default)
```

The ActionSet always has a WS source (set by `SetRootAction(..., InitialWS)`). The root Action inherits from the ActionSet unless overridden. Children inherit from their parent unless overridden.

### 5.2 Goal mechanics — no injection

In Bundle/Tier, the child Tier's `_Goal` was injected from the parent action's `_Effects` at chain extension. In the unified model, that injection is gone:

- An Action's `_Goal` IS its own declared `_Effects`.
- When an Action becomes active (joins the chain), its planner plans against its own effects as the goal.
- The parent's planner picked this Action because it expects the Action's effects to advance the parent's goal — that's the same outcome, just expressed at the parent's level (the parent's planner already considered this Action as a candidate based on its effects vs the parent's preconditions).

`_InvalidGoal` is renamed in concept: when an Action's declared effects reference WS keys not in the resolved WS, those keys land in `_InvalidGoal` at Setup time (not at activation, since effects are static).

### 5.3 Subscriber plumbing — unchanged shape

Same as Bundle/Tier — only the subscriber type changes from Tier handle to Action handle.

### 5.4 Replan policy semantics — per-Action

```
Explicit            — Request_Plan only
OnWorldStateDirty   — when resolved WS has a changed value
OnCostDirty         — when a child Action's cost changes (Request_SetChildActionCost)
OnEitherDirty       — either of the above
```

Throttle (`_MinReplanIntervalSeconds`) coalesces dirty events into one replan per window. Same shape as today.

---

## 6. Lifecycle invariants

### 6.1 State machine for an Action

```
                ┌──────────┐
                │ Catalog  │   ← lives in ActionSet catalog, not in active chain
                │ Dormant  │   ← _PlanStatus = Idle, _ActiveParent invalid
                └─────┬────┘
                      │ ChainUpdate appends → OnActionActivated
                      ▼
                ┌──────────┐
            ┌──→│ Active   │
            │   │ Planning │   ← _PlanStatus cycles: Idle → Planning → PlanFound/Failed
            │   └─────┬────┘
            │         │ Parent's Plan[0] changes
            │         │   → ChainUpdate truncates → OnActionDeactivated
            │         ▼
            │   ┌──────────┐
            │   │ Catalog  │
            │   │ Dormant  │
            └───┴──────────┘
                      │ owner-cascade destroy
                      ▼
                ┌──────────┐
                │ Destroyed│
                └──────────┘
```

### 6.2 Owner-cascade destroy

```
Owner destroyed
  → Goap root destroyed
    → All ActionSets destroyed (via RecordOfActionSets)
      → All Actions destroyed (via RecordOfActions per ActionSet)
        → A* state, subscribers, signals all cleaned up
```

### 6.3 Multi-parent Actions (intentionally not supported in v1)

Every Action has at most one parent (its `_ParentAction`). If the same gameplay behaviour needs to be reachable from two different parent Actions, the author registers two distinct subclasses with distinct identities. This keeps the tree invariant clean and makes the active chain unambiguous.

---

## 7. Diagnostics

### 7.1 Authoring-time validations (Setup-time)

| Check | Trigger | Action |
|---|---|---|
| Action-class uniqueness within ActionSet catalog | Two `AddAction_*` calls with the same ActionClass in the same ActionSet | Second call returns invalid handle, logs `ck::goap::Warning` |
| ActionSet-tag uniqueness within Goap root | Two `AddActionSet` calls with same `_ActionSetTag` on same root | Second call returns invalid handle, logs `ck::goap::Warning` |
| Root Action missing | `Request_Plan` on an ActionSet with no `SetRootAction` called | `OnPlanFailed` fired; no chain mutation |
| Action self-cycle | An Action's child set contains the Action itself (directly or transitively) | Detected at Setup; cycle recorded in `_DependencyCycles`; warning logged |
| Dependency cycle across child tree | A.children contains B; B.children contains A | Detected via Tarjan SCC at Setup; recorded in `_DependencyCycles` |
| Action's effects reference unregistered WS key | Setup-time check after WS keys resolved | `_InvalidGoal` populated; `ck::goap::Verbose` log |

### 7.2 Per-ActionSet `_DependencyCycles`

Tarjan SCC over the child-edge graph at Setup. Result stored on `FFragment_Goap_ActionSet_Current._DependencyCycles`. Queryable via `Get_DependencyCycles(ActionSet)`. Cycles aren't necessarily bugs — sometimes a sub-tree loops back to a parent intentionally — but they're flagged for review.

---

## 8. Migration story (from Bundle/Tier)

This unification replaces the in-progress Bundle/Tier refactor. Since Bundle/Tier's Phases 0–6 are committed but downstream consumers haven't migrated yet, the migration is contained to:

- `CkFoundation/Source/CkGoap/` — code rewrite
- `CkTests/Script/CkGoap/CkAutoTest_Goap_BundleTier_RootOnly.as` — rewrite against new API
- `CkTests/Script/CkGoap/CkAutoTestAction_Goap_BundleTier_Simple.as` — rename + adjust
- `CkGameplayDebugger/Source/CkGoapDebugger/` — already stubbed; no migration needed (will be rewritten in the debugger spec)
- `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` — replace Bundle/Tier overview with ActionSet/Action

No downstream consumers (out-of-tree game projects) had migrated to Bundle/Tier yet, so no external break.

### 8.1 API mapping table

| Bundle/Tier API | ActionSet/Action equivalent |
|---|---|
| `AddBundle(Goap, BundleParams)` | `AddActionSet(Goap, ActionSetParams)` |
| `AddTier(Bundle, TierParams)` (first call = root) | `SetRootAction(ActionSet, RootActionClass, InitialWS)` |
| `AddTier(Bundle, TierParams)` (subsequent) | `AddAction_ToActionSet(ActionSet, ActionClass)` |
| `AddAction(Tier, ActionClass)` | `AddAction_ToAction(ParentAction, ActionClass)` |
| `Find_Bundle` | `Find_ActionSet` |
| `Find_Tier(Bundle, TierTag)` | `Find_Action(ActionSet, ActionTag)` or `Find_ActionByClass` |
| `Get_ActiveTiers(Bundle)` | `Get_ActiveChain(ActionSet)` |
| `Get_Plan(Tier)` returning `TArray<TSubclassOf<...>>` | `Get_Plan(Action)` returning `TArray<FCk_Handle_Goap_Action>` |
| `Get_PlanStatus(Tier)` | `Get_PlanStatus(Action)` |
| `Get_WorldStateSource(Tier)` | `Get_WorldStateSource(Action)` |
| `Get_ActiveParentAction(Tier)` returning class | `Get_ActiveParent(Action)` returning handle |
| `Get_InvalidGoal(Tier)` | `Get_InvalidGoal(Action)` |
| `Get_DependencyCycles(Bundle)` | `Get_DependencyCycles(ActionSet)` |
| `Request_SetEnableToggle(Bundle, ...)` | `Request_SetEnableToggle(ActionSet, ...)` |
| `Request_ResetActiveTiers(Bundle)` | `Request_ResetActiveChain(ActionSet)` |
| `Request_Plan(Tier)` | `Request_Plan(Action)` |
| `Request_SetActionCost(Tier, ActionClass, Cost)` | `Request_SetChildActionCost(Action, ChildClass, Cost)` |
| `OnTierActivated` / `OnTierDeactivated` | `OnActionActivated` / `OnActionDeactivated` |
| `OnActiveTiersChanged` | `OnActiveChainChanged` |
| `SetActionTag(InTag)` (in DefineAction) | **Removed.** Class-derived identity tag replaces it. |
| `_ActionTag` (field on action CDO) | **Removed.** No matching layer needed. |
| `_TierTag` (field on TierParams) | **Removed.** Identity is class-derived. |

---

## 9. Test surface

Rewrite the smoke test against the new API; expand to the same 14-test surface as Bundle/Tier (renamed).

| # | Test | Validates |
|---|---|---|
| 1 | `CkAutoTest_Goap_ActionSet_RootOnly.as` | Single-Action ActionSet (root only), root goal achieved, plan fires |
| 2 | `CkAutoTest_Goap_ActionSet_ChainGrowth.as` | Plan[0] is a composite Action → chain extends, OnActionActivated fires |
| 3 | `CkAutoTest_Goap_ActionSet_ChainTruncation.as` | Plan[0] flips mid-life → chain truncates from new diverge-point, OnActionDeactivated fires |
| 4 | `CkAutoTest_Goap_ActionSet_AtomicLeaf.as` | Plan[0] is atomic (no children) → chain stable at current depth |
| 5 | `CkAutoTest_Goap_ActionSet_GoalIsEffects.as` | Child Action's `_Goal` equals its own declared `_Effects` (no injection) |
| 6 | `CkAutoTest_Goap_ActionSet_WSInheritance.as` | Child without override inherits parent's resolved WS |
| 7 | `CkAutoTest_Goap_ActionSet_WSOverride.as` | Child with override has different `_WorldStateSource_Resolved` than parent |
| 8 | `CkAutoTest_Goap_ActionSet_InvalidGoal.as` | Action's effects reference unknown WS key → land in `_InvalidGoal` |
| 9 | `CkAutoTest_Goap_ActionSet_DirtyPropagation.as` | WS change → all subscribed Actions replan; non-subscribed Actions don't |
| 10 | `CkAutoTest_Goap_ActionSet_MultiActionSet.as` | Two ActionSets on one entity tick independently |
| 11 | `CkAutoTest_Goap_ActionSet_Toggle.as` | Disabled ActionSet skips planning + chain-update; re-enable resumes |
| 12 | `CkAutoTest_Goap_ActionSet_OwnerCascadeDestroy.as` | Destroying owner cleans up GoapRoot → ActionSets → Actions without leaks |
| 13 | `CkAutoTest_Goap_ActionSet_DeferOneFrame.as` | Newly-appended Action does NOT plan in activation frame; plans in frame+1 |
| 14 | `CkAutoTest_Goap_ActionSet_ResetChain.as` | `Request_ResetActiveChain` collapses chain to root; OnActionDeactivated fires per removed Action |

Plus rename + adjust the existing smoke test (`CkAutoTest_Goap_BundleTier_RootOnly.as` → `CkAutoTest_Goap_ActionSet_RootOnly.as`) and its action class file (`CkAutoTestAction_Goap_BundleTier_Simple.as` → `CkAutoTestAction_Goap_ActionSet_Simple.as`).

---

## 10. Out of scope (v1)

- Numeric / hierarchical / non-boolean world state.
- DataAsset-driven ActionSet/Action declarations.
- ActionInitializer-style per-entity action customization.
- Network sync of the active chain.
- Multi-parent Actions (a single Action class participating in two parents' child sets — would require multi-instance per class).
- Per-Action enable toggle (only ActionSet-level toggle in v1).
- Debugger work — separate spec; resumes after this lands.

---

## 11. Open questions / risks

| Question | Disposition |
|---|---|
| Multi-parent Actions | Deferred to v2; v1 enforces tree (each Action has exactly one `_ParentAction`). |
| Cycle detection cost at large catalog sizes | Setup-time only, O(actions × edges). Fine for v1; re-evaluate if profile flags it. |
| `Request_SetRootAction` mid-life behavior | Drops active chain to length 0 (no root), then creates new root; on next ChainUpdate the new root activates. Effect is a hard reset. |
| Plan[0] = an Action that became dormant (was truncated then re-picked) | Re-activation path. ChainUpdate detects the change and re-appends; planner state spins up fresh. Pinned by Test #14. |
| WS override at non-root level | Allowed via `Request_SetWorldStateSource`. Resolution walks parent chain; first override wins. |
| Get_Plan(Action) returns child handles, but consumers may expect ActionClasses | Provide both: `Get_Plan(Action)` returns handles, `Get_PlanClasses(Action)` returns `TArray<TSubclassOf<...>>` for convenience. |

---

## 12. Implementation roadmap

This is a follow-on refactor that **redoes parts of Bundle/Tier Phases 1, 3, 4, 6**. Phase 7 (tests) is renamed and rewritten against the new API. Phase 8 (polish + docs) is unchanged in scope.

### Phase U0 — Rename pass

- Pure rename `Bundle` → `ActionSet` across all code, file names, AS bindings, and the smoke test. No semantic change yet.
- Verify build green.

### Phase U1 — Collapse Tier into Action

- Delete the Tier-specific fragments / utils / processors.
- Promote the existing Tier fragments to Action equivalents (rename `Tier_Params` → `Action_Params`, etc., adjusting fields per §2.2).
- Add the new tree fragment (`FFragment_Goap_Action_Tree`) holding `_ParentAction` + `_ChildActions`.
- `Plan` field changes type from `TArray<TSubclassOf<...>>` to `TArray<FCk_Handle_Goap_Action>` (resolved at HandleResult time).
- Delete `_ActionTag` field and `SetActionTag` builder from `UCk_GoapAction_EntityScript`. Add `Get_ActionTagForClass(InClass)` mirroring SM's helper.
- Verify build green (no consumers besides the smoke test, which we'll rewrite next).

### Phase U2 — New API surface

- Implement `AddActionSet`, `SetRootAction`, `AddAction_ToActionSet`, `AddAction_ToAction`, and the query / request verbs in §3.
- Remove old `AddBundle` / `AddTier` / `AddAction(Tier, ...)` verbs.
- Regenerate AS bindings.
- Verify build green.

### Phase U3 — Processors against the new model

- `FProcessor_Goap_Action_Setup` — extract per-Action CDO defs, populate `_Goal` from `_Effects`, register WS keys, build `_DependencyCycles` per ActionSet.
- `FProcessor_Goap_Action_AutoReplan` — per-Action dirty consumption.
- `FProcessor_Goap_Action_HandleRequests` — per-Action request drain.
- `FProcessor_Goap_Action_HandleResult` — convert A* path to **child-Action handle list** (using each candidate's class-derived tag for lookup against the parent's child list).
- `FProcessor_Goap_ActionSet_ChainUpdate` — the new (simpler) extend / truncate rule from §4.2.
- Verify build green.

### Phase U4 — Subscriber + dirty plumbing retargeted at Action handles

- Same shape as today; only the subscriber type changes.

### Phase U5 — Diagnostics

- `_InvalidGoal` populated at Setup time (effects-vs-WS check).
- `_DependencyCycles` via Tarjan SCC on the child tree.
- All `Get_*` diagnostic accessors per §7.

### Phase U6 — Rewrite the smoke test against the new API

- `CkAutoTest_Goap_ActionSet_RootOnly.as` — single Action, no children, goal achieved.
- Action class file: `CkAutoTestAction_Goap_ActionSet_Simple.as`.
- Toolbox `build-test --test-pattern Goap_ActionSet` green.

### Phase U7 — Write the remaining 13 tests

- Per the table in §9.

### Phase U8 — Polish + docs

- Update `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` to reflect ActionSet/Action model.
- Update CkGoap docs anywhere else they reference Bundle/Tier.
- Archive the Bundle/Tier refactor spec (mark superseded; leave on disk for history).

**Build/test gate after each phase.** Toolbox `build-test` workflow must be green before moving on.

---

## Appendix A — Glossary

| Term | Meaning |
|---|---|
| **ActionSet** | A named decision domain on a Goap root entity. Holds a catalog of Actions, an ordered active chain, and a WorldState source. Multiple ActionSets per Goap root allowed. |
| **Action** | The unit of work in the planner. Declares preconditions, effects, cost. Has a class-derived identity tag. Can have children (other Actions registered under it), making it composite; otherwise atomic. Has its own planner runtime state used when active. |
| **Catalog** | The flat record of all Actions registered in an ActionSet, regardless of tree position. Independent of the active chain. |
| **Active chain** | The ordered list of currently-active Actions in an ActionSet. `[0]` is the root; each subsequent entry is the previous Action's currently-chosen child (Plan[0]). |
| **Composite Action** | An Action that has registered children. When active, its planner expands into a sub-plan picking from its children. |
| **Atomic Action** | An Action with no registered children. When in a parent's Plan[0], terminates the active chain at the parent's depth; executed by gameplay code (action runner). |
| **Plan[0] match** | (Gone.) The unified model has no tag-matching step. `Plan[0]` is already a child Action handle directly. |
| **Resolved WS source** | The WS handle an Action actually consumes. Equals declared override if set, else parent's resolved WS, else ActionSet's WS source. |
| **Active parent** | The Action whose Plan[0] currently points to this Action (the activator). Invalid handle when this Action is itself the root or is dormant. |
| **Goap root** | The container entity that owns one or more ActionSets. One per gameplay entity. |

---

## Appendix B — Naming changes from Bundle/Tier refactor

| Bundle/Tier | ActionSet/Action |
|---|---|
| `FCk_Handle_Goap_Bundle` | `FCk_Handle_Goap_ActionSet` |
| `FCk_Handle_Goap_Tier` | `FCk_Handle_Goap_Action` |
| `FCk_Fragment_Goap_BundleParamsData` | `FCk_Fragment_Goap_ActionSetParamsData` |
| `FCk_Fragment_Goap_TierParamsData` | (gone — Action params live entirely on the action class CDO + per-Action fragments) |
| `_BundleTag` | `_ActionSetTag` |
| `_TierTag` | (gone — replaced by class-derived `ActionTag` on Action entities) |
| `_ActionTag` | (gone — no matching layer; class-derived identity replaces it for debug) |
| `FFragment_RecordOfGoapBundles` | `FFragment_RecordOfGoapActionSets` |
| `FFragment_RecordOfGoapTiers` | `FFragment_RecordOfGoapActions` |
| `FFragment_Goap_Bundle_*` | `FFragment_Goap_ActionSet_*` |
| `FFragment_Goap_Tier_*` | `FFragment_Goap_Action_*` |
| `FFragment_Goap_Bundle_ActiveTiers` | `FFragment_Goap_ActionSet_ActiveChain` |
| `FFragment_Goap_Bundle_TierCatalogIndex` | (gone — chain extension reads `Plan[0]` directly; no catalog index needed for matching) |
| `UCk_Utils_Goap_Bundle_UE` | `UCk_Utils_Goap_ActionSet_UE` |
| `UCk_Utils_Goap_Tier_UE` | `UCk_Utils_Goap_Action_UE` |
| `Goap.Bundle.*` tag space | `Goap.ActionSet.*` |
| `Goap.Tier.*` tag space | (gone — Actions use class-derived tags) |
| `Goap.Action.*` tag space | (still exists for action class-derived tags) |
| `OnTierActivated` / `OnTierDeactivated` | `OnActionActivated` / `OnActionDeactivated` |
| `OnActiveTiersChanged` | `OnActiveChainChanged` |
| `Request_ResetActiveTiers` | `Request_ResetActiveChain` |

---

*End of spec.*
