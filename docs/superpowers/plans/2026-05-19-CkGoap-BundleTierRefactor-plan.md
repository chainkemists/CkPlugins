# CkGoap Bundle/Tier Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor `Plugins/CkFoundation/Source/CkGoap/` from a planner-per-entity model to a Bundle/Tier/ActiveTiers model where hierarchy emerges implicitly from `Action.Tag == Tier.Tag`. Replaces 12 HGOAP tests with a fresh test suite. Stubs `CkGoapDebugger` for follow-up redesign.

**Architecture:** Three new ECS entities — Goap root (one per gameplay entity), Bundle (one per decision domain — Combat, Stocking, etc.), Tier (one per decision level within a bundle). Active-tier chain lives in a separate fragment on each bundle. Each tier owns its own planner state; WorldState entities are shared across tiers via injection (default-to-parent).

**Tech Stack:** UE 5.5, CkFoundation ECS (EnTT), CkAStar for time-sliced A*, AngelScript for tests, MSBuild/UAT for editor build, toolbox `build-test` workflow for automation tests.

**Spec:** `docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md`

---

## File structure overview

### New files (created in this plan)

```
Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/
├── Bundle/
│   ├── CkGoap_Bundle_Fragment.h
│   ├── CkGoap_Bundle_Fragment_Data.h
│   ├── CkGoap_Bundle_Fragment_Data.cpp
│   ├── CkGoap_Bundle_Processor.h          ← FProcessor_Goap_Bundle_ChainUpdate
│   ├── CkGoap_Bundle_Processor.cpp
│   ├── CkGoap_Bundle_Utils.h              ← UCk_Utils_Goap_Bundle_UE
│   └── CkGoap_Bundle_Utils.cpp
└── Tier/
    ├── CkGoap_Tier_Fragment.h
    ├── CkGoap_Tier_Fragment_Data.h
    ├── CkGoap_Tier_Fragment_Data.cpp
    ├── CkGoap_Tier_Processor.h            ← Setup, AutoReplan, HandleRequests, HandleResult
    ├── CkGoap_Tier_Processor.cpp
    ├── CkGoap_Tier_Utils.h                ← UCk_Utils_Goap_Tier_UE
    └── CkGoap_Tier_Utils.cpp
```

### Modified files

```
Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/
├── CkGoap_Fragment.h                      ← strip planner-specific, keep root-container shape
├── CkGoap_Fragment_Data.h                 ← strip planner-specific; add RootParamsData, new enums
├── CkGoap_Fragment_Data.cpp               ← match
├── CkGoap_Processor.h                     ← strip planner-specific processors
├── CkGoap_Processor.cpp                   ← match
├── CkGoap_Utils.h                         ← reshape to root-only operations (Add returns root)
├── CkGoap_Utils.cpp                       ← match
└── EntityScripts/CkGoapAction_EntityScript.h/.cpp  ← add _ActionTag + SetActionTag builder
```

### Deleted files

```
Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/
├── EntityScripts/CkGoapGoal_EntityScript.h
└── EntityScripts/CkGoapGoal_EntityScript.cpp

Plugins/CkTests/Script/CkGoap/
├── CkAutoTest_Goap_SharedWS_*.as          ← 12 files
```

### Stubbed files (body replaced with no-op)

```
Plugins/CkGameplayDebugger/Source/CkGoapDebugger/  ← entire data collector + UI shells
```

---

## Phase 0 — Cleanup & stubs

Get the repo to a state where the old API can be removed without dangling references.

### Task 0.1: Delete the 12 HGOAP test files

**Files:**
- Delete: `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_SharedWS_*.as` (12 files)

- [ ] **Step 1: Verify the file list**

Run: `git ls-files "Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_SharedWS_*.as"`
Expected: 12 files listed.

- [ ] **Step 2: Delete the files**

Run:
```powershell
cd "D:\Repos\CkPlugins\Plugins\CkTests"
git rm Script/CkGoap/CkAutoTest_Goap_SharedWS_*.as
```

- [ ] **Step 3: Verify the auto-tests JSON wrapper file**

Open `Plugins/CkTests/Script/Generated/CkTests_AutoTestActors.as` and search for `SharedWS`. If wrappers exist for any deleted tests, they'll be regenerated on next AS recompile but may emit stale references in the meantime. If file contains stale entries, manually remove the corresponding actor wrappers OR delete the whole generated file and let AS recompile recreate it (preferred).

- [ ] **Step 4: Commit (in CkTests submodule)**

Run:
```powershell
cd "D:\Repos\CkPlugins\Plugins\CkTests"
git add -A
git commit -m "test(CkGoap): remove 12 HGOAP shared-WS tests for Bundle/Tier refactor"
```

### Task 0.2: Stub the CkGoapDebugger data collector

**Files:**
- Modify: `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/Source/Data/CkGoapDebugger_DataCollector.cpp` (find via Glob in the submodule)

- [ ] **Step 1: Locate the data collector source**

Run: `Glob "Plugins/CkGameplayDebugger/Source/CkGoapDebugger/**/CkGoapDebugger_DataCollector*"`

