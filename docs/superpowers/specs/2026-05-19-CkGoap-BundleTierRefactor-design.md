> ⚠ **SUPERSEDED** by [2026-05-19-CkGoap-ActionSetUnification-design.md](2026-05-19-CkGoap-ActionSetUnification-design.md). The Bundle/Tier trichotomy collapses into the ActionSet/Action dichotomy in the successor spec; this file is retained for historical context.

# CkGoap Bundle/Tier Refactor — Design Spec

**Date:** 2026-05-19
**Status:** Approved (design phase)
**Author:** Claude (continuation of brainstorm with @Sulfur-CK)
**Supersedes:** Existing `CkGoap` planner-per-entity model
**Companion spec (next session):** Debugger redesign — to be written after refactor implementation lands.

---

## Quick index

1. [Goals & non-goals](#1-goals--non-goals)
2. [Data model: entities, fragments, handles](#2-data-model-entities-fragments-handles)
3. [API surface](#3-api-surface)
4. [Per-frame processor flow](#4-per-frame-processor-flow)
5. [WorldState injection & dirty/replan plumbing](#5-worldstate-injection--dirtyreplan-plumbing)
6. [Lifecycle invariants](#6-lifecycle-invariants)
7. [Diagnostics](#7-diagnostics)
8. [Migration story](#8-migration-story)
9. [Test surface](#9-test-surface)
10. [Out of scope (v1)](#10-out-of-scope-v1)
11. [Open questions / risks](#11-open-questions--risks)
12. [Implementation roadmap](#12-implementation-roadmap)
13. [Appendix A — Glossary](#appendix-a--glossary)
14. [Appendix B — Reference deviations summary](#appendix-b--reference-deviations-summary)

---

## 1. Goals & non-goals

### Goals

- Replace the current "one planner per entity + shared-WS entity" model with a **Bundle / Tier / ActiveTiers** model where each bundle holds a tier catalog and an active-tier chain.
- **Hierarchy emerges implicitly** from `Action.Tag == Tier.Tag` (strict full-tag equality). No parent pointer, no sub-GOAP type, no marker interface.
- **Each tier is its own ECS entity** (handle = `FCk_Handle_Goap_Tier`). Bundles and the GOAP root are also entities. Hierarchy lives in a separate `ActiveTiers` fragment on the bundle holding an ordered `TArray<FCk_Handle_Goap_Tier>`.
- **WorldState injection at tier level**, default to parent's WS at activation time. Override per-tier with explicit `_WorldStateSource_Override`.
- **Goal injection at activation is synchronous** (no one-frame lag). Newly-appended tier plans next frame against its settled goal.
- Carry over the existing WS subscriber + dirty-tag replan plumbing — same mechanism, retargeted at tier entities.
- Port the reference's **`_InvalidGoalWorldState` diagnostic** for goal keys the child tier doesn't know.
- Multi-bundle **simultaneous execution** with per-bundle enable/disable toggle.

### Non-goals

- No Services. No ActionInitializer.
- No `UCk_GoapGoal_EntityScript`. The goal IS a WorldState — for the root, supplied imperatively (via TierParams); for sub-tiers, injected from parent action's Effects at activation.
- No data-asset-driven bundle/tier/action declaration. API is imperative all the way down.
- No backwards-compat shim. The current `Add` / `Create` API on `UCk_Utils_Goap_UE` is going away (with `Add` retaining its name but a new shape). Existing 12 HGOAP tests are informational only — they'll be deleted at the start of implementation.
- No debugger work in *this* spec. Debugger redesign + mockup F live in a follow-up spec.
- No numeric/hierarchical-planning extensions. Boolean WS only (matches current CkGoap).

---

## 2. Data model: entities, fragments, handles

Three new typesafe handles. The existing `FCk_Handle_Goap` is repurposed to mean "the GOAP root container" instead of "the single planner."

### 2.1 Entity hierarchy

```
                              FCk_Handle              (the owner — NPC, ACk_Pawn, etc.)
                                  │ owner-chain
                                  ▼
                           ┌──────────────────────┐
                           │  FCk_Handle_Goap     │  ← root container (one per entity)
                           │  · RecordOfBundles   │
                           └──────────────────────┘
                                  │ record
                                  ▼
              ┌───────────────────┴───────────────────┐
              ▼                                       ▼
   ┌──────────────────────┐                ┌──────────────────────┐
   │ FCk_Handle_Goap_     │                │ FCk_Handle_Goap_     │
   │     Bundle           │                │     Bundle           │
   │  · BundleTag         │                │  · BundleTag         │
   │  · RecordOfTiers     │                │  · RecordOfTiers     │
   │    (catalog)         │                │    (catalog)         │
   │  · ActiveTiers       │                │  · ActiveTiers       │
   │    (ordered list)    │                │    (ordered list)    │
   │  · EnableToggle      │                │  · EnableToggle      │
   │  · CatalogIndex      │                │  · CatalogIndex      │
   │    (tag→Tier TMap)   │                │    (tag→Tier TMap)   │
   └──────────────────────┘                └──────────────────────┘
              │ record
              ▼
   ┌────────────────────────────────────────┐
   │  FCk_Handle_Goap_Tier                  │  ← one entity per tier
   │  · TierTag                             │
   │  · _WorldStateSource_Override (opt)    │      ← user-declared override
   │  · _WorldStateSource_Resolved          │      ← computed at activation
   │  · _Goal (FCk_GoapWS_Conditions)       │
   │  · _InvalidGoal (diagnostic)           │
   │  · _ActiveParentAction (breadcrumb)    │
   │  · ActionDefs (CDO-extracted)          │
   │  · _Plan (TArray<TSubclassOf<...>>)    │
   │  · _PlanCost / _PlanStatus             │
   │  · _PlanAttemptCount                   │
   │  · ReplanThrottle                      │
   │  · A* SearchState/Result/PlanContext   │
   └────────────────────────────────────────┘
              │ uses
              ▼
   ┌────────────────────────────────────────┐
   │  FCk_Handle_Goap_WorldState  (existing)│  ← one per WS context;
   │  · Values / KeyRegistry                │    shared across tiers
   │  · Subscribers (existing)              │    that inherit from parent
   │  · Dirty tag (existing)                │
   └────────────────────────────────────────┘
```

### 2.2 Fragment table

| Fragment | Lives on | Purpose |
|---|---|---|
| `FFragment_Goap_Root_Params` | Goap root | Reserved for future global tuning (no required fields in v1) |
| `FFragment_RecordOfGoapBundles` | Goap root | Record of bundle entities |
| `FFragment_Goap_Bundle_Params` | Bundle | `BundleTag`, initial enable toggle |
| `FFragment_Goap_Bundle_Current` | Bundle | Current enable state, `_DependencyCycles` diagnostic |
| `FFragment_RecordOfGoapTiers` | Bundle | Record of tier entities (the **catalog**) |
| `FFragment_Goap_Bundle_ActiveTiers` | Bundle | `TArray<FCk_Handle_Goap_Tier>` — ordered active chain |
| `FFragment_Goap_Bundle_TierCatalogIndex` | Bundle | `TMap<FGameplayTag, FCk_Handle_Goap_Tier>` — O(1) tag→tier lookup |
| `FFragment_Goap_Tier_Params` | Tier | `TierTag`, declared `_WorldStateSource_Override`, planning-frequency, replan-policy, search budget, cost threshold, plan-on-start |
| `FFragment_Goap_Tier_Current` | Tier | `_WorldStateSource_Resolved`, `_Goal`, `_InvalidGoal`, `_Plan`, `_PlanCost`, `_PlanStatus`, `_PlanAttemptCount`, `_ActiveParentAction` |
| `FFragment_Goap_Tier_Actions` | Tier | CDO-extracted action defs (same shape as today's `FFragment_Goap_Actions`) |
| `FFragment_Goap_Tier_ActionClasses` | Tier | List of `TSubclassOf<UCk_GoapAction_EntityScript>` registered via `AddAction` (consumed at Setup) |
| `FFragment_Goap_Tier_Requests` | Tier | std::variant queue of tier-targeted requests |
| `FFragment_Goap_Tier_ReplanThrottle` | Tier | Per-tier throttle (same shape as today, just per-tier) |
| `FFragment_Goap_Tier_SearchState` / `_Result` / `_PlanContext` | Tier | A* fragments (unchanged from today, just per-tier) |
| `FFragment_AStar_Params` | Tier | The underlying CkAStar config (search budget, cost threshold) |
| `FTag_Goap_Bundle_RequiresChainUpdate` | Bundle | Set after any tier in the bundle plans; consumed by chain-update processor (optimisation — avoids walking inert bundles each frame) |
| `FTag_Goap_Tier_RequiresSetup` | Tier | One-shot setup gate (analogous to today's planner setup) |
| `FTag_Goap_Tier_RequiresInitialPlan` | Tier | Drives first plan after activation |
| `FTag_Goap_Tier_PlanRequested` | Tier | Request-flow gate |
| `FTag_Goap_Dirty_WorldState` / `FTag_Goap_Dirty_Cost` | Tier | Per-tier dirty tracking (subscriber plumbing carries over) |

### 2.3 Why the active chain is its own fragment (not part of bundle params)

User's explicit "we avoid defining the hierarchy in the fragment itself" — hierarchy is dynamic, not data-driven. Putting it in a separate fragment makes the active-chain mutate-set independent of bundle params and keeps params immutable post-setup.

### 2.4 The `_ActiveParentAction` breadcrumb

On each non-root tier in the active chain, we record which parent action injected this tier's current goal. Two uses:

- **Debugger:** "This tier was spawned by Strategic's `OperateShop` action."
- **Activation invalidation:** If a parent tier's `Plan[0]` changes between iterations, the child's `_ActiveParentAction` records the OLD parent action. ChainUpdate compares the new Plan[0] to the recorded `_ActiveParentAction`; mismatch ⇒ truncate.

### 2.5 New action field: `_ActionTag`

`UCk_GoapAction_EntityScript` gains a single new field, populated by the builder API in the same pattern as `_Preconditions` / `_Effects` / `_Cost`:

```cpp
// In UCk_GoapAction_EntityScript — plain C++ member, not a UPROPERTY.
// Populated via the SetActionTag builder; read by FProcessor_Goap_Setup
// (which is a friend class). Lives on the CDO; not editor-exposed.
FGameplayTag _ActionTag;

// Builder:
UFUNCTION(BlueprintCallable, Category = "Ck|GOAP|Action",
          DisplayName = "[Ck][GOAP] Set Action Tag")
void SetActionTag(
    UPARAM(meta = (Categories = "Goap.Tier")) FGameplayTag InTag);
```

**Semantically, an action's tag IS a tier tag.** The author of an action calls `SetActionTag(...)` with the `_TierTag` value of whichever sub-tier the action delegates to. The `UPARAM(meta = (Categories = "Goap.Tier"))` constraint surfaces only `Goap.Tier.*` tags in BP / AS pickers at the call site, so authors discover the available sub-tier tags directly.

Subclasses populate this in their `DefineAction` builder (a new `SetActionTag(FGameplayTag)` builder added alongside `AddPrecondition`/`AddEffect`/`SetCost`). At chain-update time the bundle's catalog is keyed by `_TierTag`; the processor calls `Catalog.Find(NextActionTag)` for strict-equality lookup. When `_ActionTag` equals a tier's `_TierTag` (full-tag), that tier auto-activates as the child of whichever tier is currently planning this action.

Two important rules:

- `_ActionTag` is **scoped to the action subclass**, not to the action's appearance on a specific tier. Two tiers in the same bundle that both register the same action subclass will both see that action's tag.
- An action whose `_ActionTag` equals NO tier in the bundle catalog is a **leaf action** — the chain stops at the tier that planned it. The same is true for an action that never calls `SetActionTag` (its `_ActionTag` stays as a default-constructed `FGameplayTag`).

---

## 3. API surface

The new API is **imperative all the way down**. Bundles, tiers, and actions are added via explicit verbs on `UCk_Utils_Goap_UE`. There are no DataAssets describing the bundle/tier/action tree.

### 3.1 Construction verbs

```cpp
// Root container — one per entity. Replaces today's planner-on-owner Add.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add")
static FCk_Handle_Goap Add(
    UPARAM(ref) FCk_Handle& InOwner,
    const FCk_Fragment_Goap_RootParamsData& InParams);

// Bundle — top-level decision domain. Multiple bundles per Goap root allowed.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add Bundle")
static FCk_Handle_Goap_Bundle AddBundle(
    UPARAM(ref) FCk_Handle_Goap& InGoap,
    const FCk_Fragment_Goap_BundleParamsData& InParams);

// Tier — one decision level within a bundle. First AddTier on a bundle is the root.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add Tier")
static FCk_Handle_Goap_Tier AddTier(
    UPARAM(ref) FCk_Handle_Goap_Bundle& InBundle,
    const FCk_Fragment_Goap_TierParamsData& InParams);

// Action — a TSubclassOf<UCk_GoapAction_EntityScript> declared on a tier.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
          DisplayName = "[Ck][Goap] Add Action")
static FCk_Handle_Goap_Tier AddAction(
    UPARAM(ref) FCk_Handle_Goap_Tier& InTier,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);
```

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
struct CKGOAP_API FCk_Fragment_Goap_BundleParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_BundleParamsData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories = "Goap.Bundle"))
    FGameplayTag _BundleTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _InitialToggle = ECk_EnableDisable::Enable;

public:
    CK_PROPERTY_GET(_BundleTag);
    CK_PROPERTY(_InitialToggle);
    CK_DEFINE_CONSTRUCTORS(FCk_Fragment_Goap_BundleParamsData, _BundleTag);
};

USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_TierParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_TierParamsData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories = "Goap.Tier"))
    FGameplayTag _TierTag;

    // Optional WS override. If unset, this tier inherits parent's resolved WS
    // at activation. The ROOT tier (first AddTier on a bundle) MUST set this.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Handle_Goap_WorldState _WorldStateSource_Override;

    // Root-only: initial goal world state. Ignored on non-root tiers (their
    // goal is injected from the parent action's Effects at activation).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FCk_GoapWS_Condition_Authored> _InitialGoal_RootOnly;

    // Per-tier tuning, all optional.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    float _SearchBudgetMicroseconds = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    float _CostThreshold = 1.0e9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_Goap_ReplanPolicy _ReplanPolicy = ECk_Goap_ReplanPolicy::OnWorldStateDirty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    float _MinReplanIntervalSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _PlanOnStart = true;

public:
    CK_PROPERTY_GET(_TierTag);
    CK_PROPERTY(_WorldStateSource_Override);
    CK_PROPERTY(_InitialGoal_RootOnly);
    CK_PROPERTY(_SearchBudgetMicroseconds);
    CK_PROPERTY(_CostThreshold);
    CK_PROPERTY(_ReplanPolicy);
    CK_PROPERTY(_MinReplanIntervalSeconds);
    CK_PROPERTY(_PlanOnStart);
    CK_DEFINE_CONSTRUCTORS(FCk_Fragment_Goap_TierParamsData, _TierTag);
};
```

`FCk_GoapWS_Condition_Authored` is a BlueprintType wrapper around `(FGameplayTag Key, bool Value)` — sugar for declaring goal conditions in editor / AS. The Setup processor resolves these into the internal `FCk_GoapWS_Condition` form once keys are registered.

### 3.3 Runtime mutation verbs (per-tier targeted)

```cpp
// Replace the tier's goal. For the root, this is the live goal. For non-root
// tiers, the goal will be overwritten at the next chain-update — use only when
// you know what you're doing.
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_SetGoalWorldState(
    UPARAM(ref) FCk_Handle_Goap_Tier& InTier,
    const TArray<FCk_GoapWS_Condition_Authored>& InGoal);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_Plan(UPARAM(ref) FCk_Handle_Goap_Tier& InTier);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_CancelPlan(UPARAM(ref) FCk_Handle_Goap_Tier& InTier);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_SetActionCost(
    UPARAM(ref) FCk_Handle_Goap_Tier& InTier,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass,
    float InCost);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_SetReplanInterval(
    UPARAM(ref) FCk_Handle_Goap_Tier& InTier, float InSeconds);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_SetReplanPolicy(
    UPARAM(ref) FCk_Handle_Goap_Tier& InTier, ECk_Goap_ReplanPolicy InPolicy);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_SetSearchBudget(
    UPARAM(ref) FCk_Handle_Goap_Tier& InTier, float InMicroseconds);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Tier Request_SetCostThreshold(
    UPARAM(ref) FCk_Handle_Goap_Tier& InTier, float InThreshold);
```

### 3.4 Bundle-level verbs

```cpp
// Mute / un-mute a bundle. Disabled bundles skip planning + chain-update.
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Bundle Request_SetEnableToggle(
    UPARAM(ref) FCk_Handle_Goap_Bundle& InBundle,
    ECk_EnableDisable InToggle);

// Reset the active-tier chain back to just the root. Useful after a hard
// world-state change or scripted teleport.
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Bundle Request_ResetActiveTiers(
    UPARAM(ref) FCk_Handle_Goap_Bundle& InBundle);
```

### 3.5 Query verbs

```cpp
// Goap root.
static bool Has(const FCk_Handle& InHandle);
static FCk_Handle_Goap Cast(const FCk_Handle& InHandle);

// Bundle lookup.
static FCk_Handle_Goap_Bundle Find_Bundle(
    const FCk_Handle_Goap& InGoap, FGameplayTag InBundleTag);

// Tier lookup (within a bundle's catalog).
static FCk_Handle_Goap_Tier Find_Tier(
    const FCk_Handle_Goap_Bundle& InBundle, FGameplayTag InTierTag);

// Active tier chain inspection — ordered, root at [0].
static TArray<FCk_Handle_Goap_Tier> Get_ActiveTiers(
    const FCk_Handle_Goap_Bundle& InBundle);

// Per-tier plan state.
static ECk_GoapPlanStatus Get_PlanStatus(const FCk_Handle_Goap_Tier& InTier);
static TArray<TSubclassOf<UCk_GoapAction_EntityScript>> Get_Plan(
    const FCk_Handle_Goap_Tier& InTier);
static float Get_PlanCost(const FCk_Handle_Goap_Tier& InTier);

// Per-tier WS + parent-action breadcrumbs.
static FCk_Handle_Goap_WorldState Get_WorldStateSource(
    const FCk_Handle_Goap_Tier& InTier);
static FCk_Handle_Goap_Tier Get_ParentActiveTier(
    const FCk_Handle_Goap_Tier& InTier);   // returns invalid handle for root
static TSubclassOf<UCk_GoapAction_EntityScript> Get_ActiveParentAction(
    const FCk_Handle_Goap_Tier& InTier);   // returns nullptr for root

// Diagnostics.
static TArray<FCk_GoapWS_Condition_Authored> Get_InvalidGoal(
    const FCk_Handle_Goap_Tier& InTier);
static TArray<FString> Get_DependencyCycles(
    const FCk_Handle_Goap_Bundle& InBundle);
```

### 3.6 Signals

```cpp
// Per-tier — fires when a tier's plan completes / fails.
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanComplete,
    FCk_Delegate_Goap_OnPlanComplete,
    FCk_Handle_Goap_Tier,                                       // Source: tier
    TArray<TSubclassOf<UCk_GoapAction_EntityScript>>,           // Action list
    float);                                                      // Plan cost

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanFailed,
    FCk_Delegate_Goap_OnPlanFailed,
    FCk_Handle_Goap_Tier);

// Bundle-level — fires whenever the chain mutates.
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnActiveTiersChanged,
    FCk_Delegate_Goap_OnActiveTiersChanged,
    FCk_Handle_Goap_Bundle,                                     // Source
    TArray<FCk_Handle_Goap_Tier>);                              // Old chain
    // (New chain readable via Get_ActiveTiers in the handler.)

// Tier-level — fires once when a tier enters / exits the active chain.
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnTierActivated,
    FCk_Delegate_Goap_OnTierActivated,
    FCk_Handle_Goap_Tier);

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnTierDeactivated,
    FCk_Delegate_Goap_OnTierDeactivated,
    FCk_Handle_Goap_Tier);
```

`OnTierActivated` is the right hook for "start running this tier" code (kick off animation, claim resources). `OnTierDeactivated` is the cleanup pair. `OnPlanComplete` on the *leaf* tier is what an action-runner consumer subscribes to.

---

## 4. Per-frame processor flow

Six processors, all in `FGroup_Gameplay_AI`:

1. `FProcessor_Goap_Tier_Setup` — extract CDOs into tier action defs, register WS keys
2. `FProcessor_Goap_Tier_AutoReplan` — consume per-tier dirty tags, enqueue Plan requests
3. `FProcessor_Goap_Tier_HandleRequests` — drain per-tier request queue, build A* graph
4. `TProcessor_AStar_Execute<...>` — time-sliced A* (reused from CkAStar, unchanged)
5. `FProcessor_Goap_Tier_HandleResult` — convert A* path to action sequence, fire `OnPlanComplete`
6. `FProcessor_Goap_Bundle_ChainUpdate` ⭐ NEW — walk each bundle's ActiveTiers, apply truncate/extend rule

### 4.1 Ordering and rationale

Order within `FGroup_Gameplay_AI` (set via `CK_REGISTER_PROCESSOR` call order):

```
Setup → AutoReplan → HandleRequests → AStar_Execute → HandleResult → ChainUpdate
```

**Why ChainUpdate runs LAST.** All tiers must have planned this frame before the chain can be re-evaluated. ChainUpdate reads each tier's `_Plan[0]` and decides whether to truncate or extend the bundle's chain. Putting it last in the group guarantees `Plan[0]` is fresh.

### 4.2 Sequence diagram — one frame on one bundle

```mermaid
sequenceDiagram
    participant AR as Tier.AutoReplan
    participant HR as Tier.HandleRequests
    participant AX as AStar.Execute
    participant HRES as Tier.HandleResult
    participant CU as Bundle.ChainUpdate

    Note over AR,CU: For each tier in ActiveTiers (per-tier processors)
    AR->>HR: Dirty? enqueue Plan request
    HR->>HR: Pop Plan request, build FGoapGraph, seed search
    AX->>AX: Time-sliced A* search (multi-frame OK)
    Note over AX: Eventually completes
    HRES->>HRES: Path to action sequence, write _Plan
    HRES-->>CU: Tier OnPlanComplete signal fires

    Note over CU: After ALL tiers in bundle have planned this frame
    CU->>CU: For i=0..size-1 read ActiveTiers[i]._Plan[0]
    CU->>CU: Apply truncate/extend rule
    Note over CU: Newly-appended tiers do NOT plan this frame;<br/>plan starts NEXT frame
```

### 4.3 The chain-update rule — exact pseudocode

```cpp
// FProcessor_Goap_Bundle_ChainUpdate::ForEachEntity (per bundle, each frame)

if (BundleEnableToggle == Disable) return;

auto& ActiveTiers = InBundle.Get<FFragment_Goap_Bundle_ActiveTiers>()._Tiers;
const auto& CatalogIndex =
    InBundle.Get<FFragment_Goap_Bundle_TierCatalogIndex>()._TagToTier;

auto i = 0;
auto bChainChanged = false;
auto OldChainSnapshot = ActiveTiers;       // for the OnActiveTiersChanged signal

while (i < ActiveTiers.Num())
{
    const auto& CurrTier = ActiveTiers[i];
    const auto& CurrCurrent = CurrTier.Get<FFragment_Goap_Tier_Current>();

    if (CurrCurrent._PlanStatus != ECk_GoapPlanStatus::PlanFound)
    {
        // This tier hasn't successfully planned yet (Planning / Failed /
        // freshly-appended-this-frame). Don't mutate the chain past it.
        return;
    }

    const auto& Plan = CurrCurrent._Plan;
    if (Plan.IsEmpty())
    {
        // No actions to execute (goal already satisfied?). Truncate everything
        // past this tier — there is no child driver.
        if (i + 1 < ActiveTiers.Num())
        {
            TruncateChainFrom(InBundle, ActiveTiers, i + 1);
            bChainChanged = true;
        }
        break;
    }

    const auto NextActionClass = Plan[0];
    const auto NextActionTag   = NextActionClass.GetDefaultObject()->Get_ActionTag();

    const auto MatchingTier = CatalogIndex.Contains(NextActionTag)
        ? CatalogIndex[NextActionTag] : FCk_Handle_Goap_Tier{};

    if (i + 1 < ActiveTiers.Num())
    {
        // We already have a child. Does it still match?
        const auto& ExistingChild = ActiveTiers[i + 1];
        const auto ExistingChildTag = ExistingChild.Get<FFragment_Goap_Tier_Params>()
                                         .Get_TierTag();
        if (ExistingChildTag == NextActionTag)
        {
            // Match — chain stable at this position. Move on.
            ++i;
            continue;
        }

        // Mismatch — truncate from i+1 onward, then fall through to leaf-handling.
        TruncateChainFrom(InBundle, ActiveTiers, i + 1);
        bChainChanged = true;
    }

    // i is now effectively at the leaf.
    if (NOT ck::IsValid(MatchingTier))
    {
        // No tier in the catalog matches Plan[0]'s tag. Chain stays at length i+1.
        break;
    }

    // Append the new tier.
    ActiveTiers.Add(MatchingTier);
    bChainChanged = true;

    // Synchronous goal injection.
    const auto& ParentActionDef = ResolveActionDef(CurrTier, NextActionClass);
    auto& MatchingTierCurrent =
        MatchingTier.Get<FFragment_Goap_Tier_Current>();

    // Goal injection — synchronous, with InvalidGoal collection.
    InjectGoal_Synchronous(MatchingTier, MatchingTierCurrent, ParentActionDef);

    MatchingTierCurrent._ActiveParentAction = NextActionClass;

    // Resolve WS source. Override-if-set, else parent's resolved.
    ResolveAndAssignWorldStateSource(MatchingTier, CurrTier);

    // Subscribe tier to its WS for dirty tracking.
    SubscribeTierToWorldState(MatchingTier);

    // Mark for first plan (AutoReplan picks this up next frame).
    MatchingTier.AddOrGet<FTag_Goap_Tier_RequiresInitialPlan>();

    // Per-tier OnTierActivated signal.
    UUtils_Signal_Goap_OnTierActivated::Broadcast(MatchingTier, MakePayload(...));

    // STOP. Newly-appended tier does NOT plan this frame (decision Q4).
    // Continuation happens next frame when this child has a fresh Plan[0].
    break;
}

if (bChainChanged)
{
    UUtils_Signal_Goap_OnActiveTiersChanged::Broadcast(
        InBundle, MakePayload(InBundle, OldChainSnapshot));
}
```

### 4.4 Truncation semantics

```cpp
void TruncateChainFrom(
    FCk_Handle_Goap_Bundle& InBundle,
    TArray<FCk_Handle_Goap_Tier>& InActiveTiers,
    int32 InStartIndex)
{
    for (int32 i = InActiveTiers.Num() - 1; i >= InStartIndex; --i)
    {
        auto& Tier = InActiveTiers[i];

        UnsubscribeTierFromWorldState(Tier);

        auto& Current = Tier.Get<FFragment_Goap_Tier_Current>();
        Current._Goal = {};
        Current._InvalidGoal = {};
        Current._ActiveParentAction = nullptr;
        Current._WorldStateSource_Resolved = {};
        Current._Plan.Reset();
        Current._PlanStatus = ECk_GoapPlanStatus::Idle;

        // Per-tier OnTierDeactivated signal.
        UUtils_Signal_Goap_OnTierDeactivated::Broadcast(Tier, MakePayload(Tier));

        InActiveTiers.RemoveAt(i);
    }
}
```

**Tier catalog entries are NOT destroyed by truncation.** A truncated tier just goes dormant — its action defs persist in the catalog. Re-appending the same tier later picks up where the catalog entry left off (with a fresh goal-injection and a forced replan).

---

## 5. WorldState injection & dirty/replan plumbing

### 5.1 Resolution chain

For each tier at activation time:

```
_WorldStateSource_Resolved =
    _WorldStateSource_Override (if user-declared on this tier)
    ELSE ParentActiveTier._WorldStateSource_Resolved
```

The resolved handle is written into a separate field (`_WorldStateSource_Resolved` on `FFragment_Goap_Tier_Current`) so processors and the planner always have one source of truth without re-walking the chain at plan time.

The **root tier** MUST declare an override (there's no parent to inherit from). Setup validates this:

- If the root tier has no override → `ck::goap::Warning` log, root tier's `_PlanStatus` stays `Idle`, `OnPlanFailed` never fires (because we never planned). Tests that exercise this path assert via the diagnostic, not via the warning.

### 5.2 Subscriber plumbing (carries over)

Existing `FFragment_Goap_WorldState_Subscribers` lives on the WS entity. The subscriber list already accepts generic `FCk_Handle` entries — tiers slot in directly. Lifecycle:

```
Tier joins chain (ChainUpdate appends)
  → SubscribeTierToWorldState(Tier)
  → adds Tier handle to WS._Subscribers

Tier leaves chain (ChainUpdate truncates, owner destroyed, etc.)
  → UnsubscribeTierFromWorldState(Tier)
  → removes Tier handle from WS._Subscribers

WS value mutates (Request_SetValue succeeds AND value changed)
  → WS processor stamps FTag_Goap_Dirty_WorldState onto each subscribed tier

Next frame:
  → Tier AutoReplan reads FTag_Goap_Dirty_WorldState
  → If replan policy permits + throttle elapsed → enqueue Plan request
  → Remove FTag_Goap_Dirty_WorldState
```

**No change** from today's plumbing besides the subscriber identity (was: planner entity; now: tier entity).

### 5.3 Synchronous goal injection (rationale)

Reference uses `Request_UpdateGoalWorldState` (queued, one-frame lag). We choose synchronous because:

- We own the tier entity directly at append time — we can write `_Goal` immediately.
- The newly-appended tier won't plan this frame anyway (deferred). So there's no race between goal-set and plan.
- Removes the one-frame transient where a newly-appended tier briefly has an empty goal.

```cpp
void InjectGoal_Synchronous(
    FCk_Handle_Goap_Tier& InTier,
    FFragment_Goap_Tier_Current& InCurrent,
    const goap::FActionDef& InParentActionDef)
{
    const auto& ChildWS = InTier.Get<FFragment_Goap_Tier_Current>()
                              ._WorldStateSource_Resolved;
    const auto& ChildRegistry = ChildWS.Get<FFragment_Goap_WorldState_KeyRegistry>()
                                       .Get_Registry();

    InCurrent._Goal = {};
    InCurrent._InvalidGoal = {};

    for (const auto& Effect : InParentActionDef.Effects)
    {
        // Effect is already keyed (resolved at Setup-time via the parent's
        // registry). We need to re-resolve via the CHILD's registry — the
        // child may have a different WS source.
        const auto ChildKey = ChildRegistry.Find(InParentActionDef.GetRawTag(Effect));
        if (ChildKey == goap::InvalidGoapKey)
        {
            InCurrent._InvalidGoal.Add(
                FCk_GoapWS_Condition_Authored{ /* unresolved tag */, Effect.Value });
        }
        else
        {
            InCurrent._Goal.Add(goap::FWorldStateCondition{ChildKey, Effect.Value});
        }
    }
}
```

The plan request that drives the new tier's first planning is added via `FTag_Goap_Tier_RequiresInitialPlan` (set in `AppendToChain`); AutoReplan consumes it next frame.

### 5.4 Replan policy semantics (per tier)

Same enum as today, applied per-tier:

| Policy | Replans when |
|---|---|
| `Explicit` | Only when `Request_Plan` is called explicitly |
| `OnWorldStateDirty` | When the tier's resolved WS has a changed value |
| `OnCostDirty` | When an action's cost changes (via `Request_SetActionCost` on this tier) |
| `OnEitherDirty` | Either of the above |

Throttle (`_MinReplanIntervalSeconds`) coalesces dirty events into one replan at window end. Same shape as today.

---

## 6. Lifecycle invariants

### 6.1 State machine for a tier

```
                ┌──────────┐
                │ Catalog  │   ← lives in bundle catalog, not in active chain
                │ Dormant  │   ← _PlanStatus = Idle, _WorldStateSource_Resolved = invalid
                └─────┬────┘
                      │ ChainUpdate appends → OnTierActivated
                      ▼
                ┌──────────┐
            ┌──→│ Active   │
            │   │ Planning │   ← _PlanStatus cycles: Idle → Planning → PlanFound/Failed
            │   └─────┬────┘
            │         │ Plan[0] tag changes / parent flips
            │         │   → ChainUpdate truncates → OnTierDeactivated
            │         ▼
            │   ┌──────────┐
            │   │ Catalog  │   ← back to dormant; ready for re-activation
            │   │ Dormant  │
            └───┴──────────┘
                      │ owner-cascade destroy
                      ▼
                ┌──────────┐
                │ Destroyed│
                └──────────┘
```

### 6.2 Owner-cascade destroy

Destroying the Goap root entity (or its owner) cascades through the existing record-of-entities mechanism:

```
Owner destroyed
  → Goap root destroyed (via owner-chain)
    → All bundles destroyed (via RecordOfBundles)
      → All tiers destroyed (via RecordOfTiers per bundle)
        → A* state, subscribers, signals all cleaned up
```

WorldState entities are NOT owned by the Goap root. Their lifetime is independent (typically owned by the same owner as the Goap root, but that's a consumer choice). Truncation of an active tier unsubscribes that tier from its WS; the WS itself is unaffected.

### 6.3 Replan-during-execute

A tier may replan while a previous plan is still being consumed by gameplay code (the action runner). `_Plan` simply gets overwritten. Consumer code is expected to either re-read `_Plan[0]` each frame (cheap) or bind `OnPlanComplete` and accept that the action queue may shift between dispatches. Matches today's semantics.

### 6.4 Chain mutation during a tier's in-flight A*

If a tier is in `Planning` status (mid-A*) when ChainUpdate runs, ChainUpdate **does not mutate the chain past that tier** (see pseudo-code §4.3 — early return on non-`PlanFound` status). This guarantees we never truncate a tier's child while the tier itself is mid-decision. Next frame, when the tier has completed, ChainUpdate proceeds.

Failure mode: if a tier's plan permanently fails (A* never finds a path with the current cost threshold), the chain stalls at that tier and never extends past it. Diagnostic: `OnPlanFailed` fires; the debugger surfaces it; gameplay code may explicitly `Request_ResetActiveTiers` to recover.

### 6.5 Root-tier replacement

The root tier cannot be truncated by ChainUpdate (`i=0` is never the target of `TruncateChainFrom(0)` — the loop runs `i=0..size-1` and only truncates from `i+1` onward). To swap out the root tier, the consumer must `Request_ResetActiveTiers` (which resets to the bundle's declared root) — and the declared root is fixed at AddTier time. There is no "change the root" API in v1; if you need it, destroy the bundle and re-add. Listed in Open Questions.

### 6.6 Concurrency: multiple tiers planning in-flight

Multiple tiers in the same bundle (or across bundles) may have concurrent A* searches in flight. Each tier owns its own `FFragment_Goap_Tier_SearchState` / `_Result` / `_PlanContext`, so there's no shared mutable state. A* itself is single-threaded but time-sliced — many tiers progress fractionally each frame.

---

## 7. Diagnostics

### 7.1 `_InvalidGoalWorldState` per tier

When a parent action's Outcomes contain WS keys that the child tier's resolved WS doesn't know, those keys go into `_InvalidGoal` on the child tier instead of being silently dropped. The `_Goal` field receives only the subset of keys that ARE registered in the child's WS.

Log level: `ck::goap::Verbose` (NOT Warning — avoid AutoTest escalation per `feedback_autotest_warning_escalation`).

Surfaced via `Get_InvalidGoal(Tier)` for the debugger.

### 7.2 Authoring-time validations (setup-time)

| Check | Trigger | Action |
|---|---|---|
| Tier-tag uniqueness within bundle | Two `AddTier` calls with the same `_TierTag` on the same bundle | Second call returns invalid handle, logs `ck::goap::Warning` |
| Bundle-tag uniqueness within Goap root | Two `AddBundle` calls with same `_BundleTag` on same root | Second call returns invalid handle, logs `ck::goap::Warning` |
| Root-tier must have `_WorldStateSource_Override` | First AddTier on a bundle with no override | Tier created but flagged with Warning; planning gated on missing WS |
| Action without `_ActionTag` | Action subclass's `DefineAction` never calls `SetActionTag` | `ck::goap::Warning` at Setup; action is unusable as a chain-driver but still planning-usable as a leaf |
| Action self-trigger | Tier T has action A with `_ActionTag == T._TierTag` | `ck::goap::Warning` at Setup (would create immediate infinite chain) |
| Dependency cycle in catalog | Tier A's action triggers B, B's triggers A | Setup-time pass records cycle in `_DependencyCycles`; logs once at Setup |

### 7.3 Per-bundle `_DependencyCycles`

Setup-time pass:

```
For each tier T in catalog:
    For each action A in T._Actions:
        If A._ActionTag matches some tier T' in catalog:
            Build directed edge T -> T'

Find all cycles via Tarjan SCC (or simple DFS-on-small-graphs).
Record each as a List<TierTag> in FFragment_Goap_Bundle_Current._DependencyCycles.
```

Logged once at Setup; queryable via `Get_DependencyCycles(Bundle)`.

A cycle isn't necessarily a bug — sometimes a tier's plan loops back through itself intentionally — but the diagnostic flags it for review.

---

## 8. Migration story

**Existing CkGoap users in this repo:**

- `CkTests/Script/CkGoap/CkAutoTest_Goap_SharedWS_*.as` (12 files) — **deleted** in implementation Phase 0.
- `CkGameplayDebugger/Source/CkGoapDebugger/` — data collector body becomes a no-op stub in Phase 0; UI shells stay, display "GOAP debugger pending redesign". Full rewrite in a follow-up spec.

**No other consumers in this repo.** The host project (`Source/CkPlugins/`) doesn't use CkGoap.

**Downstream game projects** (out-of-tree consumers) that called the old API need explicit migration:

| Old API | New equivalent |
|---|---|
| `Add(Owner, GoapParams)` (single planner) | `Add(Owner, RootParams)` + `AddBundle(...)` + `AddTier(...)` + `AddAction(...)` |
| `Create(Owner, Tag, GoapParams)` (named child planner) | `AddBundle` on the Goap root produces the same shape — multi-planner via multi-bundle |
| `AddAction(Goap, ActionClass)` | `AddAction(Tier, ActionClass)` — actions are now scoped per tier |
| `AddGoal(Goap, GoalClass)` | Goal is now a WS conditions list on TierParams (root) or auto-injected (non-root). `UCk_GoapGoal_EntityScript` is **removed**. |
| `Set_WorldStateValue(Goap, ...)` | `utils_goap_world_state::Set_Value(WS_Handle, ...)` — same as today; only the consumer's place to find the WS handle has changed (it's on the tier, not the planner) |
| `Get_Plan(Goap)` | `Get_Plan(Tier)` — multiple plans, one per active tier |
| `BindTo_OnPlanComplete(Goap, ...)` | `BindTo_OnPlanComplete(Tier, ...)` — typically the leaf tier |

No compat shim. Clean break. Documented in the changelog at refactor commit time.

---

## 9. Test surface

New tests under `CkTests/Script/CkGoap/` matching the new model. Test naming preserves the `CkAutoTest_Goap_<Behavior>.as` convention.

| # | Test | Validates |
|---|---|---|
| 1 | `CkAutoTest_Goap_BundleTier_RootOnly.as` | Single-tier bundle, root goal achieved, plan fires |
| 2 | `CkAutoTest_Goap_BundleTier_ChainGrowth.as` | Plan[0] tag matches a catalog tier → chain extends, goal injected, OnTierActivated fires |
| 3 | `CkAutoTest_Goap_BundleTier_ChainTruncation.as` | Plan[0] tag changes mid-life → chain truncates from new diverge-point, OnTierDeactivated fires |
| 4 | `CkAutoTest_Goap_BundleTier_NoMatchingTier.as` | Plan[0] tag matches no catalog tier → chain stable at current depth |
| 5 | `CkAutoTest_Goap_BundleTier_GoalInjection.as` | Child tier's `_Goal` equals parent action's `_Effects` after activation |
| 6 | `CkAutoTest_Goap_BundleTier_WSInheritance.as` | Child without `_WorldStateSource_Override` inherits parent's resolved WS |
| 7 | `CkAutoTest_Goap_BundleTier_WSOverride.as` | Child with override has different `_WorldStateSource_Resolved` than parent |
| 8 | `CkAutoTest_Goap_BundleTier_InvalidGoal.as` | Parent Outcome keys not in child WS → land in `_InvalidGoal`, NOT `_Goal` |
| 9 | `CkAutoTest_Goap_BundleTier_DirtyPropagation.as` | WS change → all subscribed tiers replan; non-subscribed tiers don't |
| 10 | `CkAutoTest_Goap_BundleTier_MultiBundle.as` | Two bundles on one entity tick independently; one's chain doesn't affect the other |
| 11 | `CkAutoTest_Goap_BundleTier_BundleToggle.as` | Disabled bundle skips planning + chain-update; re-enable resumes correctly |
| 12 | `CkAutoTest_Goap_BundleTier_OwnerCascadeDestroy.as` | Destroying owner cleans up Goap root → bundles → tiers without leaks |
| 13 | `CkAutoTest_Goap_BundleTier_DeferOneFrame.as` | Newly-appended tier does NOT plan in the activation frame; plans in frame+1 |
| 14 | `CkAutoTest_Goap_BundleTier_RootReset.as` | `Request_ResetActiveTiers` collapses chain back to root, fires OnTierDeactivated for each removed tier |

**5 baseline CkGoap tests** (the pre-HGOAP ones) — review and either preserve, retarget at the new API, or replace as their semantics dictate. Verdict per test made during Phase 7.

---

## 10. Out of scope (v1)

- Numeric / hierarchical / non-boolean world state.
- Dynamic action cost services (today's static action cost is preserved).
- DataAsset-driven bundle/tier/action declarations.
- ActionInitializer-style per-entity action customization.
- Network sync of the active-tier chain (CurrentState_Bundle_Synced equivalent). Today's CkGoap is local-only; this stays local-only.
- Re-rooting a bundle (changing the root tier without re-creating the bundle).
- Debugger work — separate spec, next session.

---

## 11. Open questions / risks

| Question | Disposition |
|---|---|
| Re-rooting a bundle at runtime | Deferred to v2. v1 requires bundle re-creation. |
| Sub-tier WS schema mismatch authoring helper | Setup-time validation only flags at runtime. Could add an editor-time analyzer later. |
| Multi-tier-simultaneous-execution semantics | Each tier owns its own `_Plan`. Gameplay code chooses which to execute. GOAP is a planner, not a scheduler. (Same as today — explicit in docs.) |
| Cycle detection cost at large catalog sizes | Setup-time only, O(actions × tiers) per bundle. Fine for v1 catalog sizes. Re-evaluate if profile flags it. |
| Frame-rate budget for chain-update on many bundles | ChainUpdate is bundle-scoped; cost is O(chain-depth) per bundle per frame. Catalog lookup is O(1) via TMap. Expect cheap. |
| `Get_PlanStatus` for the leaf tier as a "what's the active action" query | Equivalent to `Get_Plan(Leaf)[0]` after `PlanFound`. Possibly add a `Get_ActiveActionClass(Bundle) -> TSubclassOf` sugar verb. Listed as nice-to-have. |

---

## 12. Implementation roadmap

Phased to minimize "broken build" stretches. Each phase ends with everything compiling + (where applicable) tests green.

### Phase 0 — Cleanup & stubs (one-shot)

- Delete the 12 HGOAP test files under `CkTests/Script/CkGoap/CkAutoTest_Goap_SharedWS_*.as`.
- Stub `CkGoapDebugger_DataCollector.cpp` body to no-op + add TODO marker pointing at the next-session spec.
- Update Slate views in `CkGoapDebugger` that referenced removed data to show "GOAP debugger pending redesign" placeholder.
- `build-test` green.

### Phase 1 — New data types (additive)

- Define `FCk_Handle_Goap_Bundle`, `FCk_Handle_Goap_Tier` typesafe handles.
- Define new fragments (`_Bundle_Params`, `_Bundle_Current`, `_Bundle_ActiveTiers`, `_Bundle_TierCatalogIndex`, `_Tier_Params`, `_Tier_Current`, `_Tier_Actions`, `_Tier_ActionClasses`, `_Tier_Requests`, `_Tier_ReplanThrottle`, `_Tier_SearchState`, `_Tier_Result`, `_Tier_PlanContext`, `_RecordOfGoapBundles`, `_RecordOfGoapTiers`).
- Define new param structs (`FCk_Fragment_Goap_BundleParamsData`, `FCk_Fragment_Goap_TierParamsData`, `FCk_Fragment_Goap_RootParamsData`).
- Define `FCk_GoapWS_Condition_Authored` (public-API condition wrapper).
- Define new request types (`FCk_Request_Goap_Bundle_*`, `FCk_Request_Goap_Tier_*`).
- Define new signals (`Goap_OnPlanComplete` retargeted, `Goap_OnPlanFailed` retargeted, `Goap_OnActiveTiersChanged`, `Goap_OnTierActivated`, `Goap_OnTierDeactivated`).
- Add `_ActionTag` field + `SetActionTag` builder on `UCk_GoapAction_EntityScript`.
- Define new gameplay tag categories: `Goap.Bundle`, `Goap.Tier`, `Goap.Action`.
- `build-test` green (no new tests yet — additive, doesn't break anything).

### Phase 2 — Old API removal + AS regen

- Remove `UCk_GoapGoal_EntityScript` (class + all references).
- Remove old utils verbs on `UCk_Utils_Goap_UE`: `Create`, `AddAction`-on-planner, `AddGoal`-on-planner, `Set_WorldStateValue`-on-planner, etc. (The WS-handle API on `utils_goap_world_state` survives untouched.)
- Remove old fragments specific to per-entity-planner: `FFragment_Goap_Params`, `FFragment_Goap_KeyRegistry` (moves to WS), `FFragment_Goap_WorldState` (renamed/moved), `FFragment_Goap_ActionClasses`, `FFragment_Goap_GoalClasses`, `FFragment_Goap_Actions`, `FFragment_Goap_Goals`, `FFragment_Goap_Current`, `FFragment_Goap_Requests`, `FFragment_Goap_Diagnostics`, `FFragment_Goap_PlanContext`, `FFragment_Goap_SearchState`, `FFragment_Goap_Result`. (Reshape into per-tier equivalents.)
- Repurpose `FCk_Handle_Goap` to mean "Goap root container" (one record-of-bundles).
- Regenerate AS handle JSON via `UCkDynamicHandleSubsystem::GenerateHandleTypeRegistry()`.
- Editor restart confirmed working.
- `build-test` green (nothing in repo consumes the old API after Phase 0 cleanup).

### Phase 3 — New API implementation

- `UCk_Utils_Goap_UE::Add` (root-only container construction).
- `UCk_Utils_Goap_UE::AddBundle`.
- `UCk_Utils_Goap_UE::AddTier` (including validation of root-must-have-override).
- `UCk_Utils_Goap_UE::AddAction`.
- Query verbs (`Find_Bundle`, `Find_Tier`, `Get_ActiveTiers`, `Get_Plan`, `Get_PlanStatus`, `Get_WorldStateSource`, `Get_ParentActiveTier`, `Get_ActiveParentAction`, `Get_InvalidGoal`, `Get_DependencyCycles`).
- Request verbs (per-tier `Request_*` + per-bundle `Request_SetEnableToggle`, `Request_ResetActiveTiers`).
- AS namespaces: `utils_goap`, `utils_goap_bundle`, `utils_goap_tier`.
- `build-test` green.

### Phase 4 — Processors

- `FProcessor_Goap_Tier_Setup` (per-tier CDO extraction; registers WS keys; populates `_Tier_Actions` from `_Tier_ActionClasses`).
- `FProcessor_Goap_Tier_HandleRequests` (per-tier request drain).
- `FProcessor_Goap_Tier_HandleResult` (reuse logic from today's per-planner version; retarget handle type, retarget signal sources).
- `FProcessor_Goap_Tier_AutoReplan`.
- `FProcessor_Goap_Bundle_ChainUpdate` ⭐ THE NEW LOGIC.
- Each processor wired via `CK_REGISTER_PROCESSOR` in the correct order.
- Reuse `TProcessor_AStar_Execute<...>` from CkAStar (no changes there).
- Setup-time bundle catalog index population (`_TagToTier` TMap built once after all `AddTier`s on a bundle).
- `build-test` green (still no new tests; processors compile but aren't exercised yet — that's Phase 7).

### Phase 5 — Subscriber/dirty plumbing

- Update WS subscriber list registration to accept tier handles.
- Tier-side AutoReplan consumes `FTag_Goap_Dirty_WorldState` (mostly moves today's logic).
- Activation-time subscribe / deactivation-time unsubscribe wired in `ChainUpdate`.
- `build-test` green.

### Phase 6 — Diagnostics

- `_InvalidGoalWorldState` field + population during `InjectGoal_Synchronous`.
- Setup-time tag-conflict detection (tier-tag uniqueness within bundle; bundle-tag uniqueness within root; action-self-trigger).
- Per-bundle `_DependencyCycles` field + Tarjan SCC pass at Setup.
- `Get_*` accessors for diagnostics.
- `build-test` green.

### Phase 7 — Tests

- Write all 14 new tests listed in §9.
- Regenerate `CkTests_AutoTestActors.as` wrapper file (toolbox `--discover-fresh`).
- Verify all green via UE automation runner.
- Review 5 baseline CkGoap tests; preserve / retarget / replace each.

### Phase 8 — Polish + docs

- Update `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` to reflect the Bundle/Tier model.
- Update root `Plugins/CkFoundation/CLAUDE.md` if it references CkGoap shape.
- Update any AS-side `assets::*` registries if action class names changed (likely they didn't — same `UCk_GoapAction_EntityScript` base).

**Build/test gate after each phase.** Toolbox `build-test` workflow must be green before moving on to the next phase.

---

## Appendix A — Glossary

| Term | Meaning |
|---|---|
| **Bundle** | A named decision domain on a Goap root entity. Holds a tier catalog + active-tier chain + enable toggle. |
| **Tier** | One decision level within a bundle. Owns a planner, an action set, a goal, a WS source. Identity = `FGameplayTag`. |
| **Action** | A `TSubclassOf<UCk_GoapAction_EntityScript>` registered on a tier. Has declared preconditions, effects, cost, and `_ActionTag`. |
| **Active-tier chain** | The ordered list of currently-active tiers in a bundle. `[0]` is the root; each subsequent entry is the previous tier's currently-active sub-planner. |
| **Catalog** | The set of all tiers a bundle declares at AddTier time. Static. Distinct from the active chain (dynamic). |
| **Plan[0]** | The first action a tier intends to execute (chronologically). Drives chain-update logic. |
| **Plan[0] match** | When `Plan[0]._ActionTag == SomeTier._TierTag` (strict full-tag equality), that tier becomes (or remains) the current tier's sub-tier. |
| **Resolved WS source** | The WS handle a tier actually consumes. Equals declared override if set, else parent's resolved WS at activation. |
| **Active-parent-action breadcrumb** | The TSubclassOf<UCk_GoapAction_EntityScript> on a child tier recording which parent action injected this tier's current goal. |
| **Goap root** | The container entity that owns one or more bundles. One per gameplay entity. |

---

## Appendix B — Reference deviations summary

For future readers comparing this design against the reference at `F:\FullSource\tuqAI\tuqAI\ECS\GOAP\`:

| Aspect | Reference | This design |
|---|---|---|
| Identity | `UTuq_Core_Label*` (UObject) | `FGameplayTag` |
| Match semantics | "Short" (last-segment) name equality | Strict full-tag equality (`Action._ActionTag` and `Tier._TierTag` both live in the `Goap.Tier.*` tagspace; authors set the action's tag to the delegated sub-tier's tag) |
| Goal injection | Deferred via request queue (1-frame lag) | Synchronous at chain append |
| Same-frame chain growth | Yes (newly-appended tier plans in same iteration) | No (deferred to next frame) |
| WS ownership | Per-tier, by value (embedded in CurrentState) | Separate entity, injected; default-to-parent |
| Services | Present (Blueprint-extensible UObjects ticked per frame) | Skipped |
| ActionInitializer | Present | Skipped |
| Goal class | None (goal = WorldState) | None (drop `UCk_GoapGoal_EntityScript`) |
| Bundle catalog | DataAsset-driven via `UTuq_AI_ECS_GOAP_DataAsset_AI` | Imperative API via `AddTier`/`AddAction` |
| Plan ordering | `_Plan.Last()` = first-to-execute (regressive raw) | `_Plan[0]` = first-to-execute (post-reverse) |
| Tier-name match `[]` indexing | Reference uses `Plan.Last()`; we use `Plan[0]` | Different array convention, same semantic |
| Multi-bundle | Simultaneous + per-bundle toggle | Same |
| Diagnostics | `_InvalidGoalWorldState`, dep-cycle | Ported |

---

*End of spec.*