- [ ] **Step 2: Read the existing collector to understand the public API surface (what it returns, what it's called from)**

Read the .h and .cpp; identify the public methods that downstream UI calls.

- [ ] **Step 3: Replace each public method body with a no-op returning a sensible empty value**

For each method that returns a TArray, TSharedPtr, struct, etc., replace the implementation with:

```cpp
// TODO(CkGoap-BundleTierRefactor): full data-collector rewrite pending.
// See docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md
// and the follow-up Debugger Redesign spec (not yet written).
return {};
```

For void methods:
```cpp
// TODO(CkGoap-BundleTierRefactor): no-op until debugger redesign.
```

- [ ] **Step 4: Find every Slate widget that displays "no GOAP data" and replace user-facing text with the placeholder**

Search for the existing labels (e.g. "No Active Plan", "World State Empty") and replace with `LOCTEXT("PendingRedesign", "GOAP debugger pending redesign for Bundle/Tier refactor.")` or similar. One place per panel.

- [ ] **Step 5: Build verify**

Use the build-test toolbox workflow (after the editor frees up — see `feedback_wait_for_other_editor`). Compile errors here would indicate that `CkGoapDebugger` includes types being removed in later phases — flag those as Phase 2 cleanup, not Phase 0 stub.

Run: `Get-Content "D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log" -Wait -Tail 5` (or via Monitor tool). Wait for editor to be unlocked, then trigger build via toolbox or directly:

```powershell
$engine = & "$env:CLAUDE_PROJECT_DIR\CkAuto\Get-ProjectEnginePath.ps1"
& "$engine\Engine\Build\BatchFiles\Build.bat" CkPluginsEditor Win64 Development -Project="$env:CLAUDE_PROJECT_DIR\CkPlugins.uproject" -WaitMutex -FromMsBuild
```

Expected: clean build.

- [ ] **Step 6: Commit (in CkGameplayDebugger submodule)**

```powershell
cd "D:\Repos\CkPlugins\Plugins\CkGameplayDebugger"
git add -A
git commit -m "refactor(CkGoapDebugger): stub data collector for Bundle/Tier refactor"
```

### Task 0.3: Bump submodule pointers in CkPlugins root

- [ ] **Step 1: Update pointers**

```powershell
cd "D:\Repos\CkPlugins"
git add Plugins/CkTests Plugins/CkGameplayDebugger
git commit -m "chore(submodule): bump CkTests + CkGameplayDebugger (Phase 0 BundleTier prep)"
```

---

## Phase 1 — New data types (additive)

Define new fragments, handles, params, and signals. Nothing removed yet; everything new compiles alongside existing.

### Task 1.1: Define `FCk_Handle_Goap_Bundle` and `FCk_Handle_Goap_Tier`

**Files:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Fragment_Data.h`
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Fragment_Data.h`

- [ ] **Step 1: Create the Bundle handle**

In `CkGoap_Bundle_Fragment_Data.h`:

```cpp
#pragma once

#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkGoap/CkGoap.h"
#include "GameplayTagContainer.h"

#include "CkGoap_Bundle_Fragment_Data.generated.h"

// ============================================================================
// TYPESAFE HANDLE
// ============================================================================

USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Handle_Goap_Bundle : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_Goap_Bundle);
};

CK_DEFINE_CUSTOM_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_Goap_Bundle);

// ============================================================================
// PARAMS DATA  (filled out in Task 1.4)
// ============================================================================
```

Use existing typesafe-handle precedents (`FCk_Handle_Goap` in `CkGoap_Fragment_Data.h`, `FCk_Handle_Inventory`, `FCk_Handle_Timer`) as references for macro shape.

- [ ] **Step 2: Create the Tier handle**

In `CkGoap_Tier_Fragment_Data.h`: same shape but with `FCk_Handle_Goap_Tier`. Include the same dependencies.

- [ ] **Step 3: Add both handles to the AS dynamic-handle registry**

Open the Editor (after build) → run `UCkDynamicHandleSubsystem::GenerateHandleTypeRegistry()` via the Editor Subsystems panel button. Confirm `DynamicHandleTypes.json` includes both new types. (Editor restart needed before AS sees them — covered in Phase 2 Task 2.6.)

- [ ] **Step 4: Compile verify**

Build using the Bash command from Task 0.2 Step 5. Expected: clean build (handles compile, nothing uses them yet).

- [ ] **Step 5: Commit**

```powershell
cd "D:\Repos\CkPlugins\Plugins\CkFoundation"
git add Source/CkGoap/Public/CkGoap/Bundle/ Source/CkGoap/Public/CkGoap/Tier/
git commit -m "feat(CkGoap): introduce FCk_Handle_Goap_Bundle and _Tier typesafe handles"
```

### Task 1.2: Define `FCk_GoapWS_Condition_Authored`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Fragment_Data.h`

- [ ] **Step 1: Add the authored-condition struct near the top of the file (before any planner-specific types)**

```cpp
// Public, BlueprintType-friendly wrapper for declaring goal/precondition entries
// in editor or AngelScript. Setup-time resolves these against the WS registry.
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_GoapWS_Condition_Authored
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_GoapWS_Condition_Authored);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories = "Goap"))
    FGameplayTag _Key;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _Value = false;

public:
    CK_PROPERTY_GET(_Key);
    CK_PROPERTY_GET(_Value);
    CK_DEFINE_CONSTRUCTORS(FCk_GoapWS_Condition_Authored, _Key, _Value);
};
```

- [ ] **Step 2: Compile-verify**

Build. Expected: clean (it's a new type with no consumers yet).

- [ ] **Step 3: Commit**

```powershell
git add Source/CkGoap/Public/CkGoap/CkGoap_Fragment_Data.h
git commit -m "feat(CkGoap): add FCk_GoapWS_Condition_Authored public condition struct"
```

### Task 1.3: Add `_ActionTag` + `SetActionTag` to `UCk_GoapAction_EntityScript`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.cpp`

- [ ] **Step 1: Add the field + builder to the header**

In the BUILDERS section of `CkGoapAction_EntityScript.h`, after `SetCost`:

```cpp
    UFUNCTION(BlueprintCallable, Category = "Ck|GOAP|Action",
        DisplayName = "[Ck][GOAP] Set Action Tag")
    void
    SetActionTag(FGameplayTag InTag);
```

In the DATA section, after `_Cost`:

```cpp
    FGameplayTag _ActionTag;

public:
    auto Get_ActionTag() const -> FGameplayTag { return _ActionTag; }
```

(`Get_ActionTag` needs a public accessor — ChainUpdate reads it via the CDO.)

- [ ] **Step 2: Implement `SetActionTag` in the cpp**

```cpp
auto
    UCk_GoapAction_EntityScript::
    SetActionTag(FGameplayTag InTag) -> void
{
    _ActionTag = InTag;
}
```

- [ ] **Step 3: Compile-verify**

Build. Expected: clean.

- [ ] **Step 4: Commit**

```powershell
git add Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.h Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.cpp
git commit -m "feat(CkGoap): add _ActionTag + SetActionTag to UCk_GoapAction_EntityScript"
```

### Task 1.4: Define `FCk_Fragment_Goap_BundleParamsData`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Fragment_Data.h`

- [ ] **Step 1: Add the params struct after the handle**

```cpp
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_BundleParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_BundleParamsData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories = "Goap.Bundle"))
    FGameplayTag _BundleTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _InitialToggle = ECk_EnableDisable::Enable;

public:
    CK_PROPERTY_GET(_BundleTag);
    CK_PROPERTY(_InitialToggle);
    CK_DEFINE_CONSTRUCTORS(FCk_Fragment_Goap_BundleParamsData, _BundleTag);
};
```

Use existing `ECk_EnableDisable` from `CkCore/Public/CkCore/Enums/Ck_Enums.h` (or equivalent — verify path by greppping if not obvious).

- [ ] **Step 2: Compile + commit**

```powershell
git commit -m "feat(CkGoap): add FCk_Fragment_Goap_BundleParamsData"
```

### Task 1.5: Define `FCk_Fragment_Goap_TierParamsData`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Fragment_Data.h`

- [ ] **Step 1: Add the params struct after the Tier handle**

```cpp
#include "CkGoap/CkGoap_Fragment_Data.h"   // for FCk_GoapWS_Condition_Authored, ECk_Goap_ReplanPolicy
#include "CkGoap/WorldState/CkGoap_WorldState_Fragment_Data.h"  // for FCk_Handle_Goap_WorldState

USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_TierParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_TierParamsData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories = "Goap.Tier"))
    FGameplayTag _TierTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Handle_Goap_WorldState _WorldStateSource_Override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FCk_GoapWS_Condition_Authored> _InitialGoal_RootOnly;

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

`ECk_Goap_ReplanPolicy` already exists in `CkGoap_Fragment_Data.h` — verify import resolves.

- [ ] **Step 2: Compile + commit**

```powershell
git commit -m "feat(CkGoap): add FCk_Fragment_Goap_TierParamsData"
```

### Task 1.6: Define `FCk_Fragment_Goap_RootParamsData`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Fragment_Data.h`

- [ ] **Step 1: Add the new (empty) params struct**

```cpp
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Fragment_Goap_RootParamsData
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_Goap_RootParamsData);

    // Reserved for future global tuning. v1 has no required fields.
};
```

- [ ] **Step 2: Compile + commit**

```powershell
git commit -m "feat(CkGoap): add FCk_Fragment_Goap_RootParamsData (placeholder for future tuning)"
```

### Task 1.7: Define Bundle ECS fragments

**Files:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Fragment.h`
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Fragment_Data.cpp`

- [ ] **Step 1: Create the fragment header**

```cpp
#pragma once

#include "CkEcs/CkEcs.h"
#include "CkGoap/Bundle/CkGoap_Bundle_Fragment_Data.h"
#include "CkGoap/Tier/CkGoap_Tier_Fragment_Data.h"
#include "CkRecord/Record/CkRecord_Fragment.h"

#include "CkGoap_Bundle_Fragment.generated.h"

namespace ck
{
    // ECS-side fragment aliases.
    using FFragment_Goap_Bundle_Params = FCk_Fragment_Goap_BundleParamsData;

    // Per-bundle live state.
    struct CKGOAP_API FFragment_Goap_Bundle_Current
    {
        CK_GENERATED_BODY(FFragment_Goap_Bundle_Current);
        friend class FProcessor_Goap_Bundle_ChainUpdate;
        friend class UCk_Utils_Goap_Bundle_UE;

    private:
        ECk_EnableDisable _EnableToggle = ECk_EnableDisable::Enable;
        TArray<FString>   _DependencyCycles;

    public:
        CK_PROPERTY_GET(_EnableToggle);
        CK_PROPERTY_GET(_DependencyCycles);
    };

    // Ordered active-tier chain (root at [0]).
    struct CKGOAP_API FFragment_Goap_Bundle_ActiveTiers
    {
        CK_GENERATED_BODY(FFragment_Goap_Bundle_ActiveTiers);
        friend class FProcessor_Goap_Bundle_ChainUpdate;
        friend class UCk_Utils_Goap_Bundle_UE;

    private:
        TArray<FCk_Handle_Goap_Tier> _Tiers;

    public:
        CK_PROPERTY_GET(_Tiers);
    };

    // O(1) tag→tier catalog lookup, populated at AddTier time.
    struct CKGOAP_API FFragment_Goap_Bundle_TierCatalogIndex
    {
        CK_GENERATED_BODY(FFragment_Goap_Bundle_TierCatalogIndex);
        friend class FProcessor_Goap_Bundle_ChainUpdate;
        friend class UCk_Utils_Goap_Bundle_UE;

    private:
        TMap<FGameplayTag, FCk_Handle_Goap_Tier> _TagToTier;

    public:
        CK_PROPERTY_GET(_TagToTier);
    };

    // Tag set after any tier in the bundle plans; consumed (and removed) by
    // ChainUpdate. Optimization to avoid walking inert bundles each frame.
    CK_DEFINE_ECS_TAG(FTag_Goap_Bundle_RequiresChainUpdate);
}

// Record-of-entities: the static tier catalog held by each bundle.
CK_DEFINE_RECORD_OF_ENTITIES(FFragment_RecordOfGoapTiers, FCk_Handle_Goap_Tier);
```

- [ ] **Step 2: Create `.cpp` (constructors live in cpp per CkFoundation style)**

```cpp
#include "CkGoap/Bundle/CkGoap_Bundle_Fragment_Data.h"

// Constructor implementations are macro-generated; this .cpp is here so the
// USTRUCTs participate in the .Build.cs's source list. Leave the body empty
// or add gameplay tag UE_DEFINE_GAMEPLAY_TAG_STATIC entries here (TBD if
// Goap.Bundle category needs static tag declarations).
```

(If no static gameplay-tag definitions are needed in this file, the cpp may just include the header and otherwise be empty — that's still required for the Build.cs to compile the USTRUCT vtable.)

- [ ] **Step 3: Compile + commit**

```powershell
git add Source/CkGoap/Public/CkGoap/Bundle/
git commit -m "feat(CkGoap): add Bundle ECS fragments (Params, Current, ActiveTiers, CatalogIndex)"
```

### Task 1.8: Define Tier ECS fragments

**Files:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Fragment.h`
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Fragment_Data.cpp`

- [ ] **Step 1: Create the fragment header**

```cpp
#pragma once

#include "CkEcs/CkEcs.h"
#include "CkGoap/Tier/CkGoap_Tier_Fragment_Data.h"
#include "CkGoap/CkGoap_Fragment_Data.h"
#include "CkGoap/Algorithm/CkGoap_Types.h"
#include "CkGoap/Algorithm/CkGoap_Graph.h"
#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkAStar/CkAStar_Fragment.h"

#include <variant>

#include "CkGoap_Tier_Fragment.generated.h"

namespace ck
{
    using FFragment_Goap_Tier_Params = FCk_Fragment_Goap_TierParamsData;

    // Live tier state (mirrors today's FFragment_Goap_Current shape, per-tier).
    struct CKGOAP_API FFragment_Goap_Tier_Current
    {
        CK_GENERATED_BODY(FFragment_Goap_Tier_Current);
        friend class FProcessor_Goap_Tier_Setup;
        friend class FProcessor_Goap_Tier_HandleRequests;
        friend class FProcessor_Goap_Tier_HandleResult;
        friend class FProcessor_Goap_Tier_AutoReplan;
        friend class FProcessor_Goap_Bundle_ChainUpdate;
        friend class UCk_Utils_Goap_Tier_UE;
        friend class UCk_Utils_Goap_Bundle_UE;

    private:
        FCk_Handle_Goap_WorldState                              _WorldStateSource_Resolved;
        TArray<goap::FWorldStateCondition>                      _Goal;
        TArray<FCk_GoapWS_Condition_Authored>                   _InvalidGoal;
        TArray<TSubclassOf<UCk_GoapAction_EntityScript>>        _Plan;
        float                                                   _PlanCost = 0.0f;
        ECk_GoapPlanStatus                                      _PlanStatus = ECk_GoapPlanStatus::Idle;
        int32                                                   _PlanAttemptCount = 0;
        TSubclassOf<UCk_GoapAction_EntityScript>                _ActiveParentAction;

    public:
        CK_PROPERTY_GET(_WorldStateSource_Resolved);
        CK_PROPERTY_GET(_Goal);
        CK_PROPERTY_GET(_InvalidGoal);
        CK_PROPERTY_GET(_Plan);
        CK_PROPERTY_GET(_PlanCost);
        CK_PROPERTY_GET(_PlanStatus);
        CK_PROPERTY_GET(_PlanAttemptCount);
        CK_PROPERTY_GET(_ActiveParentAction);
    };

    // Class list registered via AddAction; consumed once by Setup.
    struct CKGOAP_API FFragment_Goap_Tier_ActionClasses
    {
        CK_GENERATED_BODY(FFragment_Goap_Tier_ActionClasses);
        friend class FProcessor_Goap_Tier_Setup;
        friend class UCk_Utils_Goap_Tier_UE;

    private:
        TArray<TSubclassOf<UCk_GoapAction_EntityScript>> _Classes;

    public:
        CK_PROPERTY_GET(_Classes);
    };

    // CDO-extracted action defs (same shape as today's FFragment_Goap_Actions).
    struct CKGOAP_API FFragment_Goap_Tier_Actions
    {
        CK_GENERATED_BODY(FFragment_Goap_Tier_Actions);
        friend class FProcessor_Goap_Tier_Setup;
        friend class FProcessor_Goap_Tier_HandleRequests;
        friend class FProcessor_Goap_Tier_HandleResult;

    private:
        TArray<goap::FActionDef> _ActionDefs;

    public:
        CK_PROPERTY_GET(_ActionDefs);
    };

    // Request types — per tier.
    struct CKGOAP_API FCk_Request_Goap_Tier_Plan : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_Plan);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_Plan);
    };

    struct CKGOAP_API FCk_Request_Goap_Tier_CancelPlan : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_CancelPlan);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_CancelPlan);
    };

    struct CKGOAP_API FCk_Request_Goap_Tier_SetGoal : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_SetGoal);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_SetGoal);
        friend class FProcessor_Goap_Tier_HandleRequests;

    private:
        TArray<FCk_GoapWS_Condition_Authored> _Goal;

    public:
        CK_PROPERTY_GET(_Goal);
        CK_DEFINE_CONSTRUCTORS(FCk_Request_Goap_Tier_SetGoal, _Goal);
    };

    struct CKGOAP_API FCk_Request_Goap_Tier_SetActionCost : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_SetActionCost);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_SetActionCost);
        friend class FProcessor_Goap_Tier_HandleRequests;

    private:
        TSubclassOf<UCk_GoapAction_EntityScript> _ActionClass;
        float                                    _Cost = 0.0f;

    public:
        CK_PROPERTY_GET(_ActionClass);
        CK_PROPERTY_GET(_Cost);
        CK_DEFINE_CONSTRUCTORS(FCk_Request_Goap_Tier_SetActionCost, _ActionClass, _Cost);
    };

    struct CKGOAP_API FCk_Request_Goap_Tier_SetReplanInterval : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_SetReplanInterval);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_SetReplanInterval);
        friend class FProcessor_Goap_Tier_HandleRequests;

    private:
        float _Seconds = 0.0f;

    public:
        CK_PROPERTY_GET(_Seconds);
        CK_DEFINE_CONSTRUCTORS(FCk_Request_Goap_Tier_SetReplanInterval, _Seconds);
    };

    struct CKGOAP_API FCk_Request_Goap_Tier_SetReplanPolicy : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_SetReplanPolicy);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_SetReplanPolicy);
        friend class FProcessor_Goap_Tier_HandleRequests;

    private:
        ECk_Goap_ReplanPolicy _Policy = ECk_Goap_ReplanPolicy::OnWorldStateDirty;

    public:
        CK_PROPERTY_GET(_Policy);
        CK_DEFINE_CONSTRUCTORS(FCk_Request_Goap_Tier_SetReplanPolicy, _Policy);
    };

    struct CKGOAP_API FCk_Request_Goap_Tier_SetSearchBudget : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_SetSearchBudget);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_SetSearchBudget);
        friend class FProcessor_Goap_Tier_HandleRequests;

    private:
        float _Microseconds = 1000.0f;

    public:
        CK_PROPERTY_GET(_Microseconds);
        CK_DEFINE_CONSTRUCTORS(FCk_Request_Goap_Tier_SetSearchBudget, _Microseconds);
    };

    struct CKGOAP_API FCk_Request_Goap_Tier_SetCostThreshold : public FRequest_Base
    {
        CK_GENERATED_BODY(FCk_Request_Goap_Tier_SetCostThreshold);
        CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Goap_Tier_SetCostThreshold);
        friend class FProcessor_Goap_Tier_HandleRequests;

    private:
        float _Threshold = 1.0e9f;

    public:
        CK_PROPERTY_GET(_Threshold);
        CK_DEFINE_CONSTRUCTORS(FCk_Request_Goap_Tier_SetCostThreshold, _Threshold);
    };

    struct CKGOAP_API FFragment_Goap_Tier_Requests
    {
        CK_GENERATED_BODY(FFragment_Goap_Tier_Requests);
        friend class FProcessor_Goap_Tier_HandleRequests;
        friend class FProcessor_Goap_Tier_AutoReplan;
        friend class UCk_Utils_Goap_Tier_UE;

    public:
        using RequestType = std::variant<
            FCk_Request_Goap_Tier_Plan,
            FCk_Request_Goap_Tier_CancelPlan,
            FCk_Request_Goap_Tier_SetGoal,
            FCk_Request_Goap_Tier_SetActionCost,
            FCk_Request_Goap_Tier_SetReplanInterval,
            FCk_Request_Goap_Tier_SetReplanPolicy,
            FCk_Request_Goap_Tier_SetSearchBudget,
            FCk_Request_Goap_Tier_SetCostThreshold>;

    private:
        TArray<RequestType> _Requests;
    };

    // Per-tier replan throttle (same shape as today, just moved to tier scope).
    struct CKGOAP_API FFragment_Goap_Tier_ReplanThrottle
    {
        CK_GENERATED_BODY(FFragment_Goap_Tier_ReplanThrottle);
        friend class FProcessor_Goap_Tier_AutoReplan;
        friend class FProcessor_Goap_Tier_HandleRequests;

    private:
        float _SecondsSinceLastReplan = 0.0f;
        float _MinReplanIntervalSeconds = 0.0f;

    public:
        CK_PROPERTY_GET(_SecondsSinceLastReplan);
        CK_PROPERTY_GET(_MinReplanIntervalSeconds);
    };

    // A* fragments — per tier. Same template params as today's FFragment_Goap_SearchState.
    using FFragment_Goap_Tier_SearchState = TFragment_AStar_SearchState<int32, goap::FGoapGraph>;
    using FFragment_Goap_Tier_Result      = TFragment_AStar_Result<int32>;

    struct CKGOAP_API FFragment_Goap_Tier_PlanContext
    {
        CK_GENERATED_BODY(FFragment_Goap_Tier_PlanContext);
        friend class FProcessor_Goap_Tier_HandleRequests;
        friend class FProcessor_Goap_Tier_HandleResult;

    private:
        goap::FGoapGraph _Graph;

    public:
        CK_PROPERTY_GET(_Graph);
    };

    // Tier-scope ECS tags.
    CK_DEFINE_ECS_TAG(FTag_Goap_Tier_RequiresSetup);
    CK_DEFINE_ECS_TAG(FTag_Goap_Tier_RequiresInitialPlan);
    CK_DEFINE_ECS_TAG(FTag_Goap_Tier_PlanRequested);
    // FTag_Goap_Dirty_WorldState / FTag_Goap_Dirty_Cost exist already in
    // CkGoap_Fragment.h — they retarget tiers in the new model without
    // changing definition.
}
```

- [ ] **Step 2: Create the cpp (empty body — see Task 1.7 Step 2)**

- [ ] **Step 3: Compile + commit**

```powershell
git add Source/CkGoap/Public/CkGoap/Tier/
git commit -m "feat(CkGoap): add Tier ECS fragments + per-tier request types"
```

### Task 1.9: Define `FFragment_RecordOfGoapBundles` and `FFragment_Goap_Root_Params`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Fragment.h`

- [ ] **Step 1: Add at the top of `CkGoap_Fragment.h` (after existing includes)**

```cpp
#include "CkGoap/Bundle/CkGoap_Bundle_Fragment_Data.h"
```

Then at the bottom (or near existing record definitions if any):

```cpp
namespace ck
{
    using FFragment_Goap_Root_Params = FCk_Fragment_Goap_RootParamsData;
}

CK_DEFINE_RECORD_OF_ENTITIES(FFragment_RecordOfGoapBundles, FCk_Handle_Goap_Bundle);
```

- [ ] **Step 2: Compile + commit**

```powershell
git commit -m "feat(CkGoap): add FFragment_RecordOfGoapBundles + Root_Params alias"
```

### Task 1.10: Define new signals

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Fragment.h` (or a dedicated `CkGoap_Signals.h` if cleaner — check existing pattern; CkGoap currently colocates signals in fragment header)

- [ ] **Step 1: Add tier-scoped signals (replacing old planner-scoped equivalents)**

```cpp
CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanComplete,
    FCk_Delegate_Goap_OnPlanComplete,
    FCk_Handle_Goap_Tier,
    TArray<TSubclassOf<UCk_GoapAction_EntityScript>>,
    float);

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnPlanFailed,
    FCk_Delegate_Goap_OnPlanFailed,
    FCk_Handle_Goap_Tier);

CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(
    CKGOAP_API,
    Goap_OnActiveTiersChanged,
    FCk_Delegate_Goap_OnActiveTiersChanged,
    FCk_Handle_Goap_Bundle,
    TArray<FCk_Handle_Goap_Tier>);

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

The old `OnGoapPlanComplete` / `OnGoapPlanFailed` (planner-scoped) get removed in Phase 2 — keeping both compiles for now is OK if symbol-name collisions don't occur. If they do, prefix this batch with `_Tier` (e.g. `Goap_OnTierPlanComplete`) until Phase 2 deletes the old ones, then rename.

- [ ] **Step 2: Compile + commit**

```powershell
git commit -m "feat(CkGoap): add Bundle/Tier-scoped signals"
```

### Task 1.11: End-of-phase build/test gate

- [ ] **Step 1: Wait for editor freedom + run build-test workflow**

Run toolbox `build-test` (after editor unlocks).

- [ ] **Step 2: Verify**

Expected: full build green; 5 baseline GOAP tests pass (those don't depend on the new types and haven't been removed). 12 HGOAP tests already gone.

- [ ] **Step 3: Snapshot commit (optional consolidation if Phase 1 had many tiny commits)**

```powershell
git log --oneline -20
# If acceptable, no consolidation needed.
```

---

## Phase 2 — Old API removal + AS regen

Strip the planner-per-entity surface. Existing 5 baseline tests will break; they get reworked in Phase 7.

### Task 2.1: Identify all old-API references

**Files:**
- All of `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Utils.{h,cpp}`
- `CkGoap_Processor.{h,cpp}`
- `CkGoap_Fragment.h`
- `CkGoap_Fragment_Data.h`
- `EntityScripts/CkGoapGoal_EntityScript.{h,cpp}` (delete entirely)

- [ ] **Step 1: Inventory removals**

Run Grep / Glob:
```
Grep "FProcessor_Goap_Setup|FProcessor_Goap_HandleRequests|FProcessor_Goap_Execute|FProcessor_Goap_HandleResult|FProcessor_Goap_AutoReplan|FProcessor_Goap_EndPlay" in Plugins/CkFoundation/Source/CkGoap/
```

These five processors all need to be removed (their replacements live in Tier/ subfolder).

- [ ] **Step 2: List old utils verbs to remove**

Read `CkGoap_Utils.h` and inventory: `Create`, `AddAction`/`AddGoal` on planner, `Set_WorldStateValue` / `Get_WorldStateValue` / `Has_WorldStateKey` on planner (the WS-handle versions in `WorldState/CkGoap_WorldState_Utils.h` survive), `Set_ActionCost` / `Get_ActionCost`, `Request_PlanForGoal`, and all the others bound to `FCk_Handle_Goap` as a planner.

`Add` survives but reshapes (Task 3.1).

### Task 2.2: Delete `UCk_GoapGoal_EntityScript`

**Files:**
- Delete: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapGoal_EntityScript.h`
- Delete: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapGoal_EntityScript.cpp`

- [ ] **Step 1: Find references**

Run Grep for `UCk_GoapGoal_EntityScript` + `CkGoapGoal_EntityScript` across the repo. Note all files that include or reference.

- [ ] **Step 2: Delete the files**

```powershell
cd "D:\Repos\CkPlugins\Plugins\CkFoundation"
git rm Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapGoal_EntityScript.h Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapGoal_EntityScript.cpp
```

- [ ] **Step 3: Remove `#include` lines + any `TSubclassOf<UCk_GoapGoal_EntityScript>` declarations from CkGoap_Utils, CkGoap_Processor, CkGoap_Fragment_Data, CkGoap_Fragment.**

The compiler will tell you exactly where after the delete; chase each one down by including the file and removing the relevant declarations.

- [ ] **Step 4: Don't compile yet — wait until Task 2.3 also lands so the build can settle.**

### Task 2.3: Strip planner-specific fragments from `CkGoap_Fragment_Data.h` and `CkGoap_Fragment.h`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Fragment_Data.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Fragment.h`

- [ ] **Step 1: Read current `CkGoap_Fragment_Data.h` to know what to keep**

KEEP: `FCk_GoapWS_Condition_Authored` (added Phase 1), `FCk_Fragment_Goap_RootParamsData` (Phase 1), `FCk_Handle_Goap` (repurposed — still a typesafe handle, just now means "root container"), `ECk_GoapPlanStatus` enum, `ECk_Goap_ReplanPolicy` enum, `FCk_Goap_Payload_OnPlanComplete` / `FCk_Goap_Payload_OnPlanFailed` (used by signals — verify; if no longer needed, remove).

REMOVE: `FCk_Fragment_Goap_ParamsData` (the old planner params — replaced by `FCk_Fragment_Goap_RootParamsData`), any planner-specific request types (`FCk_Request_Goap_Plan`, `FCk_Request_Goap_CancelPlan`, `FCk_Request_Goap_SetActionCost`, `FCk_Request_Goap_SetReplanInterval`, `FCk_Request_Goap_SetReplanPolicy`, `FCk_Request_Goap_SetSearchBudget`, `FCk_Request_Goap_SetCostThreshold`), and old planner state structs.

- [ ] **Step 2: Strip `CkGoap_Fragment.h`**

KEEP: dirty tags (`FTag_Goap_Dirty_WorldState`, `FTag_Goap_Dirty_Cost`), `FFragment_RecordOfGoapBundles` (Phase 1), `FFragment_Goap_Root_Params` alias (Phase 1), the new signals (Phase 1).

REMOVE: old planner fragments (`FFragment_Goap_Params`, `FFragment_Goap_KeyRegistry`, `FFragment_Goap_WorldState` if that name's used as a per-planner fragment, `FFragment_Goap_ActionClasses`, `FFragment_Goap_GoalClasses`, `FFragment_Goap_Actions`, `FFragment_Goap_Goals`, `FFragment_Goap_Current`, `FFragment_Goap_Requests`, `FFragment_Goap_ReplanThrottle`, `FFragment_Goap_Diagnostics`, `FFragment_Goap_PlanContext`, `FFragment_Goap_SearchState`, `FFragment_Goap_Result`), old planner-scoped tags (`FTag_Goap_RequiresSetup`, `FTag_Goap_RequiresInitialPlan`, `FTag_Goap_PlanRequested`), old signals (`OnGoapPlanComplete`, `OnGoapPlanFailed`).

**Caveat:** `FFragment_Goap_WorldState_KeyRegistry`, `FFragment_Goap_WorldState_Values`, `FFragment_Goap_WorldState_Subscribers` (in `WorldState/`) all SURVIVE. Don't touch those.

### Task 2.4: Strip processors from `CkGoap_Processor.{h,cpp}`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Processor.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Processor.cpp`

- [ ] **Step 1: Remove all five planner-scoped processors**

`FProcessor_Goap_Setup`, `FProcessor_Goap_HandleRequests`, `FProcessor_Goap_Execute`, `FProcessor_Goap_HandleResult`, `FProcessor_Goap_AutoReplan`, `FProcessor_Goap_EndPlay`. Remove their declarations from the header and bodies + `CK_REGISTER_PROCESSOR` calls from the cpp.

After this, `CkGoap_Processor.{h,cpp}` should be effectively empty (or near-empty, just module-include + a CKGOAP_API export marker).

- [ ] **Step 2: If the files end up empty, decide whether to keep them as empty stubs (for future module-wide processors) or delete them**

Keep them. They'll be useful as the home of any future root-level processors (e.g. if a root-aggregation diagnostic processor lands).

### Task 2.5: Strip utils from `CkGoap_Utils.{h,cpp}`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Utils.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Utils.cpp`

- [ ] **Step 1: Remove all old verbs**

`Create`, `AddAction` (on Goap handle), `AddGoal`, `Set_WorldStateValue` (on Goap handle), `Get_WorldStateValue`, `Has_WorldStateKey` (on Goap handle), `Set_ActionCost`, `Get_ActionCost`, `Request_Plan`, `Request_PlanForGoal`, `Request_CancelPlan`, `Request_SetReplanInterval`, `Request_SetReplanPolicy`, `Request_SetSearchBudget`, `Request_SetCostThreshold`, `Has`, `Find_Goap`, `Find_GoapByName`, `Get_PlanStatus`, `Get_Plan`, `Get_PlanCost`, `BindTo_OnPlanComplete`, `BindTo_OnPlanFailed`, `UnbindFrom_*`, `Get_DependencyCycles` (on Goap handle), `Get_LastUnreachableGoalConditions`, `Has_DiagnosticWarnings`.

`Add` — keep declaration/signature for Phase 3 to fill in. Stub body to return empty handle if needed.

- [ ] **Step 2: Compile + verify**

Build. Expected: clean (no consumers of removed verbs after Phase 0 cleanup). If compile fails, the failures point at remaining consumers — chase them down.

- [ ] **Step 3: Commit**

```powershell
git add -A
git commit -m "refactor(CkGoap): remove planner-per-entity API (utils, processors, fragments, GoalEntityScript)"
```

### Task 2.6: Regenerate AS dynamic-handle registry

- [ ] **Step 1: Open the editor**

Make sure the editor opens cleanly after Phase 2 cuts.

- [ ] **Step 2: Run `UCkDynamicHandleSubsystem::GenerateHandleTypeRegistry`**

In the editor: Settings → Editor Subsystems → `CkDynamicHandleSubsystem` → click `GenerateHandleTypeRegistry`. Confirm `Plugins/CkFoundation/Script/Generated/DynamicHandleTypes.json` updates to include `FCk_Handle_Goap_Bundle` + `FCk_Handle_Goap_Tier`.

- [ ] **Step 3: Close + reopen editor for AS bindings to pick up the new handles**

(Or use the `ForceRefreshDynamicHandleBindings` button — both work for dev iteration.)

- [ ] **Step 4: Commit**

```powershell
cd "D:\Repos\CkPlugins\Plugins\CkFoundation"
git add Script/Generated/DynamicHandleTypes.json
git commit -m "chore(CkGoap): regen AS dynamic-handle registry with Bundle/Tier"
```

---

## Phase 3 — New API implementation

Build the imperative construction + query verbs.

### Task 3.1: Implement `UCk_Utils_Goap_UE::Add` (root-only)

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Utils.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Utils.cpp`

- [ ] **Step 1: Update Utils class declaration**

```cpp
UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle"))
class CKGOAP_API UCk_Utils_Goap_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_Utils_Goap_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_Goap);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
              DisplayName = "[Ck][Goap] Add")
    static FCk_Handle_Goap
    Add(UPARAM(ref) FCk_Handle& InOwner,
        const FCk_Fragment_Goap_RootParamsData& InParams);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
              DisplayName = "[Ck][Goap] Has")
    static bool
    Has(const FCk_Handle& InHandle);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
              DisplayName = "[Ck][Goap] Cast")
    static FCk_Handle_Goap
    Cast(const FCk_Handle& InHandle);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
              DisplayName = "[Ck][Goap] Find Bundle")
    static FCk_Handle_Goap_Bundle
    Find_Bundle(const FCk_Handle_Goap& InGoap, FGameplayTag InBundleTag);
};
```

- [ ] **Step 2: Implement in the cpp**

```cpp
auto UCk_Utils_Goap_UE::Add(
    FCk_Handle& InOwner,
    const FCk_Fragment_Goap_RootParamsData& InParams)
    -> FCk_Handle_Goap
{
    CK_ENSURE_IF_NOT(ck::IsValid(InOwner),
        TEXT("Invalid owner handle [{}].{}"), InOwner, ck::Context(this))
    { return {}; }

    // Spawn the Goap root entity as a child of the owner.
    auto GoapEntity = UCk_Utils_EntityLifetime_UE::Request_SpawnEntity(
        InOwner, FCk_Request_EntityLifetime_SpawnEntity{});

    GoapEntity.Add<ck::FFragment_Goap_Root_Params>(InParams);
    GoapEntity.Add<ck::FFragment_RecordOfGoapBundles>();

    return Cast(GoapEntity);
}

auto UCk_Utils_Goap_UE::Has(const FCk_Handle& InHandle) -> bool
{
    return ck::IsValid(InHandle) &&
        InHandle.Has<ck::FFragment_Goap_Root_Params>();
}

auto UCk_Utils_Goap_UE::Cast(const FCk_Handle& InHandle) -> FCk_Handle_Goap
{
    auto Result = FCk_Handle_Goap{};
    Result.CopyFrom(InHandle);
    return Result;
}

auto UCk_Utils_Goap_UE::Find_Bundle(
    const FCk_Handle_Goap& InGoap, FGameplayTag InBundleTag)
    -> FCk_Handle_Goap_Bundle
{
    if (NOT ck::IsValid(InGoap)) { return {}; }

    auto Result = FCk_Handle_Goap_Bundle{};
    UCk_Utils_RecordOfEntities_UE::ForEach<ck::FFragment_RecordOfGoapBundles>(
        InGoap, [&](FCk_Handle Bundle)
    {
        const auto& Params = Bundle.Get<ck::FFragment_Goap_Bundle_Params>();
        if (Params.Get_BundleTag() == InBundleTag)
        {
            Result.CopyFrom(Bundle);
        }
    });
    return Result;
}
```

The exact spawn-entity / record-add APIs may differ — check `CkEcsExt/EntityLifetime/CkEntityLifetime_Utils.h` and existing record consumers (`CkInventory` is a good model) for the right call.

- [ ] **Step 3: Compile + commit**

```powershell
git commit -m "feat(CkGoap): implement Add/Has/Cast/Find_Bundle root-level verbs"
```

### Task 3.2: Implement `UCk_Utils_Goap_Bundle_UE::AddBundle`

**Files:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Utils.h`
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Utils.cpp`

- [ ] **Step 1: Header**

```cpp
#pragma once

#include "CkGoap/Bundle/CkGoap_Bundle_Fragment_Data.h"
#include "CkGoap/CkGoap_Fragment_Data.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CkGoap_Bundle_Utils.generated.h"

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_Goap_Bundle"))
class CKGOAP_API UCk_Utils_Goap_Bundle_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_Utils_Goap_Bundle_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_Goap_Bundle);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Bundle",
              DisplayName = "[Ck][Goap][Bundle] Add Bundle")
    static FCk_Handle_Goap_Bundle
    AddBundle(UPARAM(ref) FCk_Handle_Goap& InGoap,
              const FCk_Fragment_Goap_BundleParamsData& InParams);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Bundle",
              DisplayName = "[Ck][Goap][Bundle] Find Tier")
    static FCk_Handle_Goap_Tier
    Find_Tier(const FCk_Handle_Goap_Bundle& InBundle, FGameplayTag InTierTag);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Bundle",
              DisplayName = "[Ck][Goap][Bundle] Get Active Tiers")
    static TArray<FCk_Handle_Goap_Tier>
    Get_ActiveTiers(const FCk_Handle_Goap_Bundle& InBundle);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Bundle",
              DisplayName = "[Ck][Goap][Bundle] Request Set Enable Toggle")
    static FCk_Handle_Goap_Bundle
    Request_SetEnableToggle(UPARAM(ref) FCk_Handle_Goap_Bundle& InBundle,
                            ECk_EnableDisable InToggle);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Bundle",
              DisplayName = "[Ck][Goap][Bundle] Request Reset Active Tiers")
    static FCk_Handle_Goap_Bundle
    Request_ResetActiveTiers(UPARAM(ref) FCk_Handle_Goap_Bundle& InBundle);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|Bundle",
              DisplayName = "[Ck][Goap][Bundle] Get Dependency Cycles")
    static TArray<FString>
    Get_DependencyCycles(const FCk_Handle_Goap_Bundle& InBundle);

    // Bind/Unbind helpers for OnActiveTiersChanged.
    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Bundle BindTo_OnActiveTiersChanged(...);

    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Bundle UnbindFrom_OnActiveTiersChanged(...);
};
```

- [ ] **Step 2: Implement `AddBundle` in the cpp**

```cpp
auto UCk_Utils_Goap_Bundle_UE::AddBundle(
    FCk_Handle_Goap& InGoap,
    const FCk_Fragment_Goap_BundleParamsData& InParams)
    -> FCk_Handle_Goap_Bundle
{
    CK_ENSURE_IF_NOT(ck::IsValid(InGoap),
        TEXT("Invalid Goap handle [{}].{}"), InGoap, ck::Context(this))
    { return {}; }

    // Diagnostic: bundle-tag uniqueness within root.
    if (auto Existing = UCk_Utils_Goap_UE::Find_Bundle(InGoap, InParams.Get_BundleTag());
        ck::IsValid(Existing))
    {
        ck::goap::Warning(TEXT("Bundle with tag [{}] already exists on Goap root [{}]; AddBundle rejected"),
            InParams.Get_BundleTag(), InGoap);
        return {};
    }

    auto BundleEntity = UCk_Utils_EntityLifetime_UE::Request_SpawnEntity(
        InGoap, FCk_Request_EntityLifetime_SpawnEntity{});

    BundleEntity.Add<ck::FFragment_Goap_Bundle_Params>(InParams);
    BundleEntity.Add<ck::FFragment_Goap_Bundle_Current>();
    auto& Current = BundleEntity.Get<ck::FFragment_Goap_Bundle_Current>();
    Current._EnableToggle = InParams.Get_InitialToggle();

    BundleEntity.Add<ck::FFragment_Goap_Bundle_ActiveTiers>();
    BundleEntity.Add<ck::FFragment_Goap_Bundle_TierCatalogIndex>();
    BundleEntity.Add<ck::FFragment_RecordOfGoapTiers>();

    // Add to record on Goap root.
    UCk_Utils_RecordOfEntities_UE::AddTo<ck::FFragment_RecordOfGoapBundles>(
        InGoap, BundleEntity);

    auto Result = FCk_Handle_Goap_Bundle{};
    Result.CopyFrom(BundleEntity);
    return Result;
}
```

The remaining methods (Find_Tier, Get_ActiveTiers, Request_*, etc.) follow standard CkFoundation patterns. Implement them straightforwardly:

- `Find_Tier` — walk the bundle's `FFragment_RecordOfGoapTiers` looking for matching `_TierTag`, or read directly from `FFragment_Goap_Bundle_TierCatalogIndex._TagToTier[InTierTag]`.
- `Get_ActiveTiers` — return `InBundle.Get<FFragment_Goap_Bundle_ActiveTiers>()._Tiers`.
- `Request_SetEnableToggle` / `Request_ResetActiveTiers` — write to fragment directly (or queue if a bundle-request-queue is added; v1 can mutate directly since both are simple boolean/array ops).
- `Get_DependencyCycles` — return `InBundle.Get<FFragment_Goap_Bundle_Current>()._DependencyCycles`.
- `BindTo_OnActiveTiersChanged` — use `CK_SIGNAL_BIND(ck::UUtils_Signal_Goap_OnActiveTiersChanged, ...)`.

- [ ] **Step 3: Compile + commit**

```powershell
git add Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Utils.h Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Utils.cpp
git commit -m "feat(CkGoap): implement Bundle-level utils (AddBundle, Find_Tier, etc.)"
```

### Task 3.3: Implement `UCk_Utils_Goap_Tier_UE::AddTier` + `AddAction`

**Files:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Utils.h`
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Utils.cpp`

- [ ] **Step 1: Header**

```cpp
UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_Goap_Tier"))
class CKGOAP_API UCk_Utils_Goap_Tier_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    CK_GENERATED_BODY(UCk_Utils_Goap_Tier_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_Goap_Tier);

public:
    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Tier AddTier(
        UPARAM(ref) FCk_Handle_Goap_Bundle& InBundle,
        const FCk_Fragment_Goap_TierParamsData& InParams);

    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Tier AddAction(
        UPARAM(ref) FCk_Handle_Goap_Tier& InTier,
        TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);

    // Query verbs.
    UFUNCTION(BlueprintCallable, ...)
    static ECk_GoapPlanStatus Get_PlanStatus(const FCk_Handle_Goap_Tier& InTier);

    UFUNCTION(BlueprintCallable, ...)
    static TArray<TSubclassOf<UCk_GoapAction_EntityScript>>
    Get_Plan(const FCk_Handle_Goap_Tier& InTier);

    UFUNCTION(BlueprintCallable, ...)
    static float Get_PlanCost(const FCk_Handle_Goap_Tier& InTier);

    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_WorldState
    Get_WorldStateSource(const FCk_Handle_Goap_Tier& InTier);

    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Tier
    Get_ParentActiveTier(const FCk_Handle_Goap_Tier& InTier);

    UFUNCTION(BlueprintCallable, ...)
    static TSubclassOf<UCk_GoapAction_EntityScript>
    Get_ActiveParentAction(const FCk_Handle_Goap_Tier& InTier);

    UFUNCTION(BlueprintCallable, ...)
    static TArray<FCk_GoapWS_Condition_Authored>
    Get_InvalidGoal(const FCk_Handle_Goap_Tier& InTier);

    // Request verbs (each writes onto FFragment_Goap_Tier_Requests).
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

    // Signal binders.
    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Tier BindTo_OnPlanComplete(...);
    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Tier BindTo_OnPlanFailed(...);
    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Tier BindTo_OnTierActivated(...);
    UFUNCTION(BlueprintCallable, ...)
    static FCk_Handle_Goap_Tier BindTo_OnTierDeactivated(...);
    // (Plus corresponding UnbindFrom_*.)
};
```

- [ ] **Step 2: Implement `AddTier`**

```cpp
auto UCk_Utils_Goap_Tier_UE::AddTier(
    FCk_Handle_Goap_Bundle& InBundle,
    const FCk_Fragment_Goap_TierParamsData& InParams)
    -> FCk_Handle_Goap_Tier
{
    CK_ENSURE_IF_NOT(ck::IsValid(InBundle),
        TEXT("Invalid bundle handle [{}].{}"), InBundle, ck::Context(this))
    { return {}; }

    // Diagnostic: tier-tag uniqueness within bundle.
    if (auto Existing = UCk_Utils_Goap_Bundle_UE::Find_Tier(InBundle, InParams.Get_TierTag());
        ck::IsValid(Existing))
    {
        ck::goap::Warning(TEXT("Tier with tag [{}] already exists in bundle [{}]; AddTier rejected"),
            InParams.Get_TierTag(), InBundle);
        return {};
    }

    auto TierEntity = UCk_Utils_EntityLifetime_UE::Request_SpawnEntity(
        InBundle, FCk_Request_EntityLifetime_SpawnEntity{});

    TierEntity.Add<ck::FFragment_Goap_Tier_Params>(InParams);
    TierEntity.Add<ck::FFragment_Goap_Tier_Current>();
    TierEntity.Add<ck::FFragment_Goap_Tier_ActionClasses>();
    TierEntity.Add<ck::FFragment_Goap_Tier_Actions>();
    TierEntity.Add<ck::FFragment_Goap_Tier_Requests>();
    TierEntity.Add<ck::FFragment_Goap_Tier_ReplanThrottle>();
    auto& Throttle = TierEntity.Get<ck::FFragment_Goap_Tier_ReplanThrottle>();
    Throttle._MinReplanIntervalSeconds = InParams.Get_MinReplanIntervalSeconds();

    TierEntity.Add<ck::FFragment_Goap_Tier_SearchState>();
    TierEntity.Add<ck::FFragment_Goap_Tier_Result>();
    TierEntity.Add<ck::FFragment_Goap_Tier_PlanContext>();
    TierEntity.Add<FFragment_AStar_Params>();  // CkAStar params

    // Mark for one-shot setup.
    TierEntity.Add<ck::FTag_Goap_Tier_RequiresSetup>();
    if (InParams.Get_PlanOnStart())
    {
        TierEntity.Add<ck::FTag_Goap_Tier_RequiresInitialPlan>();
    }

    auto Result = FCk_Handle_Goap_Tier{};
    Result.CopyFrom(TierEntity);

    // Register in bundle's catalog + tag→tier index.
    UCk_Utils_RecordOfEntities_UE::AddTo<ck::FFragment_RecordOfGoapTiers>(
        InBundle, TierEntity);

    auto& Index = InBundle.Get<ck::FFragment_Goap_Bundle_TierCatalogIndex>();
    Index._TagToTier.Add(InParams.Get_TierTag(), Result);

    // First AddTier on a bundle = root → also append to ActiveTiers.
    auto& ActiveTiers = InBundle.Get<ck::FFragment_Goap_Bundle_ActiveTiers>();
    if (ActiveTiers._Tiers.IsEmpty())
    {
        // Validate root has _WorldStateSource_Override.
        if (NOT ck::IsValid(InParams.Get_WorldStateSource_Override()))
        {
            ck::goap::Warning(
                TEXT("Root tier [{}] in bundle [{}] has no _WorldStateSource_Override; planning will not run."),
                InParams.Get_TierTag(), InBundle);
        }

        ActiveTiers._Tiers.Add(Result);
        // Resolve WS source synchronously for the root.
        auto& Current = Result.Get<ck::FFragment_Goap_Tier_Current>();
        Current._WorldStateSource_Resolved = InParams.Get_WorldStateSource_Override();

        // If root has initial goal, inject it synchronously.
        // (Setup processor resolves the keys later — for now stash raw form
        // and let Setup populate _Goal.)
        // Store the authored form somewhere Setup can read. Easiest: also
        // copy onto the tier's params (already there) and have Setup pick it
        // up. _Goal stays empty until Setup runs.

        // Subscribe to WS.
        if (ck::IsValid(Current._WorldStateSource_Resolved))
        {
            SubscribeTierToWorldState_Static(Result, Current._WorldStateSource_Resolved);
        }
    }

    return Result;
}
```

`SubscribeTierToWorldState_Static` is a helper introduced in Phase 5.

- [ ] **Step 3: Implement `AddAction`**

```cpp
auto UCk_Utils_Goap_Tier_UE::AddAction(
    FCk_Handle_Goap_Tier& InTier,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass)
    -> FCk_Handle_Goap_Tier
{
    CK_ENSURE_IF_NOT(ck::IsValid(InTier),
        TEXT("Invalid tier handle [{}].{}"), InTier, ck::Context(this))
    { return InTier; }

    CK_ENSURE_IF_NOT(ck::IsValid(InActionClass),
        TEXT("Invalid action class on tier [{}].{}"), InTier, ck::Context(this))
    { return InTier; }

    auto& Classes = InTier.Get<ck::FFragment_Goap_Tier_ActionClasses>();
    Classes._Classes.AddUnique(InActionClass);

    // Mark tier as requiring re-setup (so the new action's CDO gets extracted).
    InTier.AddOrGet<ck::FTag_Goap_Tier_RequiresSetup>();

    return InTier;
}
```

- [ ] **Step 4: Implement query + request verbs**

Each one is a straight fragment read or queue append. Follow existing CkGoap patterns (today's `CkGoap_Utils.cpp` has analogous code).

- [ ] **Step 5: Compile + commit**

```powershell
git commit -m "feat(CkGoap): implement Tier-level utils (AddTier, AddAction, query, request verbs)"
```

---

## Phase 4 — Processors

### Task 4.1: Implement `FProcessor_Goap_Tier_Setup`

**Files:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Processor.h`
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/CkGoap_Tier_Processor.cpp`

- [ ] **Step 1: Declare the Setup processor in the header**

```cpp
class CKGOAP_API FProcessor_Goap_Tier_Setup : public ck_exp::TProcessor<
    FProcessor_Goap_Tier_Setup,
    FCk_Handle_Goap_Tier,
    FFragment_Goap_Tier_Params,
    FFragment_Goap_Tier_ActionClasses,
    FFragment_Goap_Tier_Actions,
    FFragment_Goap_Tier_Current,
    FTag_Goap_Tier_RequiresSetup,
    CK_IGNORE_PENDING_KILL>
{
public:
    using TProcessor::TProcessor;

    auto ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_Goap_Tier_Params& InParams,
        const FFragment_Goap_Tier_ActionClasses& InClasses,
        FFragment_Goap_Tier_Actions& InActions,
        FFragment_Goap_Tier_Current& InCurrent) const -> void;
};
```

- [ ] **Step 2: Implement Setup body in the cpp**

Port the logic from today's `FProcessor_Goap_Setup::ForEachEntity` (`CkGoap_Processor.cpp:68-220`) but retarget to per-tier:

```cpp
CK_REGISTER_PROCESSOR(ck::FProcessor_Goap_Tier_Setup);

auto FProcessor_Goap_Tier_Setup::ForEachEntity(
    TimeType, HandleType InHandle,
    const FFragment_Goap_Tier_Params& InParams,
    const FFragment_Goap_Tier_ActionClasses& InClasses,
    FFragment_Goap_Tier_Actions& InActions,
    FFragment_Goap_Tier_Current& InCurrent) const -> void
{
    InHandle.Remove<FTag_Goap_Tier_RequiresSetup>();

    const auto Source = InCurrent._WorldStateSource_Resolved;
    CK_ENSURE_IF_NOT(ck::IsValid(Source),
        TEXT("Tier [{}] Setup failed: _WorldStateSource_Resolved is invalid.{}"),
        InHandle, ck::Context(this))
    { return; }

    auto& SourceRegistry =
        Source.Get<FFragment_Goap_WorldState_KeyRegistry>().Get_MutableRegistry();

    // Pull raw entries out of each action's CDO (same as today, just per-tier scope).
    // Resolve raw entries into FActionDef via SourceRegistry.
    // Write into InActions._ActionDefs.
    //
    // (Mirrors CkGoap_Processor.cpp:106-208 — the Phases 1-3 from the old Setup
    //  carry over directly. Just narrowed to one tier's action list.)

    // Also resolve InParams._InitialGoal_RootOnly (root only) into InCurrent._Goal.
    if (NOT InParams.Get_InitialGoal_RootOnly().IsEmpty())
    {
        // Only meaningful for the root tier. For non-root tiers this list
        // should be empty (we don't enforce — just inject if present).
        for (const auto& Cond : InParams.Get_InitialGoal_RootOnly())
        {
            SourceRegistry.FindOrRegister(Cond.Get_Key());
            const auto Key = SourceRegistry.Find(Cond.Get_Key());
            if (Key != goap::InvalidGoapKey)
            {
                InCurrent._Goal.Add(goap::FWorldStateCondition{Key, Cond.Get_Value()});
            }
        }
    }
}
```

The full porting from today's processor is mostly mechanical. Reference `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/CkGoap_Processor.cpp:64-220` for the CDO-extraction + resolve logic.

- [ ] **Step 3: Commit**

```powershell
git commit -m "feat(CkGoap): add FProcessor_Goap_Tier_Setup (CDO extraction per tier)"
```

### Task 4.2: Implement `FProcessor_Goap_Tier_HandleRequests`

- [ ] **Step 1: Port today's `FProcessor_Goap_HandleRequests` (CkGoap_Processor.cpp:225-453) retargeted to per-tier fragments**

Reuse the variant-dispatch + DoHandleRequest pattern. Each request becomes:

- `FCk_Request_Goap_Tier_Plan` → trigger A* via FFragment_Goap_Tier_SearchState (same as today's Plan request)
- `FCk_Request_Goap_Tier_CancelPlan` → reset search-state
- `FCk_Request_Goap_Tier_SetGoal` → resolve authored conditions via WS registry, update `_Goal` + `_InvalidGoal`
- `FCk_Request_Goap_Tier_SetActionCost` → mutate matching action def's cost
- `FCk_Request_Goap_Tier_SetReplanInterval` → update throttle
- etc.

- [ ] **Step 2: Register processor + commit**

```powershell
git commit -m "feat(CkGoap): add FProcessor_Goap_Tier_HandleRequests"
```

### Task 4.3: Implement `FProcessor_Goap_Tier_HandleResult`

- [ ] **Step 1: Port `FProcessor_Goap_HandleResult` (CkGoap_Processor.cpp:459-535) retargeted to per-tier**

Key change: `UUtils_Signal_OnGoapPlanComplete::Broadcast(InTierHandle, ...)` (tier-handle signal, not planner-handle).

- [ ] **Step 2: Register + commit**

```powershell
git commit -m "feat(CkGoap): add FProcessor_Goap_Tier_HandleResult"
```

### Task 4.4: Implement `FProcessor_Goap_Tier_AutoReplan`

- [ ] **Step 1: Port today's AutoReplan logic per-tier**

Consumes `FTag_Goap_Dirty_WorldState` / `_Cost` / `RequiresInitialPlan`; enqueues `FCk_Request_Goap_Tier_Plan` according to policy + throttle.

- [ ] **Step 2: Register + commit**

```powershell
git commit -m "feat(CkGoap): add FProcessor_Goap_Tier_AutoReplan"
```

### Task 4.5: Implement `FProcessor_Goap_Bundle_ChainUpdate` ⭐

**Files:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Processor.h`
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Processor.cpp`

- [ ] **Step 1: Declare**

```cpp
class CKGOAP_API FProcessor_Goap_Bundle_ChainUpdate : public ck_exp::TProcessor<
    FProcessor_Goap_Bundle_ChainUpdate,
    FCk_Handle_Goap_Bundle,
    const FFragment_Goap_Bundle_Params,
    FFragment_Goap_Bundle_Current,
    FFragment_Goap_Bundle_ActiveTiers,
    const FFragment_Goap_Bundle_TierCatalogIndex,
    CK_IGNORE_PENDING_KILL>
{
public:
    using TProcessor::TProcessor;

    auto ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        const FFragment_Goap_Bundle_Params& InParams,
        FFragment_Goap_Bundle_Current& InCurrent,
        FFragment_Goap_Bundle_ActiveTiers& InActiveTiers,
        const FFragment_Goap_Bundle_TierCatalogIndex& InCatalogIndex) const -> void;
};
```

- [ ] **Step 2: Implement the chain-update rule from spec §4.3**

```cpp
CK_REGISTER_PROCESSOR(ck::FProcessor_Goap_Bundle_ChainUpdate);

auto FProcessor_Goap_Bundle_ChainUpdate::ForEachEntity(
    TimeType, HandleType InHandle,
    const FFragment_Goap_Bundle_Params&,
    FFragment_Goap_Bundle_Current& InCurrent,
    FFragment_Goap_Bundle_ActiveTiers& InActiveTiers,
    const FFragment_Goap_Bundle_TierCatalogIndex& InCatalogIndex) const -> void
{
    if (InCurrent.Get_EnableToggle() == ECk_EnableDisable::Disable) { return; }

    auto& Tiers = InActiveTiers._Tiers;
    const auto& Catalog = InCatalogIndex.Get_TagToTier();

    if (Tiers.IsEmpty()) { return; }

    const auto OldChainSnapshot = Tiers;
    auto bChainChanged = false;
    auto i = int32{0};

    while (i < Tiers.Num())
    {
        auto& CurrTier = Tiers[i];
        auto& CurrCurrent = CurrTier.template Get<FFragment_Goap_Tier_Current>();

        if (CurrCurrent._PlanStatus != ECk_GoapPlanStatus::PlanFound)
        { goto Done; }   // freshly-appended / planning / failed — don't walk past

        const auto& Plan = CurrCurrent._Plan;
        if (Plan.IsEmpty())
        {
            // Goal already satisfied / no actions. Truncate any children.
            if (i + 1 < Tiers.Num())
            {
                DoTruncateChainFrom(Tiers, i + 1);
                bChainChanged = true;
            }
            break;
        }

        const auto NextActionClass = Plan[0];
        const auto* CDO = NextActionClass.GetDefaultObject();
        if (NOT ck::IsValid(CDO)) { break; }
        const auto NextActionTag = CDO->Get_ActionTag();

        const auto* MatchingTierPtr = Catalog.Find(NextActionTag);
        const auto MatchingTier = MatchingTierPtr ? *MatchingTierPtr : FCk_Handle_Goap_Tier{};

        if (i + 1 < Tiers.Num())
        {
            const auto& ExistingChild = Tiers[i + 1];
            const auto ExistingChildTag = ExistingChild
                .template Get<FFragment_Goap_Tier_Params>().Get_TierTag();
            if (ExistingChildTag == NextActionTag)
            {
                // Match — chain stable here.
                ++i;
                continue;
            }
            // Mismatch — truncate, then fall through to leaf-handling.
            DoTruncateChainFrom(Tiers, i + 1);
            bChainChanged = true;
        }

        // i is at the leaf.
        if (NOT ck::IsValid(MatchingTier)) { break; }  // no child to append

        Tiers.Add(MatchingTier);
        bChainChanged = true;

        auto& MatchingCurrent = MatchingTier.template Get<FFragment_Goap_Tier_Current>();

        // Resolve WS source.
        const auto& ChildParams = MatchingTier.template Get<FFragment_Goap_Tier_Params>();
        if (ck::IsValid(ChildParams.Get_WorldStateSource_Override()))
        {
            MatchingCurrent._WorldStateSource_Resolved =
                ChildParams.Get_WorldStateSource_Override();
        }
        else
        {
            MatchingCurrent._WorldStateSource_Resolved =
                CurrCurrent._WorldStateSource_Resolved;   // inherit
        }

        // Inject goal from parent action's Effects (synchronous).
        DoInjectGoalSynchronous(CurrTier, NextActionClass, MatchingTier, MatchingCurrent);

        MatchingCurrent._ActiveParentAction = NextActionClass;

        // Subscribe to WS.
        if (ck::IsValid(MatchingCurrent._WorldStateSource_Resolved))
        {
            DoSubscribeTierToWorldState(MatchingTier, MatchingCurrent._WorldStateSource_Resolved);
        }

        // Mark for first plan (next frame).
        MatchingTier.AddOrGet<FTag_Goap_Tier_RequiresInitialPlan>();

        // Fire OnTierActivated.
        UUtils_Signal_Goap_OnTierActivated::Broadcast(MatchingTier, MakePayload(MatchingTier));

        break;  // deferred — newly-appended tier plans next frame
    }

Done:
    if (bChainChanged)
    {
        UUtils_Signal_Goap_OnActiveTiersChanged::Broadcast(
            InHandle, MakePayload(InHandle, OldChainSnapshot));
    }
}
```

`DoTruncateChainFrom`, `DoInjectGoalSynchronous`, `DoSubscribeTierToWorldState` are free helpers in the cpp:

```cpp
namespace
{
    auto DoTruncateChainFrom(
        TArray<FCk_Handle_Goap_Tier>& InTiers, int32 InStartIndex) -> void
    {
        for (auto i = InTiers.Num() - 1; i >= InStartIndex; --i)
        {
            auto& Tier = InTiers[i];
            auto& Current = Tier.template Get<FFragment_Goap_Tier_Current>();
            if (ck::IsValid(Current._WorldStateSource_Resolved))
            {
                DoUnsubscribeTierFromWorldState(Tier, Current._WorldStateSource_Resolved);
            }
            Current._Goal = {};
            Current._InvalidGoal = {};
            Current._ActiveParentAction = nullptr;
            Current._WorldStateSource_Resolved = {};
            Current._Plan.Reset();
            Current._PlanStatus = ECk_GoapPlanStatus::Idle;
            UUtils_Signal_Goap_OnTierDeactivated::Broadcast(Tier, MakePayload(Tier));
            InTiers.RemoveAt(i);
        }
    }

    auto DoInjectGoalSynchronous(
        const FCk_Handle_Goap_Tier& InParentTier,
        TSubclassOf<UCk_GoapAction_EntityScript> InParentAction,
        FCk_Handle_Goap_Tier& InChildTier,
        FFragment_Goap_Tier_Current& InChildCurrent) -> void
    {
        InChildCurrent._Goal = {};
        InChildCurrent._InvalidGoal = {};

        // Find the parent action's effects (CDO-extracted, raw tag form, in
        // the parent tier's resolved action defs).
        const auto& ParentActions = InParentTier
            .template Get<FFragment_Goap_Tier_Actions>().Get_ActionDefs();
        const auto* ParentDef = ParentActions.FindByPredicate(
            [&](const goap::FActionDef& D) { return D.ActionClass == InParentAction; });
        if (NOT ParentDef) { return; }

        const auto& ChildRegistry = InChildCurrent._WorldStateSource_Resolved
            .Get<FFragment_Goap_WorldState_KeyRegistry>().Get_Registry();

        for (const auto& Effect : ParentDef->Effects)
        {
            // Effect.Key is a keyed slot from PARENT's registry; we need to
            // look up by RAW tag in the CHILD's registry. The parent's
            // registry maps tag→key; we need to invert. Easiest: store the
            // raw FGameplayTag alongside each effect in FActionDef.

            // NOTE: If FActionDef currently stores ONLY keyed conditions, add
            // a parallel raw-tag list (FActionDef gains Effects_Raw + Pre_Raw
            // populated at Setup-time). Goal injection reads Effects_Raw.

            const auto RawTag = Effect.RawKey;  // assumes FActionDef has this; see Phase 1 amendments
            const auto ChildKey = ChildRegistry.Find(RawTag);
            if (ChildKey == goap::InvalidGoapKey)
            {
                InChildCurrent._InvalidGoal.Add(
                    FCk_GoapWS_Condition_Authored{RawTag, Effect.Value});
            }
            else
            {
                InChildCurrent._Goal.Add(goap::FWorldStateCondition{ChildKey, Effect.Value});
            }
        }
    }
}
```

**NOTE ABOUT `FActionDef`:** if `goap::FActionDef` currently does NOT carry the raw `FGameplayTag` alongside the keyed effect, add it. Look in `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Algorithm/CkGoap_Types.h` for FActionDef's definition; add `TArray<FGameplayTag> Effects_RawKeys` (parallel to `Effects`) or change `FWorldStateEffect` to carry both. Setup populates both during the raw→keyed resolution pass.

- [ ] **Step 3: Set processor registration order in CkGoap_Module.cpp or wherever processors are registered to ensure ChainUpdate runs AFTER the per-tier processors.**

Per spec §4.1: `Setup → AutoReplan → HandleRequests → AStar_Execute → HandleResult → ChainUpdate`.

- [ ] **Step 4: Compile + commit**

```powershell
git commit -m "feat(CkGoap): add FProcessor_Goap_Bundle_ChainUpdate (truncate/extend rule)"
```

### Task 4.6: Wire up `TProcessor_AStar_Execute` for the new per-tier fragments

- [ ] **Step 1: Make sure CkAStar's execute processor is templated against our `FCk_Handle_Goap_Tier`, `FFragment_Goap_Tier_SearchState`, `FFragment_Goap_Tier_Result`, `FFragment_Goap_Tier_PlanContext`, and registered via `CK_REGISTER_PROCESSOR`.**

Pattern matches today's `CkGoap_Processor.cpp:17` (which has `CK_REGISTER_PROCESSOR(ck::FProcessor_Goap_Execute);`). Add the per-tier equivalent.

- [ ] **Step 2: Compile + commit**

```powershell
git commit -m "feat(CkGoap): register per-tier A* execute processor"
```

---

## Phase 5 — Subscriber/dirty plumbing

### Task 5.1: Verify WS subscriber list accepts tier handles

**Files:**
- Read: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/WorldState/CkGoap_WorldState_Fragment_Data.h`
- Read: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/WorldState/CkGoap_WorldState_Processor.cpp`

- [ ] **Step 1: Inspect the existing subscriber field shape**

If `_Subscribers` is `TArray<FCk_Handle>`, tiers slot in directly. If it's `TArray<FCk_Handle_Goap>`, we need to retype it.

- [ ] **Step 2: If retype needed, update to `TArray<FCk_Handle>` (generic) and verify the WS processor's dirty-tag application still works (it stamps `FTag_Goap_Dirty_WorldState` on each subscriber regardless of typesafe-handle shape).**

- [ ] **Step 3: Commit**

```powershell
git commit -m "refactor(CkGoap): widen WS subscriber list to generic FCk_Handle"
```

### Task 5.2: Implement subscribe/unsubscribe helpers for tier WS

These are the `DoSubscribeTierToWorldState` / `DoUnsubscribeTierFromWorldState` helpers referenced in Task 4.5.

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Processor.cpp` (or move to a shared helper header)

- [ ] **Step 1: Implement**

```cpp
namespace
{
    auto DoSubscribeTierToWorldState(
        FCk_Handle_Goap_Tier& InTier, FCk_Handle_Goap_WorldState& InWS) -> void
    {
        if (NOT ck::IsValid(InWS)) { return; }
        auto& Subs = InWS.template Get<FFragment_Goap_WorldState_Subscribers>();
        Subs._Subscribers.AddUnique(InTier);
    }

    auto DoUnsubscribeTierFromWorldState(
        FCk_Handle_Goap_Tier& InTier, FCk_Handle_Goap_WorldState& InWS) -> void
    {
        if (NOT ck::IsValid(InWS)) { return; }
        auto& Subs = InWS.template Get<FFragment_Goap_WorldState_Subscribers>();
        Subs._Subscribers.RemoveSwap(InTier);
    }
}
```

- [ ] **Step 2: Compile + commit**

```powershell
git commit -m "feat(CkGoap): subscribe/unsubscribe helpers for tier WS"
```

---

## Phase 6 — Diagnostics

### Task 6.1: Setup-time dependency-cycle detection

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/CkGoap_Bundle_Processor.cpp` (or split into a small setup processor — recommend separate `FProcessor_Goap_Bundle_Setup`)

- [ ] **Step 1: Add Bundle setup processor**

```cpp
class CKGOAP_API FProcessor_Goap_Bundle_Setup : public ck_exp::TProcessor<
    FProcessor_Goap_Bundle_Setup,
    FCk_Handle_Goap_Bundle,
    FFragment_Goap_Bundle_Current,
    const FFragment_Goap_Bundle_TierCatalogIndex,
    FTag_Goap_Bundle_RequiresSetup,
    CK_IGNORE_PENDING_KILL>
{
public:
    using TProcessor::TProcessor;
    auto ForEachEntity(TimeType, HandleType,
        FFragment_Goap_Bundle_Current&,
        const FFragment_Goap_Bundle_TierCatalogIndex&) const -> void;
};

CK_REGISTER_PROCESSOR(ck::FProcessor_Goap_Bundle_Setup);
```

- [ ] **Step 2: Implement cycle detection**

```cpp
auto FProcessor_Goap_Bundle_Setup::ForEachEntity(
    TimeType, HandleType InHandle,
    FFragment_Goap_Bundle_Current& InCurrent,
    const FFragment_Goap_Bundle_TierCatalogIndex& InIndex) const -> void
{
    InHandle.Remove<FTag_Goap_Bundle_RequiresSetup>();

    InCurrent._DependencyCycles.Reset();

    // Build graph: for each tier T, for each action A on T whose ActionTag
    // matches another tier T', add edge T -> T'.
    TMap<FCk_Handle_Goap_Tier, TArray<FCk_Handle_Goap_Tier>> Graph;
    const auto& Catalog = InIndex.Get_TagToTier();

    for (const auto& Entry : Catalog)
    {
        auto Tier = Entry.Value;
        const auto& Actions = Tier.Get<FFragment_Goap_Tier_Actions>().Get_ActionDefs();
        for (const auto& Def : Actions)
        {
            const auto* TargetTier = Catalog.Find(Def.ActionTag);
            if (TargetTier) { Graph.FindOrAdd(Tier).AddUnique(*TargetTier); }
        }
    }

    // DFS-based simple cycle detection (Tarjan SCC overkill for typical sizes).
    // Record each cycle as a comma-joined list of tier-tag names.
    // [Implementation detail — straightforward DFS with on-stack tracking]
}
```

- [ ] **Step 2: Add `FTag_Goap_Bundle_RequiresSetup` to bundle entities at AddBundle time** (back in `CkGoap_Bundle_Utils.cpp::AddBundle`).

- [ ] **Step 3: Compile + commit**

```powershell
git commit -m "feat(CkGoap): bundle dependency-cycle detection at setup"
```

---

## Phase 7 — Tests

### Task 7.1: Write AS test infrastructure (gym + base entity scripts)

**Files:**
- Modify: `Plugins/CkTests/Script/CkGoap/CkGoap_Shared.as` — update shared utilities to reflect the new API (helper functions for creating bundles/tiers/actions).

- [ ] **Step 1: Read existing `CkGoap_Shared.as` to understand patterns**

- [ ] **Step 2: Update / replace shared helpers to use `utils_goap`, `utils_goap_bundle`, `utils_goap_tier` namespaces (verify generated AS surfaces match these names)**

- [ ] **Step 3: Commit**

```powershell
cd "D:\Repos\CkPlugins\Plugins\CkTests"
git commit -m "test(CkGoap): update AS shared helpers for Bundle/Tier API"
```

### Task 7.2-7.15: Write the 14 new tests

Each test in spec §9 gets its own AS file under `Plugins/CkTests/Script/CkGoap/`. Each follows the pattern:

```as
class UCkAutoTest_Goap_BundleTier_RootOnly_AutoTest : UCkAutoTest_EntityScript
{
    default _TimeoutSeconds = 3.0f;

    UFUNCTION()
    void DoConstruct()
    {
        const auto Owner = ck::SelfEntity(this);
        auto Goap = utils_goap::Add(Owner, FCk_Fragment_Goap_RootParamsData());
        auto Bundle = utils_goap_bundle::AddBundle(Goap,
            FCk_Fragment_Goap_BundleParamsData(n"Goap.Bundle.TestBundle"));

        auto WS = utils_goap_world_state::Add(Owner,
            FCk_Fragment_GoapWorldState_ParamsData());

        auto TierParams = FCk_Fragment_Goap_TierParamsData(n"Goap.Tier.Root");
        TierParams.Set_WorldStateSource_Override(WS);
        // ... initial goal, etc.

        auto Tier = utils_goap_tier::AddTier(Bundle, TierParams);
        utils_goap_tier::AddAction(Tier, UCk_GoapAction_Test_Foo::StaticClass());

        utils_goap_tier::BindTo_OnPlanComplete(Tier, ...);
    }
}
```

- [ ] **Step 1-14: Write each test, mark `[ ]` per item**

Per spec §9 — 14 tests. List each one as a checkbox so progress is visible.

- [ ] `CkAutoTest_Goap_BundleTier_RootOnly.as`
- [ ] `CkAutoTest_Goap_BundleTier_ChainGrowth.as`
- [ ] `CkAutoTest_Goap_BundleTier_ChainTruncation.as`
- [ ] `CkAutoTest_Goap_BundleTier_NoMatchingTier.as`
- [ ] `CkAutoTest_Goap_BundleTier_GoalInjection.as`
- [ ] `CkAutoTest_Goap_BundleTier_WSInheritance.as`
- [ ] `CkAutoTest_Goap_BundleTier_WSOverride.as`
- [ ] `CkAutoTest_Goap_BundleTier_InvalidGoal.as`
- [ ] `CkAutoTest_Goap_BundleTier_DirtyPropagation.as`
- [ ] `CkAutoTest_Goap_BundleTier_MultiBundle.as`
- [ ] `CkAutoTest_Goap_BundleTier_BundleToggle.as`
- [ ] `CkAutoTest_Goap_BundleTier_OwnerCascadeDestroy.as`
- [ ] `CkAutoTest_Goap_BundleTier_DeferOneFrame.as`
- [ ] `CkAutoTest_Goap_BundleTier_RootReset.as`

After each test lands, run the toolbox `build-test` workflow and confirm the new test passes alongside the existing baseline tests.

### Task 7.16: Review the 5 baseline tests

**Files:**
- Review: `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_BasicPlan.as`, `CkAutoTest_Goap_DependencyChain.as`, and 3 others

- [ ] **Step 1: Retarget each baseline test to the new API**

For each test, rewrite to use the Bundle/Tier model:

- Single-bundle / single-tier where the test exercises basic planning.
- Replace `AddGoal` calls with TierParams `_InitialGoal_RootOnly`.

- [ ] **Step 2: Verify each passes**

- [ ] **Step 3: Commit per test, or batch at end**

```powershell
git commit -m "test(CkGoap): retarget 5 baseline tests to Bundle/Tier API"
```

---

## Phase 8 — Polish + docs

### Task 8.1: Update CkGoap CLAUDE.md

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md`

- [ ] **Step 1: Rewrite the architecture diagram + API surface table to reflect Bundle/Tier model**

- [ ] **Step 2: Update the "Anti-patterns" section to reflect what's now-not-applicable (e.g. `Add` on owner with standalone AStar is still relevant but rephrased)**

- [ ] **Step 3: Add a "Bundle/Tier model overview" section at the top, replacing the old "Add vs Create" section**

- [ ] **Step 4: Commit**

```powershell
git commit -m "docs(CkGoap): rewrite CLAUDE.md for Bundle/Tier model"
```

### Task 8.2: Final submodule pointer bump

- [ ] **Step 1: Bump all submodule pointers in CkPlugins root**

```powershell
cd "D:\Repos\CkPlugins"
git add Plugins/CkFoundation Plugins/CkTests Plugins/CkGameplayDebugger
git commit -m "chore(submodule): bump CkFoundation + CkTests + CkGameplayDebugger (BundleTier refactor complete)"
```

---

## Self-review checklist

After completing this plan, verify:

- [ ] All 16 spec decisions reflected in tasks (cross-reference spec §1 + §3-§7)
- [ ] No TBD / TODO placeholders in this plan (the one TODO in Phase 0 Task 0.2 Step 3 refers to a CODE marker, not a plan placeholder)
- [ ] Type names consistent across tasks (e.g. `FCk_Handle_Goap_Tier`, `_TierTag`, `_ActiveParentAction`)
- [ ] Each phase ends with a clean compile + (where possible) test run
- [ ] Each commit is scoped to one logical change
- [ ] No reference to removed types in late-phase tasks
- [ ] All 14 new tests listed in §9 of the spec have a corresponding task in Phase 7

---

## Execution notes

- **Build-test gate** after each phase. Toolbox `build-test` workflow.
- **Editor lock awareness**: per `feedback_wait_for_other_editor`, poll the editor log lock before running build commands.
- **Submodule discipline**: each plugin's commits land in the plugin's submodule; CkPlugins root commits are pointer bumps only.
- **Live Coding limitations**: per `feedback_processor_registration`, new `CK_REGISTER_PROCESSOR` calls may not be picked up via Live Coding — full rebuild may be needed when adding the per-tier processors.
- **AS recompile**: deleting old tests in Phase 0 + the autogenerated `CkTests_AutoTestActors.as` regen is the main AS-side action. Editor restart after Phase 2's handle JSON regen.
