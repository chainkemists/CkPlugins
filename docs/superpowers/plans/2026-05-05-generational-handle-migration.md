# Generational Handle Migration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `TSharedPtr<entt::basic_registry>`-based `FCk_Handle` representation with a generational slot+gen handle, eliminating atomic refcount overhead on every handle copy/destroy and giving defined behavior on stale handle access.

**Architecture:** A new `FCk_RegistryHandle` is a 12-byte POD `{int32 SlotIndex; uint32 Generation}`. A static slot table in `CkEcs` maps slot indices to live `entt::basic_registry*`s, bumping a generation counter when a slot is freed. `FCk_Handle` becomes `{FCk_Entity; FCk_RegistryHandle; ...}` — trivially copyable, no special members beyond what UE reflection requires. `FCk_Registry` becomes a non-owning view that resolves a `FCk_RegistryHandle` to the underlying `entt::basic_registry*` on demand. Ownership of each `entt::basic_registry` lives in `UCk_EcsWorld_Subsystem_UE` (and `UCk_EcsEditor_Subsystem`) via `TUniquePtr`.

**Tech Stack:** Unreal Engine 5.6, C++20, EnTT 3.16, AngelScript (Hazelight fork). All work happens in `D:/Repos/CkPlugins/Plugins/CkFoundation/`. Tests in `D:/Repos/CkPlugins/Plugins/CkTests/Script/CkRegistry/` as AngelScript AutoTests subclassing `UCk_AutoTest_Base`.

---

## File Structure

### New files

- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_Handle.h` — declares `FCk_RegistryHandle` POD struct.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.h` — declares slot-table API (`Allocate`, `Free`, `Resolve`, `TryResolve`).
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp` — slot-table implementation with phoenix-singleton storage.
- `Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_HandleCopyDestroy.as`
- `Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_HandleInFragmentLifecycle.as`
- `Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_PieStartStopStress.as`
- `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_SlotTable.spec.cpp` — C++ unit tests: basic allocate/free/resolve, generation wrap, post-destruction `Free`, `IsInGameThread` enforcement.
- `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_LifetimeInversion.spec.cpp` — **C++ test that explicitly reproduces the `UCk_Processor_Script_Base_UE`-style lifetime inversion** (synthetic `UObject` with handle field; subsystem-equivalent freed; force GC; verify dtor doesn't crash and handle reports invalid). Replaces the previously-planned AS-side `StaleHandleAfterPieStop` test, which couldn't reach the same GC-ordering bucket.
- `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_HandleCopyBenchmark.cpp` — microbenchmark (~30 lines) measuring 10M handle copy/destroy throughput; logged for record-keeping per CTO request.

### Modified files

- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle.h` — replace `TOptional<FCk_Registry> _Registry` with `FCk_RegistryHandle _RegistryHandle`. Remove special members (now trivial). Update `TStructOpsTypeTraits`.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle.cpp` — strip out the migration-era diagnostic logging, simplify special-member implementations.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle_TypeSafe.h` — update `CK_GENERATED_BODY_HANDLE_TYPESAFE` macro for new layout; preserve `sizeof(typesafe) == sizeof(FCk_Handle)` invariant.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle_ReadOnly.h` / `.cpp` — mirror layout change.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.h` — `FCk_Registry` becomes a non-owning view (`entt::basic_registry*` + `FCk_RegistryHandle` for staleness checks); remove TSharedPtr storage.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.cpp` — strip diagnostic logging, simplify special members.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsWorld_Subsystem.h` / `.cpp` — own `TUniquePtr<entt::basic_registry<...>>`, allocate slot on `Initialize`, free on `Deinitialize`.
- `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsEditor_Subsystem.h` / `.cpp` — same pattern as world subsystem.
- `Plugins/CkFoundation/Source/CkEcs/CkEcs_Module.cpp` — remove diagnostic startup logging.
- `Plugins/CkFoundation/Source/CkDynamic/CkDynamic_Module.cpp` — remove diagnostic startup logging.
- `Plugins/CkFoundation/Source/CkAssetExporter/CkAssetExporter_Module.cpp` — remove diagnostic startup logging.
- `Plugins/CkFoundation/Source/CkPmg/CkPmg_Module.cpp` — remove diagnostic startup logging.

### Deleted concepts (no file deletion, just member/macro removal)

- `FCk_Registry::Shutdown()` — no longer needed once cycles are impossible.
- `FCk_Registry::Debug_GetSharedRefCount()`, `Debug_GetInternalRegistryPtr()` — diagnostic-only, not needed post-migration.
- `WithCopy = true` traits flag on handles — type is trivially copyable, flag becomes irrelevant.
- The migration-era detector (`Do_CheckSuspiciousRefCount`) and lifecycle macros (`CK_HANDLE_LC_LOG`, `CK_REGISTRY_LC_LOG`) — entire diagnostic apparatus.

---

## Pre-Flight — branch, tag, baseline

Land **before any code change.** Establishes the rollback anchor and the test-pass baseline against which migration progress is measured.

### Pre-Flight Task PF.1: Tag the parent commit on `dev`

Per CTO guardrail #3: revert anchor for the entire migration. The migration branches from `dev` (not `main`) because `dev` is 101 commits ahead and contains the current state we need to integrate against.

- [ ] **Step 1: Tag and commit (no code change)**

```bash
cd D:/Repos/CkPlugins
git fetch origin
git checkout dev
git pull
git tag pre-generational-handle-migration
# Push the tag to origin once ready (gated on user confirmation):
# git push origin pre-generational-handle-migration
```

### Pre-Flight Task PF.2: Create the migration branch

- [ ] **Step 1: Create and check out the branch**

```bash
git checkout -b feature/generational-handle-migration
```

The plan lands as a **single PR** with per-phase commits preserved for archaeology, targeting `dev`. **No cherry-picks, no rebase-onto-dev mid-migration.** Merge button stays disabled until Task 8.7 is complete and signed off.

### Pre-Flight Task PF.3: AutoTest baseline — capture known-failing tests

There are pre-existing failing tests in the suite that are not migration-related. Capture them now so Phase 6 / Phase 8 sweeps can ignore them.

- [ ] **Step 1: Run the full suite via Test Automation panel**

Filter: blank (run everything). Save the results to disk via the panel's export, or via:

```
UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests CkAutoTest_; Quit" -log=baseline-tests.log
```

- [ ] **Step 2: Extract the failing test names into a baseline file**

Create `docs/superpowers/plans/2026-05-05-generational-handle-migration-baseline-failures.md`:

```markdown
# AutoTest baseline — pre-migration known-failing tests

Captured at branch `feature/generational-handle-migration` HEAD on YYYY-MM-DD.
Migration phases ignore this list; Phase 6 / Phase 8 sweeps verify only that
no NEW failures are introduced.

## Failing tests (do not chase during migration)

- CkAutoTest_<Module>_<Name> — failure: <one-line reason if known>
- ...
```

- [ ] **Step 3: Commit the baseline**

```bash
git add docs/superpowers/plans/2026-05-05-generational-handle-migration-baseline-failures.md
git commit -m "docs(plan): capture pre-migration AutoTest baseline (known failures)"
```

### Pre-Flight Audit Findings (Tasks 0.6 + 0.7 completed early)

The two Phase 0 research tasks (`_TransientEntity` audit and NetSerializer wire-format verification) were executed during Pre-Flight since their findings affect plan task structure for later phases. Recording the decisions here so subsequent task implementations can reference them:

#### Audit 0.6 — `_TransientEntity` decision: **option (c) — store in `entt::registry::ctx`**

Findings from `grep -rn "_TransientEntity\|Get_TransientEntity"` across `Plugins/CkFoundation/Source`:

- Most readers go through `UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(world)` or `UCk_Utils_EntityLifetime_UE::Get_TransientEntity(handle/registry)` — utility-routed access.
- A handful read directly via `FCk_Registry::Get_TransientEntity()` (e.g., `CkEntityLifetime_Utils.cpp:401`).
- `TProcessor::_TransientEntity` is a per-processor cached field (separate concept; unchanged by migration).

Decision rationale: storing in `entt::registry::ctx<FCk_TransientEntityCtx>()` (a) keeps `FCk_Registry` view at minimum size — just slot+gen, 12 bytes; (b) is idiomatic for entt; (c) requires zero changes to existing call sites since `FCk_Registry::Get_TransientEntity()` keeps its signature, the implementation swap is internal.

**Implementation:**
- **Phase 4 — subsystem `Initialize`:** after `entt::basic_registry` creation, register the transient entity into the registry's context: `OwnedRegistry->ctx().emplace<FCk_TransientEntityCtx>(FCk_TransientEntityCtx{TransientEntity})`.
- **Phase 2 — `FCk_Registry::Get_TransientEntity()`:** read it back: `return Resolve()->ctx().get<FCk_TransientEntityCtx>().Entity`.
- **Phase 2 — `FCk_Registry`:** remove `FCk_Entity _TransientEntity;` field. `FCk_Registry` becomes `{ FCk_RegistryHandle _RegistryHandle; }` only — 12 bytes.

Add a tiny POD type at the top of `CkRegistry.h` or in `CkRegistry_Handle.h`:
```cpp
namespace ck { struct FCk_TransientEntityCtx { FCk_Entity Entity; }; }
```

#### Audit 0.7 — NetSerializer wire-format claim: **CONFIRMED**

Findings from inspection of `CkHandle.cpp`:

- Legacy `NetSerialize` body (line 407–438): both saving and loading paths execute only `Ar << _ReplicationDriver` (lines 423, 428). On load, the entity-side state is reconstructed locally via `*this = _ReplicationDriver->Get_AssociatedEntity()`.
- Iris `FCk_HandleNetSerializer::Quantize` (line ~845): writes only `&Source._ReplicationDriver` through `FWeakObjectNetSerializer`.
- Iris `Dequantize` (line ~865): reads `_ReplicationDriver`, calls `Get_AssociatedEntity()` to reconstruct local handle.
- Zero references to `_Entity` or `_Registry` (or `_RegistryHandle` post-migration) in any serialization body.

**Conclusion:** Internal handle bytes never cross machines. The migration's layout change (replacing `TOptional<FCk_Registry>` with `FCk_RegistryHandle`) is invisible to replication. Phase 7 stays as a verification-only phase — no code changes to `NetSerialize` or `FCk_HandleNetSerializer` required.

### Pre-Flight Task PF.4: Test scoping — run only new + relevant tests during migration

Per user direction: full-suite runs are too slow for per-task iteration. Subsequent test invocations in this plan default to:

- **C++ unit tests:** `Filter: "Ck.Registry.*"` (covers the slot table + lifetime-inversion tests we add).
- **AS AutoTests:** `Filter: "CkAutoTest_Registry_*"` (covers the new tests in `Plugins/CkTests/Script/CkRegistry/`).
- **Phase 6 + Phase 8 sweeps only:** widen to the full `CkAutoTest_*` filter to catch cross-module regressions, comparing failures against the baseline file from PF.3.

This is documented as a convention; individual task steps reference these filters explicitly.

---

## Phase 0 — Test Infrastructure

Set up the AutoTest suite that documents desired handle/registry behavior. Tests in this phase run against the **current** code first; some will fail (those failures characterize the bug we're fixing). After the migration, all tests must pass.

### Task 0.1: Create test directory and skeleton runner

**Files:**
- Create: `Plugins/CkTests/Script/CkRegistry/` (directory)

- [ ] **Step 1: Create the directory**

```bash
mkdir -p "D:/Repos/CkPlugins/Plugins/CkTests/Script/CkRegistry"
```

- [ ] **Step 2: Verify the directory is empty and ready**

Run: `ls "D:/Repos/CkPlugins/Plugins/CkTests/Script/CkRegistry"`
Expected: empty output (no files yet).

- [ ] **Step 3: Commit the empty directory placeholder**

Create a `.gitkeep` so git tracks the directory:

```bash
touch "D:/Repos/CkPlugins/Plugins/CkTests/Script/CkRegistry/.gitkeep"
git add "Plugins/CkTests/Script/CkRegistry/.gitkeep"
git commit -m "test(CkRegistry): scaffold AutoTest directory"
```

### Task 0.2: AutoTest — handle copy/destroy roundtrip

Verifies that copying a handle, holding the copy, and destroying the original leaves the copy valid. This currently passes (TSharedPtr accounting works); after migration it must continue to pass with no atomics.

**Files:**
- Create: `Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_HandleCopyDestroy.as`

- [ ] **Step 1: Write the test**

```angelscript
// Language=angelscript

//============================================================================
// CK REGISTRY — AUTOMATION TEST: HANDLE COPY / DESTROY
//============================================================================
//
// Verifies basic copy/destroy semantics for FCk_Handle:
//   1. Spawn an entity, capture handle A.
//   2. Copy handle A into handle B (separate variable).
//   3. Destroy entity via handle A.
//   4. Assert ck::IsValid(A) == false (entity gone).
//   5. Assert ck::IsValid(B) == false (entity also gone for B since they
//      both reference the same entity in the same registry).
//   6. Assert that destroying B does not crash (its registry-handle
//      should still be resolvable, the entity is just gone).
//============================================================================

class UCk_AutoTest_Registry_HandleCopyDestroy : UCk_AutoTest_Base
{
    UFUNCTION(BlueprintOverride)
    void DoBeginPlay(FCk_Handle InHandle)
    {
        auto LocalHandle = InHandle;

        // Spawn a child entity off the runner's handle. utils_entity_lifetime
        // gives a freshly-spawned handle.
        auto SpawnedA = utils_entity_lifetime::Request_SpawnEntity(LocalHandle);
        Assert_True(ck::IsValid(SpawnedA), "Spawned entity should be valid");

        auto SpawnedB = SpawnedA; // copy
        Assert_True(ck::IsValid(SpawnedB), "Copy of valid handle should be valid");

        utils_entity_lifetime::Request_DestroyEntity(SpawnedA);

        // Both A and B reference the same entity, which is now gone.
        Assert_True(ck::Is_NOT_Valid(SpawnedA), "After destroy, original is invalid");
        Assert_True(ck::Is_NOT_Valid(SpawnedB), "After destroy, copy is also invalid");

        // Destroying B implicitly when this function returns must not crash.
        Finish_Success();
    }
}
```

- [ ] **Step 2: Run the existing AutoTest harness to verify the test loads**

Run: open the editor, run **Window → Test Automation → Filter: "CkAutoTest_Registry_HandleCopyDestroy"** and execute.
Expected: PASS on current code (TSharedPtr accounting handles this case correctly).

- [ ] **Step 3: Commit**

```bash
git add Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_HandleCopyDestroy.as
git commit -m "test(CkRegistry): handle copy/destroy roundtrip"
```

### Task 0.3: C++ test — `UObject` lifetime inversion (the actual bug we're fixing)

**This task replaces a previously-planned AngelScript test that wouldn't reach the same GC-ordering bucket as the real bug.** The original repro is `UCk_Processor_Script_Base_UE` (a `UObject`) outliving the world subsystem during PIE teardown — its `_Handle` field's destructor decrements an already-freed control block.

To exercise that lifetime inversion deterministically and synchronously (without depending on PIE start/stop timing or the AS test class living in a different GC bucket), we use a synthetic `UObject` test harness directly. Phase 1's slot table is a prerequisite — this test is *written* in Phase 0 to characterize current behavior, but only *runs green* once the migration is complete. On current code, it is expected to fail (handle dtor crashes or fires `[BAD_REFCOUNT_DETECTED]`).

**Files:**
- Create: `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_LifetimeInversion.spec.cpp`
- Create: `Plugins/CkTests/Source/CkTests/Public/UnitTests/CkRegistry_LifetimeInversion_Holder.h` — synthetic UObject holding an FCk_Handle.

- [ ] **Step 1: Write the synthetic UObject holder**

`Plugins/CkTests/Source/CkTests/Public/UnitTests/CkRegistry_LifetimeInversion_Holder.h`:

```cpp
#pragma once

#include "UObject/Object.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkRegistry_LifetimeInversion_Holder.generated.h"

// Synthetic UObject mirroring UCk_Processor_Script_Base_UE's pattern: a
// UPROPERTY FCk_Handle field that the test can deliberately leave dangling
// past the registry's destruction. Used only by the lifetime-inversion test.
UCLASS()
class CKTESTS_API UCk_LifetimeInversion_Holder : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY()
    FCk_Handle _Handle;
};
```

- [ ] **Step 2: Write the failing C++ test**

`Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_LifetimeInversion.spec.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "UnitTests/CkRegistry_LifetimeInversion_Holder.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
#include "CkEcs/Handle/CkHandle.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkRegistry_LifetimeInversion_HandleSurvivesRegistry,
    "Ck.Registry.LifetimeInversion.HandleSurvivesRegistry",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCkRegistry_LifetimeInversion_HandleSurvivesRegistry::RunTest(const FString& Parameters)
{
    using namespace ck::registry_table;

    // 1. Allocate a registry slot — analog of UCk_EcsWorld_Subsystem_UE::Initialize.
    auto* OwnedRegistry = new EnttRegistryType{};
    auto SlotHandle = Allocate(OwnedRegistry);

    // 2. Create a UObject that holds an FCk_Handle bound to that registry.
    //    AddToRoot prevents it being GC'd before we explicitly release it.
    auto* Holder = NewObject<UCk_LifetimeInversion_Holder>();
    Holder->AddToRoot();

    const auto Entity = FCk_Entity{OwnedRegistry->create()};
    Holder->_Handle = FCk_Handle{Entity, SlotHandle};

    TestTrue(TEXT("Handle valid before registry teardown"),
        ck::IsValid(Holder->_Handle));

    // 3. Tear down the registry — analog of UCk_EcsWorld_Subsystem_UE::Deinitialize.
    //    Slot is freed BEFORE the entt registry is deleted (matches subsystem
    //    deinit order in the production code).
    Free(SlotHandle);
    delete OwnedRegistry;
    OwnedRegistry = nullptr;

    // 4. The Holder's _Handle is now dangling. Pre-migration: ck::IsValid
    //    crashes / triggers BAD_REFCOUNT. Post-migration: returns false cleanly.
    TestFalse(TEXT("Stale handle reports invalid (no crash)"),
        ck::IsValid(Holder->_Handle));

    // 5. Force GC on the Holder. Pre-migration: ~FCk_Handle decrements freed
    //    control block; the editor fast-fails. Post-migration: 8-byte memcpy
    //    teardown is a no-op.
    Holder->RemoveFromRoot();
    Holder = nullptr;
    CollectGarbage(RF_NoFlags, true);

    // If we get here without crashing, the migration succeeded.
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkRegistry_LifetimeInversion_FreeAfterTableDestruction,
    "Ck.Registry.LifetimeInversion.FreeAfterTableDestruction",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCkRegistry_LifetimeInversion_FreeAfterTableDestruction::RunTest(const FString& Parameters)
{
    // Smoke test for the phoenix-singleton sentinel: simulate the "Free
    // called during static destruction order" scenario by directly invoking
    // the test hook below. The phoenix singleton must accept Free calls
    // after its sentinel flips and silently no-op them.
    using namespace ck::registry_table;

    // Test hook (defined in CkRegistry_SlotTable.cpp in non-shipping):
    extern auto Debug_SimulateTableDestruction_DoNotUseInProduction() -> void;

    auto Reg = EnttRegistryType{};
    auto H = Allocate(&Reg);
    TestTrue(TEXT("Handle resolves before sim-destruction"), Resolve(H) == &Reg);

    Debug_SimulateTableDestruction_DoNotUseInProduction();

    Free(H); // Must not crash.
    TestNull(TEXT("Resolve after sim-destruction is null"), Resolve(H));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 3: Run on current code; expect first test to fail/crash**

Run: `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests Ck.Registry.LifetimeInversion; Quit"`
Expected (current code): `HandleSurvivesRegistry` crashes the editor or fires `[BAD_REFCOUNT_DETECTED]` (depending on which test framework catches it first). The second test will fail to link until Phase 1's `Debug_SimulateTableDestruction_DoNotUseInProduction` exists. This is the documented broken behavior the migration fixes.

- [ ] **Step 4: Commit (failing test, characterizing the bug)**

```bash
git add Plugins/CkTests/Source/CkTests/Public/UnitTests/CkRegistry_LifetimeInversion_Holder.h \
        Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_LifetimeInversion.spec.cpp
git commit -m "test(CkRegistry): C++ UObject lifetime-inversion test (currently FAILS — bug repro)"
```

### Task 0.4: AutoTest — handle stored in fragment lifecycle

Tests the cycle scenario: a handle stored inside a fragment of an entity referencing back to the same registry. Currently this requires `FCk_Registry::Shutdown()` to break the cycle; post-migration there should be no cycle (slot+gen is just bytes).

**Files:**
- Create: `Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_HandleInFragmentLifecycle.as`

- [ ] **Step 1: Write the test**

```angelscript
// Language=angelscript

//============================================================================
// CK REGISTRY — AUTOMATION TEST: HANDLE-IN-FRAGMENT LIFECYCLE
//============================================================================
//
// Verifies that storing FCk_Handle inside a fragment (which lives inside
// the registry) does not prevent the registry from being torn down cleanly.
//
//   1. Spawn parent and child entities.
//   2. Store the child handle inside a debug fragment on the parent.
//   3. Destroy the parent (cascades fragment destruction).
//   4. Assert no crashes.
//   5. Re-query: child should also be cleaned up (or detectably invalid).
//============================================================================

class UCk_AutoTest_Registry_HandleInFragmentLifecycle : UCk_AutoTest_Base
{
    UFUNCTION(BlueprintOverride)
    void DoBeginPlay(FCk_Handle InHandle)
    {
        auto LocalHandle = InHandle;
        auto Parent = utils_entity_lifetime::Request_SpawnEntity(LocalHandle);
        auto Child = utils_entity_lifetime::Request_SpawnEntity(Parent);

        Assert_True(ck::IsValid(Parent), "Parent must be valid");
        Assert_True(ck::IsValid(Child),  "Child must be valid");

        // Use the existing AutoTest debug fragment to stash a handle.
        auto Frag = FCk_Fragment_AutoTest_HandleHolder();
        Frag.Set_StoredHandle(Child);
        Parent.Add_Fragment(Frag);

        utils_entity_lifetime::Request_DestroyEntity(Parent);

        Assert_True(ck::Is_NOT_Valid(Parent),
            "Parent must be invalid after destroy");
        Assert_True(ck::Is_NOT_Valid(Child),
            "Child must be invalid after parent destroy (cascades)");

        Finish_Success();
    }
}
```

- [ ] **Step 2: If `FCk_Fragment_AutoTest_HandleHolder` doesn't already exist, add it**

Search for existing test fragment types:

```bash
grep -rn "FCk_Fragment_AutoTest" Plugins/CkTests/Source
```

If none stores a handle, add a minimal one in `Plugins/CkTests/Source/CkTests/Public/CkTests/CkTests_Fragment_Data.h`:

```cpp
USTRUCT(BlueprintType)
struct CKTESTS_API FCk_Fragment_AutoTest_HandleHolder
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Fragment_AutoTest_HandleHolder);

private:
    UPROPERTY()
    FCk_Handle _StoredHandle;

public:
    CK_PROPERTY(_StoredHandle);
};
```

- [ ] **Step 3: Run and verify on current code**

Expected: PASS on current code (cycle-breaking via `Shutdown` works in this scenario).

- [ ] **Step 4: Commit**

```bash
git add Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_HandleInFragmentLifecycle.as \
        Plugins/CkTests/Source/CkTests/Public/CkTests/CkTests_Fragment_Data.h
git commit -m "test(CkRegistry): handle-in-fragment lifecycle"
```

### Task 0.5: AutoTest — PIE start/stop stress

Repeats the user's reproducer in test form: many consecutive PIE start/stops while handles are held in fragments and UObjects. Currently this fails (silent process termination); post-migration it must pass.

**Files:**
- Create: `Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_PieStartStopStress.as`

- [ ] **Step 1: Write the test**

```angelscript
// Language=angelscript

//============================================================================
// CK REGISTRY — AUTOMATION TEST: PIE START/STOP STRESS
//============================================================================
//
// Cycles entity spawn → store-in-fragment → tag-for-destroy → tick → repeat
// many times within a single PIE session. Although this doesn't actually
// stop/start PIE (AutoTests don't drive that themselves), it stresses the
// same allocation patterns the bug exhibited: many short-lived entities
// with handles stashed across frame boundaries.
//
// The actual cross-PIE coverage requires running the AutoTest suite
// repeatedly via the Test Automation panel; that part is operator-driven.
//============================================================================

class UCk_AutoTest_Registry_PieStartStopStress : UCk_AutoTest_Base
{
    private const int32 _IterationCount = 1000;
    private int32 _Iteration = 0;
    private FCk_Handle _PersistentHandle;

    UFUNCTION(BlueprintOverride)
    void DoBeginPlay(FCk_Handle InHandle)
    {
        _PersistentHandle = InHandle;
        DoNextIteration();
    }

    private void DoNextIteration()
    {
        if (_Iteration >= _IterationCount)
        {
            Finish_Success();
            return;
        }

        ++_Iteration;
        auto Spawn = utils_entity_lifetime::Request_SpawnEntity(_PersistentHandle);
        Assert_True(ck::IsValid(Spawn),
            f"Iteration {_Iteration}: spawn must produce valid handle");

        utils_entity_lifetime::Request_DestroyEntity(Spawn);

        Assert_True(ck::Is_NOT_Valid(Spawn),
            f"Iteration {_Iteration}: post-destroy must be invalid");

        DoNextIteration();
    }
}
```

- [ ] **Step 2: Run on current code**

Expected: PASS within a single PIE session (the bug only manifests across PIE start/stop). This test serves as a perf-floor check — 1000 spawn/destroys must complete without churn.

- [ ] **Step 3: Commit**

```bash
git add Plugins/CkTests/Script/CkRegistry/CkAutoTest_Registry_PieStartStopStress.as
git commit -m "test(CkRegistry): PIE start/stop stress (1000 spawn/destroy cycles)"
```

### Task 0.6: Audit `_TransientEntity` on `FCk_Registry`

Per CTO review: a non-owning view should not snapshot per-instance state. `_TransientEntity` carried on the view propagates through every `Get_Registry()` clone and is painful to remove later. Decide upfront whether it lives on the view, on the subsystem, or in `entt::registry::ctx`.

**Files:**
- (research only — produces a decision recorded as a comment on `CkRegistry.h`'s `_TransientEntity` member or a new top-of-file note)

- [ ] **Step 1: Find all readers and writers**

```bash
grep -rn "_TransientEntity\|Get_TransientEntity\|GetTransientEntity" Plugins/CkFoundation/Source
```

- [ ] **Step 2: Categorize each call site as one of**

  1. **Convenience accessor on the view** — read-only, never mutated post-construction. Candidate to move to `entt::ctx` or to the subsystem.
  2. **Read on a freshly-constructed view** — caller could fetch from subsystem just as easily.
  3. **Stored across handle copies** — bad signal; means the view's fragment state is being relied on for identity. Audit each individually.

- [ ] **Step 3: Pick a destination**

Based on the audit, choose one and document the choice:
- (a) Stay on `FCk_Registry` view. Simple migration; CTO concern stands but is tolerable.
- (b) Move to `UCk_EcsWorld_Subsystem_UE` and the editor mirror. Callers fetch from subsystem instead of view. Cleaner; one-pass change to ~N call sites.
- (c) Move to `entt::registry`'s context (`registry.ctx().emplace<FTransientEntityCtx>(...)`). Most idiomatic for entt; resolves on every access via the underlying registry pointer.

Default recommendation if the audit doesn't produce a strong signal otherwise: **(b)** — move to subsystem. Callers that need it ask the subsystem; the view stays small and stateless.

- [ ] **Step 4: Record the decision**

Add a comment to `CkRegistry.h` near the `_TransientEntity` field (or to the migration plan if removed) documenting the chosen destination and the rationale.

- [ ] **Step 5: Commit the decision note (no code change yet — that lands in Phase 2 alongside the view refactor)**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.h
git commit -m "docs(CkEcs): record _TransientEntity migration decision per CTO review"
```

### Task 0.7: Verify NetSerializer wire format claim

Per CTO review: don't trust the assertion that wire format goes through `_ReplicationDriver` only. Read the implementation and confirm.

**Files:**
- (research only — produces verified note in this plan)

- [ ] **Step 1: Read the implementation**

```bash
grep -n "Quantize\|Dequantize\|Serialize\|_Entity\b\|_Registry\b" \
    Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle.cpp \
    | head -100
```

Specifically inspect the bodies of:
- `FCk_Handle::NetSerialize`
- `UE::Net::FCk_HandleNetSerializer::Serialize` / `Deserialize`
- `UE::Net::FCk_HandleNetSerializer::Quantize` / `Dequantize`

- [ ] **Step 2: Confirm or refute the claim**

If both legacy `NetSerialize` and Iris `FCk_HandleNetSerializer` serialize **only** `_ReplicationDriver`, the claim holds and Phase 7 stays as a verification-only phase.

If either path serializes `_Entity` or other handle bytes directly, the receiver's slot index will not match the sender's. In that case, add a Phase 3 task: introduce a wire-stable identifier (e.g., the replication driver's NetGUID) that's translated on each side via the local slot table, and update both serializers to use it.

- [ ] **Step 3: Document findings**

Append a note to this plan (search for "Replication path" in self-review) recording the verified state of the wire format and any required follow-up.

- [ ] **Step 4: Commit (note-only commit)**

```bash
git add docs/superpowers/plans/2026-05-05-generational-handle-migration.md
git commit -m "docs(plan): verify NetSerializer wire format claim per CTO review"
```
```

---

## Phase 1 — Slot Table Infrastructure

Build the new infrastructure in isolation, before touching `FCk_Handle`. These tasks are pure additions and don't break anything.

### Task 1.1: `FCk_RegistryHandle` POD type

The handle that replaces the SharedPtr inside FCk_Handle.

**Files:**
- Create: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_Handle.h`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "CkRegistry_Handle.generated.h"

// --------------------------------------------------------------------------------------------------------------------
// FCk_RegistryHandle — generational reference to an entt::basic_registry slot.
//
// SlotIndex == INDEX_NONE represents an "unset" handle. Generation == 0 is reserved as a
// never-allocated sentinel; every successful Allocate produces a Generation >= 1.
//
// Trivially copyable. Stored by value inside FCk_Handle. Resolution to a real
// entt::basic_registry* is done via ck::registry_table::Resolve.

USTRUCT(BlueprintType)
struct CKECS_API FCk_RegistryHandle
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Ck|Registry")
    int32 SlotIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "Ck|Registry")
    uint32 Generation = 0;

public:
    auto operator==(const FCk_RegistryHandle& InOther) const -> bool
    {
        return SlotIndex == InOther.SlotIndex && Generation == InOther.Generation;
    }

    auto operator!=(const FCk_RegistryHandle& InOther) const -> bool
    {
        return NOT (*this == InOther);
    }

    auto IsSet() const -> bool { return SlotIndex != INDEX_NONE; }

    static auto Unset() -> FCk_RegistryHandle { return FCk_RegistryHandle{}; }
};

template<>
struct TStructOpsTypeTraits<FCk_RegistryHandle> : public TStructOpsTypeTraitsBase2<FCk_RegistryHandle>
{
    enum { WithIdenticalViaEquality = true };
};
```

- [ ] **Step 2: Verify it compiles**

Build the CkEcs module; the header is included by no other code yet, so a clean compile of CkEcs is sufficient.

```bash
# from a Developer Command Prompt at D:/Repos/CkPlugins (or via UnrealBuildTool)
# run your normal incremental build of CkEcs
```

Expected: clean build of CkEcs.

- [ ] **Step 3: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_Handle.h
git commit -m "feat(CkEcs): add FCk_RegistryHandle POD slot+gen type"
```

### Task 1.2: Slot table API header

Public surface of the slot table.

**Files:**
- Create: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.h`

- [ ] **Step 1: Create the header**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "CkEcs/Entity/CkEntity.h"
#include "CkEcs/Registry/CkRegistry_Handle.h"

#include "entt/entity/registry.hpp"

namespace ck::registry_table
{
    using EnttRegistryType = entt::basic_registry<FCk_Entity::IdType, std::allocator<FCk_Entity::IdType>>;

    // Allocate a slot pointing at InRegistry. Slot is reusable after Free.
    // Game-thread only — fires ensure in non-shipping if called off-thread.
    // Asserts if InRegistry is null.
    CKECS_API auto Allocate(EnttRegistryType* InRegistry) -> FCk_RegistryHandle;

    // Free a slot. Increments the slot's generation so future Resolve calls
    // with the old handle return null. The pointer stored in the slot is
    // nulled (the caller is responsible for actually deleting the registry).
    // Game-thread only.
    //
    // Stale-handle Free fires ensure in non-shipping (signals caller-side
    // double-free or subsystem double-deinit). Always idempotent — returns
    // without crashing in shipping or after the table has been destroyed.
    CKECS_API auto Free(FCk_RegistryHandle InHandle) -> void;

    // Resolve a handle to its registry pointer. STRICT default: fires
    // CK_ENSURE_IF_NOT in non-shipping when the handle is set but the slot
    // has been freed/recycled — i.e., the caller is using a stale handle.
    // Returns nullptr on stale handles, unset handles, or out-of-range
    // indices. This is the recommended access path; almost every caller
    // wants the staleness signal in development.
    CKECS_API auto Resolve(FCk_RegistryHandle InHandle) -> EnttRegistryType*;

    // Silent variant for callers that legitimately want "is this stale?"
    // semantics without firing an ensure. Used internally by
    // ck::IsValid / FCk_Handle::IsValid where a stale handle is a normal
    // condition (the check itself is the point), not a bug.
    CKECS_API auto TryResolve(FCk_RegistryHandle InHandle) -> EnttRegistryType*;
}
```

The previous name `Resolve_OrEnsure` is gone. The strict-by-default `Resolve` + silent `TryResolve` pair encodes discipline in the type rather than asking each call site to choose the right variant — bad calls fire ensures automatically, validity-check call sites opt into silence explicitly.

- [ ] **Step 2: Verify it compiles**

Build CkEcs; the header has no .cpp counterpart yet so it should compile but produce link errors only when something calls it. Don't call it yet.

- [ ] **Step 3: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.h
git commit -m "feat(CkEcs): declare slot-table API"
```

### Task 1.3: Slot table implementation

**Files:**
- Create: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp`

- [ ] **Step 1: Write the failing C++ unit test**

Create `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_SlotTable.spec.cpp`:

```cpp
#include "Misc/AutomationTest.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkRegistrySlotTable_BasicAllocateFreeResolve,
    "Ck.Registry.SlotTable.BasicAllocateFreeResolve",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCkRegistrySlotTable_BasicAllocateFreeResolve::RunTest(const FString& Parameters)
{
    using namespace ck::registry_table;

    EnttRegistryType Reg1;
    EnttRegistryType Reg2;

    auto H1 = Allocate(&Reg1);
    auto H2 = Allocate(&Reg2);

    TestTrue(TEXT("H1 resolves"),  Resolve(H1) == &Reg1);
    TestTrue(TEXT("H2 resolves"),  Resolve(H2) == &Reg2);
    TestTrue(TEXT("H1 != H2"),     H1 != H2);

    Free(H1);
    TestTrue(TEXT("Freed H1 resolves to nullptr"), Resolve(H1) == nullptr);
    TestTrue(TEXT("H2 still resolves"),            Resolve(H2) == &Reg2);

    auto H1Recycled = Allocate(&Reg1);
    TestTrue(TEXT("Recycled slot reused"),         H1Recycled.SlotIndex == H1.SlotIndex);
    TestTrue(TEXT("Recycled gen differs"),         H1Recycled.Generation != H1.Generation);
    TestTrue(TEXT("Stale handle still nullptr"),   Resolve(H1) == nullptr);
    TestTrue(TEXT("Recycled handle resolves"),     Resolve(H1Recycled) == &Reg1);

    Free(H2);
    Free(H1Recycled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkRegistrySlotTable_UnsetHandleResolvesNullSilently,
    "Ck.Registry.SlotTable.UnsetHandleResolvesNullSilently",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCkRegistrySlotTable_UnsetHandleResolvesNullSilently::RunTest(const FString& Parameters)
{
    using namespace ck::registry_table;
    auto Unset = FCk_RegistryHandle::Unset();

    TestTrue(TEXT("Unset handle resolves nullptr"),  Resolve(Unset) == nullptr);
    // Strict Resolve on Unset does NOT fire ensure (SlotIndex == INDEX_NONE
    // is the legitimate "no registry bound yet" sentinel, not a stale-handle).
    TestTrue(TEXT("Unset TryResolve nullptr"),       TryResolve(Unset) == nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkRegistrySlotTable_GenerationWrapDoesNotCollideWithSentinel,
    "Ck.Registry.SlotTable.GenerationWrapDoesNotCollideWithSentinel",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCkRegistrySlotTable_GenerationWrapDoesNotCollideWithSentinel::RunTest(const FString& Parameters)
{
    using namespace ck::registry_table;

    // Force a slot to wrap its 32-bit generation counter. Test hook fast-
    // forwards the generation just below UINT32_MAX so we don't need to
    // actually allocate/free 4 billion times.
    extern auto Debug_ForceSlotGenerationNearWrap_DoNotUseInProduction(int32 SlotIndex) -> void;

    EnttRegistryType Reg;
    auto H1 = Allocate(&Reg);
    Debug_ForceSlotGenerationNearWrap_DoNotUseInProduction(H1.SlotIndex);
    Free(H1);

    auto H2 = Allocate(&Reg);
    TestNotEqual(TEXT("Wrapped generation must skip 0 (the never-allocated sentinel)"),
        H2.Generation, uint32{0});

    Free(H2);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests Ck.Registry.SlotTable; Quit"`
Expected: FAIL — link error or "test not found" because the slot table has no implementation yet.

- [ ] **Step 3: Implement the slot table with phoenix-singleton storage**

Per CTO blocker #1: a function-local `static FState` is destroyed during DLL static-destruction. UObject destructors firing during editor shutdown can call `Free()` after that point, reintroducing the same lifetime-inversion bug we set out to fix — at the slot-table level. Fix: place the state in an `alignas` raw-bytes buffer, placement-new it on first access, and gate every method through an atomic "table alive" sentinel that flips to false in a `__attribute__((destructor))` / `atexit`-equivalent hook. Once the sentinel is false, `Free` is a silent no-op and `Resolve` / `TryResolve` return nullptr. Allocation after sentinel-flip is a hard error (fires fatal in non-shipping; returns Unset in shipping).

Create `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp`:

```cpp
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

#include "CkCore/Ensure/CkEnsure.h"

#include "Containers/Array.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"

#include <atomic>
#include <new>

// --------------------------------------------------------------------------------------------------------------------
// Why module-static (phoenix-singleton) and NOT a UEngineSubsystem:
//
//   1. UObject lifecycle: subsystems are torn down DURING editor shutdown,
//      BEFORE the final GC purge that destroys the long tail of unreachable
//      UObjects. Those late-purged UObjects can hold FCk_Handle fields whose
//      destructors call into the slot table — by which time a subsystem-
//      owned table would already be gone. That's the same lifetime-inversion
//      bug this whole migration was built to fix; we must not reintroduce it
//      one level up.
//   2. Module-static storage outlives the entire UObject lifecycle (DLL
//      unload happens after all UObjects are gone). The phoenix sentinel
//      makes Free()/Resolve()/TryResolve() safe through DLL teardown
//      regardless of caller order.
//   3. Trade-off accepted: the FState bytes are intentionally leaked at
//      process exit. This is a finite, process-lifetime leak — not a
//      growing one — and avoids a class of crash that's far worse than
//      a few KB of unfreed memory at shutdown.
//
// Live Coding caveat:
//   Live-Coding-patching CkEcs.dll while a PIE session is live can leave
//   the slot table in an inconsistent state (new DLL's table is empty;
//   existing handles still point at old DLL's storage). If you Live-Code
//   patch CkEcs, fully restart the editor before the next PIE session.
//   There is no automatic recovery.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::registry_table
{
    namespace
    {
        struct FSlot
        {
            EnttRegistryType* Registry   = nullptr;
            uint32            Generation = 0; // 0 = never-allocated sentinel.
        };

        struct FState
        {
            TArray<FSlot> Slots;
            TArray<int32> FreeList;

            FState()
            {
                Slots.Reserve(16);
                FreeList.Reserve(16);
            }
        };

        // Phoenix-singleton storage. The bytes are leaked at process exit; the
        // sentinel flips so further Free/Resolve calls become safe no-ops even
        // if other static destructors run in parallel. Aligned for FState.
        alignas(FState) static unsigned char     GStateStorage[sizeof(FState)];
        static std::atomic<bool>                  GStateAlive{false};
        static FState*                            GStatePtr = nullptr;

        // First-touch initializer. Idempotent. Game-thread only — Allocate/Free
        // are serialized on the game thread, so no double-init race.
        auto Get_State() -> FState*
        {
            if (NOT GStateAlive.load(std::memory_order_acquire))
            {
                if (GStatePtr == nullptr)
                {
                    GStatePtr = ::new (&GStateStorage) FState{};
                    GStateAlive.store(true, std::memory_order_release);
                    // Note: we deliberately do NOT register an atexit handler
                    // here. The sentinel-flip happens via the Lifecycle hook
                    // below, called explicitly from CkEcs's ShutdownModule.
                }
            }
            return GStatePtr;
        }

        struct FStateLifecycle
        {
            // Called from FCkEcsModule::ShutdownModule. Flips sentinel; future
            // Free/Resolve calls become silent no-ops. We deliberately do NOT
            // run ~FState (that's a leak, but a finite, process-lifetime one).
            static auto Flip_Dead() -> void
            {
                GStateAlive.store(false, std::memory_order_release);
            }
        };
    }

    // Public test hooks (non-shipping) used by the Phase 0 / Phase 1 test suite.
    // Kept in this TU so they share the namespace's anonymous state.
#if !UE_BUILD_SHIPPING
    auto Debug_SimulateTableDestruction_DoNotUseInProduction() -> void
    {
        FStateLifecycle::Flip_Dead();
    }

    auto Debug_ForceSlotGenerationNearWrap_DoNotUseInProduction(int32 SlotIndex) -> void
    {
        auto* State = Get_State();
        if (State == nullptr) { return; }
        if (State->Slots.IsValidIndex(SlotIndex))
        {
            State->Slots[SlotIndex].Generation = TNumericLimits<uint32>::Max() - 1;
        }
    }
#endif

    auto Allocate(EnttRegistryType* InRegistry) -> FCk_RegistryHandle
    {
        check(IsInGameThread());
        CK_ENSURE_IF_NOT(InRegistry != nullptr,
            TEXT("registry_table::Allocate: InRegistry must not be null"))
        { return FCk_RegistryHandle::Unset(); }

        auto* State = Get_State();
        CK_ENSURE_IF_NOT(State != nullptr,
            TEXT("registry_table::Allocate: state is dead — Allocate after module shutdown is a hard error"))
        { return FCk_RegistryHandle::Unset(); }

        int32 Index;
        if (State->FreeList.Num() > 0)
        {
            Index = State->FreeList.Pop(EAllowShrinking::No);
        }
        else
        {
            Index = State->Slots.Add(FSlot{});
        }

        auto& Slot = State->Slots[Index];
        Slot.Registry = InRegistry;
        // Skip generation 0 on wrap (Gen==0 is the "never-allocated" sentinel).
        ++Slot.Generation;
        if (Slot.Generation == 0) { ++Slot.Generation; }

        FCk_RegistryHandle Handle;
        Handle.SlotIndex  = Index;
        Handle.Generation = Slot.Generation;
        return Handle;
    }

    auto Free(FCk_RegistryHandle InHandle) -> void
    {
        check(IsInGameThread());
        if (NOT InHandle.IsSet()) { return; }

        // Sentinel-dead: silent no-op. This is the *whole point* of the phoenix
        // singleton — UObject destructors firing during DLL teardown must not
        // crash here just because the slot table's static is gone.
        if (NOT GStateAlive.load(std::memory_order_acquire)) { return; }

        auto* State = Get_State();
        if (State == nullptr) { return; }
        if (NOT State->Slots.IsValidIndex(InHandle.SlotIndex)) { return; }

        auto& Slot = State->Slots[InHandle.SlotIndex];
        if (Slot.Generation != InHandle.Generation)
        {
            // Stale Free signals subsystem-level double-deinit — fire ensure
            // in non-shipping so we surface the bug; remain idempotent in
            // shipping for safety.
            CK_ENSURE_IF_NOT(false,
                TEXT("registry_table::Free called with stale handle (slot {} expected gen {} but is at {}). "
                     "Likely subsystem double-deinit or caller-side double-free."),
                InHandle.SlotIndex, InHandle.Generation, Slot.Generation)
            { return; }
        }

        Slot.Registry = nullptr;
        ++Slot.Generation;
        if (Slot.Generation == 0) { ++Slot.Generation; }
        State->FreeList.Push(InHandle.SlotIndex);
    }

    auto TryResolve(FCk_RegistryHandle InHandle) -> EnttRegistryType*
    {
        // Resolve is read-only and called from many paths (incl. validity
        // checks). Non-shipping check: still expects game thread. Shipping:
        // honor-system, since contention is realistically nil.
#if !UE_BUILD_SHIPPING
        ensureMsgf(IsInGameThread(),
            TEXT("registry_table::TryResolve called from non-game thread"));
#endif
        if (NOT InHandle.IsSet()) { return nullptr; }
        if (NOT GStateAlive.load(std::memory_order_acquire)) { return nullptr; }

        auto* State = Get_State();
        if (State == nullptr) { return nullptr; }
        if (NOT State->Slots.IsValidIndex(InHandle.SlotIndex)) { return nullptr; }

        const auto& Slot = State->Slots[InHandle.SlotIndex];
        if (Slot.Generation != InHandle.Generation) { return nullptr; }
        return Slot.Registry;
    }

    auto Resolve(FCk_RegistryHandle InHandle) -> EnttRegistryType*
    {
        // Strict default. Fire ensure in non-shipping if the handle is set
        // but the slot is stale — that's a programming bug worth surfacing.
        if (NOT InHandle.IsSet()) { return nullptr; }
        if (NOT GStateAlive.load(std::memory_order_acquire)) { return nullptr; }

        auto* State = Get_State();
        if (State == nullptr) { return nullptr; }

        if (NOT State->Slots.IsValidIndex(InHandle.SlotIndex))
        {
            CK_ENSURE_IF_NOT(false,
                TEXT("Stale FCk_RegistryHandle: SlotIndex {} out of range (table has {} slots)"),
                InHandle.SlotIndex, State->Slots.Num())
            { return nullptr; }
        }

        const auto& Slot = State->Slots[InHandle.SlotIndex];
        if (Slot.Generation != InHandle.Generation)
        {
            CK_ENSURE_IF_NOT(false,
                TEXT("Stale FCk_RegistryHandle: slot {} expected gen {} but slot is at gen {}"),
                InHandle.SlotIndex, InHandle.Generation, Slot.Generation)
            { return nullptr; }
        }
        return Slot.Registry;
    }
}
```

- [ ] **Step 4: Wire the lifecycle hook into `FCkEcsModule::ShutdownModule`**

Modify `Plugins/CkFoundation/Source/CkEcs/CkEcs_Module.cpp`:

```cpp
auto FCkEcsModule::ShutdownModule() -> void
{
    // Flip the slot table's "alive" sentinel BEFORE Super::ShutdownModule
    // returns. After this call, Free()/Resolve()/TryResolve() are safe
    // no-ops for any UObject destructors that fire later in the DLL
    // teardown sequence — the phoenix singleton's whole purpose.
    extern auto Debug_FlipSlotTable_Dead_InternalUseOnly() -> void;
    // (named "Internal" rather than "Debug" because it ships; alternatively
    // expose a public `ck::registry_table::ShutdownTable()` symbol.)

    return IModuleInterface::ShutdownModule();
}
```

Decide between exposing `FStateLifecycle::Flip_Dead` as a public `ck::registry_table::ShutdownTable()` or via a friend declaration. Pick whichever is more idiomatic for the codebase (likely public, named `ShutdownTable`).

- [ ] **Step 5: Run the test to verify it passes**

Run: `UnrealEditor-Cmd.exe ... -ExecCmds="Automation RunTests Ck.Registry.SlotTable Ck.Registry.LifetimeInversion.FreeAfterTableDestruction; Quit"`
Expected: all four slot-table tests PASS, plus `FreeAfterTableDestruction` from the Phase 0 test file PASSES (it's testing this hook).

- [ ] **Step 6: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp \
        Plugins/CkFoundation/Source/CkEcs/CkEcs_Module.cpp \
        Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_SlotTable.spec.cpp
git commit -m "feat(CkEcs): slot-table with phoenix-singleton storage + game-thread enforcement + gen-wrap skip"
```

---

## Phase 2 — `FCk_Registry` becomes a non-owning view

Refactor `FCk_Registry` to wrap an `entt::basic_registry*` resolved on demand from a `FCk_RegistryHandle`. The same public method surface (`Add`, `Get`, `View`, etc.) stays — implementations dispatch through the slot-table-resolved pointer.

### Task 2.1: New `FCk_Registry` header

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.h`

- [ ] **Step 1: Replace the storage and special-member declarations**

Replace the `private:` section near the bottom (currently containing `ck::TPtrWrapper<InternalRegistryPtrType> _InternalRegistry;` and the atomic and copy/move/dtor declarations) with:

```cpp
private:
    FCk_RegistryHandle _RegistryHandle;
    EntityType         _TransientEntity;

public:
    // FCk_Registry is now a non-owning view. Trivially copyable / movable.
    // Default ctor produces an unbound view; resolve to nullptr.
    FCk_Registry() = default;

    explicit FCk_Registry(FCk_RegistryHandle InHandle, EntityType InTransientEntity)
        : _RegistryHandle(InHandle)
        , _TransientEntity(InTransientEntity)
    {}

    CK_PROPERTY_GET(_RegistryHandle);
    CK_PROPERTY_GET(_TransientEntity);
```

Add at the top of the file (after existing includes):

```cpp
#include "CkEcs/Registry/CkRegistry_Handle.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"
```

Remove the old TSharedPtr-related types and fields:

- Remove: `using InternalRegistryPtrType = TSharedPtr<InternalRegistryType>;`
- Remove: the explicit copy/move ctor / op= declarations and the `~FCk_Registry()` declaration.
- Remove: `Debug_GetSharedRefCount`, `Debug_GetInternalRegistryPtr` (these are replaced by direct slot-table queries in tests).
- Remove: `Shutdown()` (no longer needed; cycles are impossible).
- Remove: `_IsInParallelRegion` `std::atomic<bool>` member and its `BeginParallelRegion`/`EndParallelRegion`/`AssertNotInParallelRegion` plumbing — keep only if you still want the parallel-region debug check, but rewrite it to use a side-channel global rather than a per-FCk_Registry-instance atomic. **Decision: keep the parallel-region check as a global on the slot table to avoid bloating FCk_Registry.** Move it to `ck::registry_table::BeginParallelRegion(FCk_RegistryHandle)` / `EndParallelRegion(FCk_RegistryHandle)`.

- [ ] **Step 2: Update internal registry access throughout `CkRegistry.h`**

Every site currently reading `_InternalRegistry->X` (e.g., `_InternalRegistry->create()`, `_InternalRegistry->valid(...)`, `_InternalRegistry->Add<T>(...)`) must instead resolve via the slot table. Add a private helper:

```cpp
private:
    auto Resolve() -> ck::registry_table::EnttRegistryType*
    {
        return ck::registry_table::Resolve(_RegistryHandle);
    }
    auto Resolve() const -> const ck::registry_table::EnttRegistryType*
    {
        return ck::registry_table::Resolve(_RegistryHandle);
    }
```

Then replace every `_InternalRegistry->X` with `Resolve()->X`. For sites that previously called `Resolve()->X` without checking validity, wrap with `CK_ENSURE_IF_NOT(Resolve() != nullptr, ...)` first.

This is mechanical; do it once with a single pass. Search and verify:

```bash
grep -n "_InternalRegistry" Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.h
```

Expected after change: zero matches.

- [ ] **Step 3: Build CkEcs**

Expected: many compile errors in callers of FCk_Registry (we haven't updated FCk_Handle yet). That's fine for this commit — the next phase fixes them.

- [ ] **Step 4: Stash before commit, verify FCk_Registry header compiles in isolation**

A quick sanity check: write a tiny throwaway `.cpp` that includes `CkRegistry.h` and `CkRegistry_SlotTable.h`, instantiates an `FCk_Registry`, and calls a few methods. If the .cpp compiles clean, FCk_Registry is structurally sound.

```cpp
// Throwaway, do not commit.
#include "CkEcs/Registry/CkRegistry.h"
static auto _Sanity() -> void
{
    auto R = FCk_Registry{};
    auto H = R.Get_RegistryHandle();
    (void)H;
}
```

- [ ] **Step 5: Commit (header-only change, dependents will break)**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.h
git commit -m "refactor(CkEcs): FCk_Registry now a non-owning view via FCk_RegistryHandle (WIP)"
```

### Task 2.2: Update `CkRegistry.cpp`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.cpp`

- [ ] **Step 1: Strip out diagnostic logging and explicit special members**

Delete the entire `[CK_REGISTRY_LC]` macro, the `Get_NextRegistryLifecycleSeq` helper, `Do_CheckSuspiciousRegistryRefCount`, and all the explicit copy/move/dtor implementations. Replace with a slim file that only defines:

- The constructor that takes `FCk_RegistryHandle` (already inline in header — possibly remove from .cpp).
- Any methods declared but not defined inline (e.g., `CreateEntity`, `DestroyEntity`, `IsValid`, `GetTypeHash`).

Keep these implementations but route through `Resolve()` instead of `_InternalRegistry`:

```cpp
auto FCk_Registry::CreateEntity() -> EntityType
{
    auto* Reg = Resolve();
    CK_ENSURE_IF_NOT(Reg != nullptr,
        TEXT("FCk_Registry::CreateEntity: registry handle is stale or unset"))
    { return EntityType{}; }

    const auto Created = EntityType{Reg->create()};
    CK_ENSURE_IF_NOT(Reg->orphan(Created.Get_ID()),
        TEXT("Newly-created entity {} still has components attached"),
        static_cast<int32>(Created.Get_ID()))
    { return EntityType{}; }
    return Created;
}

// ... and so on for each previously-defined method.
```

- [ ] **Step 2: Build CkEcs**

Expected: clean build of CkEcs internals; callers (FCk_Handle) still broken — that's Phase 3.

- [ ] **Step 3: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry.cpp
git commit -m "refactor(CkEcs): route FCk_Registry methods through slot-table resolution"
```

---

## Phase 3 — `FCk_Handle` new layout

Now the load-bearing change: replace `TOptional<FCk_Registry>` storage with `FCk_RegistryHandle`. After this phase the type is trivially copyable.

> **Execution note (per CTO review):** Run this phase **inline** in the active session, not via a fresh subagent per task. The layout change cascades template-instantiation errors across 80+ modules; resolving those efficiently requires the operator's accumulated context. Resume subagent-driven execution from Phase 4 onward.

### Task 3.1: Update `FCk_Handle` declaration

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle.h`

- [ ] **Step 1: Replace the storage**

In the `protected:` section currently containing `_Entity`, `_Registry`, `_ReplicationDriver`, `_Mapper`, `_Fragments`, change to:

```cpp
protected:
    UPROPERTY(BlueprintReadOnly, NotReplicated)
    FCk_Entity _Entity;

    // Replaces TOptional<FCk_Registry>. Trivially copyable, no SharedPtr.
    UPROPERTY()
    FCk_RegistryHandle _RegistryHandle;

private:
    UPROPERTY()
    TWeakObjectPtr<class UCk_Ecs_ReplicatedObject_UE> _ReplicationDriver;

#if NOT CK_DISABLE_ECS_HANDLE_DEBUGGING
    const struct FEntity_FragmentMapper* _Mapper = nullptr;
#endif

#if WITH_EDITORONLY_DATA
private:
    UPROPERTY(NotReplicated, Transient)
    TWeakObjectPtr<class UCk_Handle_FragmentsDebug> _Fragments = nullptr;
#endif
```

- [ ] **Step 2: Remove the diagnostic helpers**

Delete the `Debug_GetRegistryRefCount` and `Debug_IsRegistrySet` declarations added during the diagnostic phase.

- [ ] **Step 3: Remove special-member declarations**

Delete:
- `FCk_Handle();`
- `FCk_Handle(EntityType, const RegistryType&);`
- `FCk_Handle(ThisType&&) noexcept;`
- `FCk_Handle(const ThisType&);`
- `auto operator=(ThisType InOther) -> ThisType&;`
- `~FCk_Handle();`

Replace with a single defaulted constructor and let the compiler synthesize copy/move/destructor:

```cpp
public:
    FCk_Handle() = default;
    FCk_Handle(EntityType InEntity, FCk_RegistryHandle InRegistry);
```

The single non-trivial constructor stays because callers want `(entity, registry)` construction. Implementation in `.cpp`.

- [ ] **Step 4: Update `TStructOpsTypeTraits<FCk_Handle>`**

Remove `WithCopy = true` (irrelevant for a trivially-copyable type). Keep `WithIdenticalViaEquality` and `WithNetSerializer`:

```cpp
template<>
struct TStructOpsTypeTraits<FCk_Handle> : public TStructOpsTypeTraitsBase2<FCk_Handle>
{
    enum
    {
        WithIdenticalViaEquality = true,
        WithNetSerializer        = true,
    };
};
```

- [ ] **Step 5: Update `operator->` / `operator*` AND rename `Get_Registry` → `Get_RegistryView`**

Per CTO concern #6: a `Handle.Get_Registry()` returning by value where it previously returned `FCk_Registry&` is a landmine — `auto& R = Handle.Get_Registry();` keeps compiling but binds to a temporary, and discovering each such site reactively in Phase 6 is fragile. **Force the issue with a rename**: every call site becomes a compile error, and we deal with each explicitly.

```cpp
public:
    // operator-> / operator* keep their names; UE convention favors them and
    // they're already widely understood to produce small temporaries.
    auto operator->() -> FCk_Registry;
    auto operator->() const -> const FCk_Registry;
    auto operator*() -> FCk_Registry;
    auto operator*() const -> const FCk_Registry;

    // Renamed from Get_Registry. The "View" suffix signals "non-owning,
    // returned by value, do not bind to a reference." Old name is removed
    // — no shim. Compiler-driven migration of all call sites.
    auto Get_RegistryView()       -> FCk_Registry;
    auto Get_RegistryView() const -> const FCk_Registry;
```

The previous `Get_Registry()` symbol is intentionally **deleted, not deprecated**. Every existing call must be updated, and the compiler will list them.

- [ ] **Step 6: Project-wide rename pre-flight (before commit)**

```bash
grep -rn "Handle\.Get_Registry\|Handle->Get_Registry\|->Get_Registry\b" Plugins/CkFoundation/Source
```

Expected output: a list of N call sites. Each will become a compile error after the header rename. Capture the count to gauge fix-up work for Phase 6.

- [ ] **Step 7: Build (will fail in CkHandle.cpp and at all `Get_Registry` callers)**

Expected: errors at every `Get_Registry` call site across the codebase. Don't commit yet — pair with Task 3.2 and the call-site sweep.

- [ ] **Step 6: Build (will fail in CkHandle.cpp)**

Expected: errors point at .cpp, not .h. Don't commit yet — pair with Task 3.2.

### Task 3.2: Update `CkHandle.cpp`

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle.cpp`

- [ ] **Step 1: Strip out diagnostic apparatus**

Delete:
- The entire `[CK_HANDLE_LC]` macro and `Get_NextLifecycleSeq` helper.
- The `Do_CheckSuspiciousRefCount` namespace function.
- The `<atomic>` include if no longer used.
- The `[FIRST_DTOR_FLAGS_CHECK]` block in the destructor.

- [ ] **Step 2: Replace special-member implementations**

Delete the previous explicit copy ctor / move ctor / op= / dtor / `Debug_GetRegistryRefCount` / `Debug_IsRegistrySet` definitions. Replace with the single non-trivial constructor:

```cpp
FCk_Handle::FCk_Handle(EntityType InEntity, FCk_RegistryHandle InRegistry)
    : _Entity(InEntity)
    , _RegistryHandle(InRegistry)
{
    DoUpdate_FragmentDebugInfo_Blueprints();
}
```

`DoUpdate_FragmentDebugInfo_Blueprints` may need adjusting — review whether it relied on the old TOptional state. If yes, reroute through `_RegistryHandle`-based resolution.

- [ ] **Step 3: Replace `Get_Registry`/`operator->`/`operator*`**

```cpp
auto FCk_Handle::operator->() -> FCk_Registry
{
    CK_ENSURE_IF_NOT(IsRegistryValid(),
        TEXT("FCk_Handle::operator->: registry handle stale or unset"))
    { return FCk_Registry{}; }
    return FCk_Registry{_RegistryHandle, /*transient*/ FCk_Entity{}};
}

auto FCk_Handle::operator->() const -> const FCk_Registry
{
    CK_ENSURE_IF_NOT(IsRegistryValid(),
        TEXT("FCk_Handle::operator-> const: registry handle stale or unset"))
    { return FCk_Registry{}; }
    return FCk_Registry{_RegistryHandle, FCk_Entity{}};
}

// operator* and Get_Registry follow the same pattern.

auto FCk_Handle::IsRegistryValid() const -> bool
{
    return ck::registry_table::Resolve(_RegistryHandle) != nullptr;
}
```

- [ ] **Step 4: Update `IsValid` paths**

Replace `_Registry.IsSet() && ck::IsValid(*_Registry) && _Registry->IsValid(_Entity)` with `IsRegistryValid() && Resolve()->valid(_Entity.Get_ID())`.

(Where `Resolve` is the slot-table call — bring it in via include.)

- [ ] **Step 5: Build CkEcs**

Expected: compile errors elsewhere in CkEcs that bypass the public API (e.g., friends manipulating `_Registry` directly). Each one is a small fix:
- `CkHandle_ReadOnly.cpp` `_Entity = ...` direct write — now `_Entity` is still accessible from the friend, just verify it still compiles.
- `UCk_Utils_EntityReplicationDriver_UE` accesses — verify each.
- `UE::Net::FCk_HandleNetSerializer` accesses — only touches `_ReplicationDriver`, should still work.

Patch each. Don't gold-plate — keep the changes minimal.

- [ ] **Step 6: Build the rest of CkFoundation**

Expected: a wave of compile errors across modules where templated `Add`/`Get`/`View` definitions in `CkHandle.h` are instantiated. Most should be transparent fixes (the API is unchanged), but watch for sites that grabbed `auto& Reg = Handle.Get_Registry();` — those are now binding to a temporary. Convert to `auto Reg = Handle.Get_Registry();` (by value, since `FCk_Registry` is small).

- [ ] **Step 7: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle.h \
        Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle.cpp \
        Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle_ReadOnly.cpp
git commit -m "refactor(CkEcs): FCk_Handle stores FCk_RegistryHandle, type is trivially copyable"
```

### Task 3.3: Update `FCk_Handle_TypeSafe` and the macro

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle_TypeSafe.h`

- [ ] **Step 1: Update `TStructOpsTypeTraits<FCk_Handle_TypeSafe>`**

Remove `WithCopy = true`. Keep the rest.

- [ ] **Step 2: Update the macro `CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE`**

Remove `WithCopy = true` from the macro body so all 77 generated typesafe traits drop the now-irrelevant flag. The `static_assert(sizeof(_HandleType_) == sizeof(FCk_Handle))` stays — both base and derived now have the same trivial layout.

- [ ] **Step 3: Build everything**

Expected: clean build of all 77 typesafe handles. `static_assert` should still hold (both base and derived contain the same fields, so layout is identical).

If `static_assert` fails: confirm that no typesafe handle adds any new UPROPERTYs of its own (per the codebase convention — they're "tagging types" with no extra storage). If one does, that's a pre-existing bug; flag it back to the user.

- [ ] **Step 4: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle_TypeSafe.h
git commit -m "refactor(CkEcs): drop WithCopy from typesafe handle traits (no longer needed)"
```

### Task 3.5: HARD GATE — full editor-cycle smoke pass before Phase 4

Per CTO guardrail #2: reflection-driven bugs from a layout change tend to surface as silent UPROPERTY round-trip loss or save corruption that the AutoTest suite won't catch. **Stop here. Do not start Phase 4 until every smoke step below passes.**

If any step fails: triage and fix BEFORE proceeding. Do not compound layout-related bugs under Phase 4 changes.

**Files:**
- (none modified — this is verification only)

- [ ] **Step 1: Clean rebuild**

```bash
# From Developer Command Prompt at D:/Repos/CkPlugins
# Force a clean build of the affected modules:
UnrealBuildTool ... -Clean
UnrealBuildTool ... -Build
```

Expected: builds clean.

- [ ] **Step 2: Open the editor cleanly**

Launch the editor from a normal working state (no debugger). Watch the Output Log during startup.

Expected: zero `Ensure failed`, zero `Error:` lines beyond the project's known baseline (compare to typical clean startup before migration began).

- [ ] **Step 3: PIE start/stop ×10**

In the editor, press Play, wait 1 second, press Stop. Repeat 10 times.

Expected: editor stays alive every cycle. No crash, no silent termination, no `Ensure failed` for stale-handle access during normal play.

If `Ensure failed` fires from `ck::registry_table::Resolve` (the strict-default variant): triage. Stale-handle access during normal play is the bug we're fixing — the migration is supposed to eliminate it, not surface it as a routine ensure.

- [ ] **Step 4: Open a level containing FCk_Handle UPROPERTY usage**

Pick a level that uses one of the 4 `EditAnywhere` handle fields (e.g., a level with a `CameraShake` setup or an `InteractionResolver` setup). Open it in the editor.

Expected: opens cleanly. UPROPERTY values are preserved (handle fields show "Unset" since the bytes don't survive the layout change — that's expected per "no backward compat", and is what Task 8.5's PR-description deliverable warns about).

- [ ] **Step 5: Save the level, close the editor**

Save the level. Close the editor. No crash on save. No crash on close.

- [ ] **Step 6: Reopen the editor; reopen the level fresh**

Cold-start the editor. Open the level just saved.

Expected: opens cleanly. Any UPROPERTY values that were re-set during step 4–5 round-trip correctly. No silent property loss for non-handle fields. Handle fields are still "Unset" (the levels haven't been re-authored yet — that's PR-description scope per Task 8.5).

- [ ] **Step 7: Capture the smoke-test result**

```bash
# Append a line to the plan note section confirming smoke passed:
echo "Phase 3 smoke pass: $(date -u +%Y-%m-%dT%H:%M:%SZ) — clean" >> docs/superpowers/plans/2026-05-05-generational-handle-migration-smoke.md
git add docs/superpowers/plans/2026-05-05-generational-handle-migration-smoke.md
git commit -m "test(CkRegistry): Phase 3 smoke pass — editor lifecycle verified clean"
```

**Phase 4 is gated on this commit existing.** If smoke fails, no Phase 4 commit is made; investigate and re-attempt.

---

## Phase 4 — Subsystem ownership of registry

The registry is now allocated/freed explicitly in the world subsystem.

### Task 4.1: `UCk_EcsWorld_Subsystem_UE` owns the entt registry

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsWorld_Subsystem.h`
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsWorld_Subsystem.cpp`

- [ ] **Step 1: Replace `FCk_Registry _Registry` with owned entt registry + slot handle**

In the header, change the member declaration:

```cpp
private:
    // Owns the underlying entt registry. Slot is registered with
    // ck::registry_table on Initialize; freed on Deinitialize.
    TUniquePtr<ck::registry_table::EnttRegistryType> _OwnedRegistry;
    FCk_RegistryHandle                                _RegistryHandle;
    FCk_Entity                                        _TransientEntity;

public:
    // Per Phase 0 _TransientEntity audit: if Task 0.6 chose option (b) move-
    // to-subsystem, this getter is now the canonical access for the transient
    // entity. If (c) move-to-entt-ctx, this returns it from registry context.
    // If (a) keep-on-view, the view ctor takes _TransientEntity.
    auto Get_RegistryView() -> FCk_Registry
    {
        return FCk_Registry{_RegistryHandle, _TransientEntity};
    }

    auto Get_TransientEntity() const -> FCk_Entity { return _TransientEntity; }
```

- [ ] **Step 2: Implement `Initialize` and `Deinitialize`**

In the .cpp (`UCk_EcsWorld_Subsystem_UE::Initialize` / `Deinitialize`):

```cpp
auto UCk_EcsWorld_Subsystem_UE::Initialize(FSubsystemCollectionBase& Collection) -> void
{
    Super::Initialize(Collection);

    _OwnedRegistry  = MakeUnique<ck::registry_table::EnttRegistryType>();
    _RegistryHandle = ck::registry_table::Allocate(_OwnedRegistry.Get());

    _TransientEntity = FCk_Entity{_OwnedRegistry->create()};
}

auto UCk_EcsWorld_Subsystem_UE::Deinitialize() -> void
{
    // Free the slot FIRST so any outstanding handle resolves to nullptr from
    // here on. Then destroy the entt registry. Order matters: we want any
    // ghost handle access between these two calls to fail safe (resolve →
    // nullptr → IsValid false), not access freed memory.
    ck::registry_table::Free(_RegistryHandle);
    _RegistryHandle = FCk_RegistryHandle::Unset();
    _OwnedRegistry.Reset();
    _TransientEntity = FCk_Entity{};

    Super::Deinitialize();
}
```

- [ ] **Step 3: Build CkEcs**

Expected: compile errors at sites that previously grabbed `_Registry` as `FCk_Registry&` from the subsystem. Update to use `Get_Registry()` (by value).

- [ ] **Step 4: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsWorld_Subsystem.h \
        Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsWorld_Subsystem.cpp
git commit -m "refactor(CkEcs): UCk_EcsWorld_Subsystem_UE owns entt registry + slot handle"
```

### Task 4.2: `UCk_EcsEditor_Subsystem` mirror

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsEditor_Subsystem.h`
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsEditor_Subsystem.cpp`

- [ ] **Step 1: Mirror Task 4.1**

Same pattern — owned `TUniquePtr`, `FCk_RegistryHandle`, allocate on `Initialize`, free on `Deinitialize`.

- [ ] **Step 2: Build, commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsEditor_Subsystem.h \
        Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Subsystem/CkEcsEditor_Subsystem.cpp
git commit -m "refactor(CkEcs): UCk_EcsEditor_Subsystem mirrors world subsystem ownership"
```

---

## Phase 5 — Remove diagnostic apparatus and stale concepts

### Task 5.1: Strip diagnostic logging from module startup

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/CkEcs_Module.cpp`
- Modify: `Plugins/CkFoundation/Source/CkDynamic/CkDynamic_Module.cpp`
- Modify: `Plugins/CkFoundation/Source/CkAssetExporter/CkAssetExporter_Module.cpp`
- Modify: `Plugins/CkFoundation/Source/CkPmg/CkPmg_Module.cpp`

- [ ] **Step 1: Remove `[CK_LAYOUT_DIAG]` and `[CK_FLAGS_DIAG]` log statements**

For each module, restore the `StartupModule` / `ShutdownModule` to their pre-diagnostic state. Diff against the original (use `git log` on each file to find the commit that added the diagnostics; revert those specific lines).

Quick verification:

```bash
grep -rn "CK_LAYOUT_DIAG\|CK_FLAGS_DIAG\|CK_HANDLE_LC\|CK_REGISTRY_LC\|BAD_REFCOUNT" Plugins/CkFoundation/Source
```

Expected: zero matches after this step.

- [ ] **Step 2: Build, run a basic AutoTest to verify nothing's broken**

Run: `Ck.Registry.SlotTable.BasicAllocateFreeResolve` — should still pass.

- [ ] **Step 3: Commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs/CkEcs_Module.cpp \
        Plugins/CkFoundation/Source/CkDynamic/CkDynamic_Module.cpp \
        Plugins/CkFoundation/Source/CkAssetExporter/CkAssetExporter_Module.cpp \
        Plugins/CkFoundation/Source/CkPmg/CkPmg_Module.cpp
git commit -m "chore(CkEcs): remove migration-era diagnostic logging from module startups"
```

### Task 5.2: Audit for `Shutdown()` callers and remove

**Files:**
- Modify: anywhere in `Plugins/CkFoundation/Source` that calls `FCk_Registry::Shutdown()`

- [ ] **Step 1: Find call sites**

```bash
grep -rn "FCk_Registry::Shutdown\|->Shutdown(" Plugins/CkFoundation/Source/CkEcs
```

- [ ] **Step 2: Remove or replace each call**

`Shutdown()` was a cycle-breaker for SharedPtr cycles. Without SharedPtrs, it's a no-op. If any call site expected it to also clear-the-entt-registry-but-keep-the-FCk_Registry-alive, replace with direct `Reg->clear()` via a public helper instead.

- [ ] **Step 3: Build, commit**

```bash
git add Plugins/CkFoundation/Source/CkEcs
git commit -m "refactor(CkEcs): remove FCk_Registry::Shutdown (no SharedPtr cycles to break)"
```

### Task 5.3: Restore `UCk_Processor_Script_Base_UE::BeginDestroy` if it was added as a stop-gap

If during the diagnostic phase a `BeginDestroy` override was added to clear `_Handle`, remove it now. The migration removes the bug it was working around.

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Processor/CkProcessor_Script.h` / `.cpp`

- [ ] **Step 1: Check whether a stop-gap was added**

```bash
grep -n "BeginDestroy\|_Handle = FCk_Handle" Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Processor/CkProcessor_Script.cpp
```

- [ ] **Step 2: If present, remove it**

The new representation makes the dangling-handle scenario harmless — `_Handle`'s slot+gen will resolve to nullptr safely after the registry is freed.

- [ ] **Step 3: Build, commit (no-op commit if not needed)**

---

## Phase 6 — AngelScript integration audit

The auto-bound USTRUCT layout for `FCk_Handle` changed (different fields, smaller size). UE's AS auto-binding regenerates from reflection at startup, so most of this is automatic — but the `opImplConv` reinterpret-cast bindings on typesafe handles are sensitive to layout.

### Task 6.1: Verify byte-layout invariant still holds

**Files:**
- (none modified)

- [ ] **Step 1: Inspect the static_assert in `CkHandle_TypeSafe.h`**

```bash
grep -A2 "Type-Safe Handle should be EXACTLY" Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Handle/CkHandle_TypeSafe.h
```

Expected: still present, both invocations (base typesafe and per-derived). They protect the AS reinterpret_cast bindings.

- [ ] **Step 2: Build all 77 typesafe handles**

Build the entire CkFoundation. Any `static_assert` failure here indicates a layout mismatch that breaks AS bindings. Fix by re-checking the new FCk_Handle layout against the typesafe subclass's fields.

### Task 6.2: Sweep all `Get_RegistryView` call sites (compiler-driven)

The `Get_Registry` → `Get_RegistryView` rename in Phase 3 produces compile errors at every former call site. Walk the build's error list and fix each. Most are mechanical:

- `auto& R = Handle.Get_Registry();` → `auto R = Handle.Get_RegistryView();`
- `Handle.Get_Registry().X(...)` → `Handle.Get_RegistryView().X(...)`
- Subsystem callers that grabbed `_Registry` as `FCk_Registry&` → use the renamed subsystem getter (`Get_RegistryView()`).

- [ ] **Step 1: Build the full project, capture the error list**

```bash
# UnrealBuildTool produces a structured error list — pipe to a file:
UnrealBuildTool ... > build-errors.log 2>&1
grep -E "Get_Registry\b" build-errors.log | sort -u | wc -l
```

Expected: a finite count. If it's >200 we should plan time accordingly; the fix is mechanical but volume matters.

- [ ] **Step 2: Fix each call site**

Mechanical: each error site converts to the new name and value semantics. Commit in small batches by module to keep history clean.

- [ ] **Step 3: Run full AutoTest suite via the editor**

```
Window → Test Automation → Filter: "CkAutoTest_" → Run All
```

Compare failures to `docs/superpowers/plans/2026-05-05-generational-handle-migration-baseline-failures.md` (captured in Pre-Flight PF.3). Expected: no NEW failures vs the baseline. Any failure not in the baseline is a regression introduced by the rename — investigate before continuing.

---

## Phase 7 — Replication path validation

The wire format goes through `_ReplicationDriver`, not the registry pointer. No code changes expected, but verify with a network test.

### Task 7.1: Build and run a 2-player network AutoTest

**Files:**
- (none modified, just verification)

- [ ] **Step 1: Find an existing replication AutoTest**

```bash
ls Plugins/CkTests/Script/CkReplication
```

- [ ] **Step 2: Run it via the Test Automation panel**

Expected: PASS. If a test relied on `FCk_Handle`'s exact byte serialization (it shouldn't — only `_ReplicationDriver` is on the wire), update the test to the new representation.

- [ ] **Step 3: Commit any test updates**

---

## Phase 8 — Final validation

### Task 8.1: Re-run Phase 0 tests

The C++ lifetime-inversion test (`Ck.Registry.LifetimeInversion.HandleSurvivesRegistry`) now expected to pass cleanly. All AS AutoTests likewise.

- [ ] **Step 1: Run all Phase 0 tests**

Run via Test Automation. Expected: ALL PASS, including `LifetimeInversion.HandleSurvivesRegistry` which previously failed/crashed on current code.

### Task 8.2: PIE start/stop manual stress

The original repro: open the editor, start PIE, stop PIE, repeat ~10 times. The original crash should be gone.

- [ ] **Step 1: Manually cycle PIE 10+ times in the editor**

Expected: editor stays alive, no crashes, no `BAD_REFCOUNT_DETECTED` lines (the detector is removed but the underlying issue is gone).

- [ ] **Step 2: Inspect the log for any unexpected ensures**

```bash
grep -nE "Ensure failed|FCk_RegistryHandle" "<editor log path>"
```

Expected: no `Ensure failed` for stale-handle access during normal play. Stale-handle ensures may legitimately fire from `ck::registry_table::Resolve` (the strict default) if old code holds a handle past PIE-stop — those failures are *good* (they tell us where to clear handles in `BeginDestroy` or via a lifecycle hook).

### Task 8.3: Full regression sweep — diff against baseline

- [ ] **Step 1: Run the entire AutoTest suite**

```
Window → Test Automation → Filter: "CkAutoTest_" → Run All
```

- [ ] **Step 2: Diff against baseline**

Compare the failure list to `docs/superpowers/plans/2026-05-05-generational-handle-migration-baseline-failures.md`. Expected: same set of failures, OR fewer (the migration may incidentally fix some unrelated pre-existing failures — that's fine).

Any **new** failure that's not in the baseline is a migration regression — fix and re-run before merging.

### Task 8.4: Microbenchmark — handle copy/destroy throughput

Per CTO recommendation: a small benchmark, not for go/no-go but for record. Lets us cite a number the next time someone asks why we did this.

**Files:**
- Create: `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_HandleCopyBenchmark.cpp`

- [ ] **Step 1: Write the benchmark**

```cpp
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Registry/CkRegistry_SlotTable.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkRegistry_HandleCopyDestroyBenchmark,
    "Ck.Registry.Benchmark.HandleCopyDestroy",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FCkRegistry_HandleCopyDestroyBenchmark::RunTest(const FString& Parameters)
{
    using namespace ck::registry_table;

    auto* OwnedReg = new EnttRegistryType{};
    auto Slot = Allocate(OwnedReg);
    const auto Source = FCk_Handle{FCk_Entity{OwnedReg->create()}, Slot};

    constexpr int32 N = 10'000'000;
    const double Start = FPlatformTime::Seconds();

    for (int32 i = 0; i < N; ++i)
    {
        auto Copy = Source;            // copy ctor
        (void)Copy;                    // ensure not optimized away
    }                                  // dtor on every iteration

    const double Elapsed = FPlatformTime::Seconds() - Start;
    AddInfo(FString::Printf(TEXT("Handle copy+destroy: %d iterations in %.3fs (%.1f ns/iter)"),
        N, Elapsed, (Elapsed / N) * 1.0e9));

    Free(Slot);
    delete OwnedReg;
    return true;
}

#endif
```

- [ ] **Step 2: Run and record the number**

```
Window → Test Automation → Filter: "Ck.Registry.Benchmark" → Run
```

Note the ns/iter result in the PR description. For reference: pre-migration TSharedPtr copy+destroy is ~30–60 ns/iter (atomic-bound); post-migration trivial copy of 12 bytes should be ~1–3 ns/iter.

- [ ] **Step 3: Commit**

```bash
git add Plugins/CkTests/Source/CkTests/Private/UnitTests/CkRegistry_HandleCopyBenchmark.cpp
git commit -m "test(CkRegistry): handle copy/destroy throughput microbenchmark"
```

### Task 8.5: Document affected `.uasset` paths in PR description

Per CTO answer #4: re-authoring the 4 `EditAnywhere` handle-field consumers shouldn't be a scavenger hunt. List them in the PR description so reviewers and content authors know what's broken by design.

- [ ] **Step 1: Run a project-wide asset query**

```bash
# Find any .uasset files that reference the affected fragment types.
# Use the editor's Asset Registry from Tools → "Reference Viewer" on each of:
#   - FCk_Camera_ShakeData
#   - FCk_InteractionResolver_Target (×2 USTRUCT variants)
#   - FCk_Objective_Data
#   - FCk_StateTree_ContextData
```

- [ ] **Step 2: Compile the list into the PR description**

Format: a markdown bullet list under a `### Re-author required` heading, with the asset paths and the field that needs re-setting.

- [ ] **Step 3: Commit (PR-description-only; no code change)**

(No commit for this task; it's a PR-description deliverable.)

### Task 8.6: Postmortem — `UObject` GC ordering vs SharedPtr-owned external state

Per CTO forward ask: write up a one-page postmortem so the next engineer who builds a subsystem exposing resource handles to UObjects has a written rule to follow.

**Files:**
- Create: `docs/postmortems/2026-05-05-uobject-gc-vs-shared-resource-handles.md`

- [ ] **Step 1: Write the postmortem**

```markdown
# Postmortem: UObject GC ordering vs SharedPtr-owned external state

## Date
2026-05-05 (bug discovered) → 2026-05-NN (migration completed)

## Summary
A `TSharedPtr<entt::basic_registry>` was held both by the world subsystem
(intended owner) and by `FCk_Handle` value copies stored as UPROPERTY
fields on UObjects across the codebase. During PIE-stop the engine's
GC purge destroyed those UObjects in an order that wasn't guaranteed
to follow ownership intent — the registry's last legitimate SharedPtr
release ran before the dangling handles' destructors did, freeing the
TSharedPtr's control block and leaving subsequent dtors decrementing
freed memory. Eventually the heap allocator detected corruption and
fast-fail-terminated the process. UE's normal crash handler was bypassed
(no dump, no reporter UI) because `__fastfail` skips SEH.

## Symptoms
- Editor would silently vanish after several PIE start/stop cycles.
- No crash dump in `Saved/Crashes/`, no Windows Error Reporting popup.
- Logs ended mid-frame with no `Fatal` markers.
- Reproduction was timing-sensitive — depended on which UObject-holders
  happened to be GC'd before the world subsystem.

## Root cause
**UObject GC ordering does not respect SharedPtr ownership semantics.**
Multiple UObjects holding `TSharedPtr` copies of the same control block
will be destroyed in arbitrary order during a GC sweep. If the
"primary owner" UObject is destroyed before any of the secondary
holders, the secondary holders become dangling SharedPtrs whose
destructors will scribble on freed memory.

## Why we didn't catch it sooner
- TSharedPtr's safety contract (refcount accounting on copy/destroy)
  is correct in isolation — there's no static check that catches
  "two UObjects sharing a control block in arbitrary GC order is
  unsafe."
- The bug only manifested across a PIE start/stop boundary, in a
  reproducer that required ~3 minutes of PIE activity. Single-session
  testing didn't surface it.
- AutoTests didn't cover the "UObject lifecycle inversion" class
  of scenario.

## Fix
Replaced `FCk_Handle`'s SharedPtr storage with a generational handle
(slot index + generation). The world subsystem owns the registry
(`TUniquePtr`) and a slot in a process-static slot table. UObject
holders carry only POD slot+gen pairs — no smart-pointer ownership.
On stale handle access, the slot table's generation mismatch returns
a clean nullptr instead of dereferencing freed memory.

## The rule, going forward

> **If a subsystem owns a non-UObject resource (registry, handle pool,
> external library state) and exposes references to that resource to
> UObjects, those references must NOT be smart pointers that share
> ownership. Use a generational-handle pattern with the subsystem
> retaining sole ownership.**

Applies to any future subsystem that:
- Owns a `TSharedPtr` / `TUniquePtr` to non-UObject state.
- Hands out references that may end up stored on UObject UPROPERTY fields.
- Has a teardown ordering that interleaves with UE's GC purge.

The generational handle pattern is documented in
[`CkRegistry_SlotTable.cpp`](../../Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp)
as the reference implementation for this codebase.

## Diagnostics that found it (for the next investigation)
- Per-instance lifecycle logging on copy/move/destroy + the registry's
  refcount at each event. Made the dangling-decrement pattern visible:
  refcount marched 0 → -1 → -2 → ... → -12.
- Static-flag check on `STRUCT_CopyNative` ruled out copy-semantics
  bugs early; let us focus on lifetime.
- Stack trace at first negative refcount pointed straight at the
  UObject holder via `FObjectPurge::DestroyObjects` →
  `<UObjectClass>::~vector_deleting_destructor`.

## Detection going forward
The migration leaves `ck::registry_table::Resolve` as a
strict-by-default API that fires `CK_ENSURE_IF_NOT` in non-shipping
when a stale handle is used. New instances of this bug class would
surface as a stale-handle ensure rather than silent corruption.
```

- [ ] **Step 2: Commit**

```bash
git add docs/postmortems/2026-05-05-uobject-gc-vs-shared-resource-handles.md
git commit -m "docs(postmortem): UObject GC vs SharedPtr-owned external state"
```

### Task 8.7: Final commit + tag + PR

- [ ] **Step 1: Tag the migration completion**

```bash
git tag generational-handle-migration-complete
git push origin generational-handle-migration-complete
```

- [ ] **Step 2: Push branch**

```bash
git push origin feature/generational-handle-migration
```

- [ ] **Step 3: Open the PR (draft until reviewed)**

Body should include:
- Summary linking the migration plan and postmortem.
- The "Re-author required" list of `.uasset` paths from Task 8.5.
- The microbenchmark numbers from Task 8.4.
- Test-plan checklist confirming Phase 3 smoke (Task 3.5), Task 8.1–8.4 results, full sweep against baseline.

Single PR landing per CTO direction. Merge button stays disabled until all checklist items are signed off.

---

## CTO-Review Updates Summary

This plan was reviewed and the following changes were made before kickoff:

**Plan-blockers addressed:**
1. **Phoenix-singleton storage** for the slot table (Task 1.3) — `alignas` raw bytes + atomic sentinel + explicit `ShutdownTable` from `FCkEcsModule::ShutdownModule`. Eliminates the static-destruction-order trap.
2. **`_TransientEntity` audit task** (Task 0.6) — decide upfront whether the field stays on the view, moves to the subsystem, or to `entt::ctx`. Prevents baking in API debt.
3. **Lifetime-inversion test rewritten in C++** (Task 0.3) — synthetic `UObject` against the slot table directly, exercising the same GC-ordering bucket as `UCk_Processor_Script_Base_UE`. The previous AS-side test couldn't reach the bug.

**Lesser concerns addressed:**
4. **`check(IsInGameThread())`** in Allocate/Free (Task 1.3); `ensureMsgf` in TryResolve.
5. **Generation-zero skip on wrap** (Task 1.3) + dedicated test (`GenerationWrapDoesNotCollideWithSentinel`).
6. **`Get_Registry` → `Get_RegistryView` rename** (Task 3.1, Step 5–7) — every call site is a compile error; no shim. Phase 6 sweep is compiler-driven.
7. **Discipline-by-type** for resolution (Task 1.2) — strict `Resolve` is the default with ensure-on-stale; silent `TryResolve` is the explicit opt-in for validity-check paths.
8. **NetSerializer wire-format claim verified** as a Phase 0 task (Task 0.7) before being relied on in Phase 7.
9. **Live Coding caveat documented** as a comment block at the top of `CkRegistry_SlotTable.cpp`.
10. **Free-on-stale fires ensure** in non-shipping (Task 1.3) — surfaces double-deinit; remains idempotent in shipping.

**Direct answers to CTO questions:**
- Static FState table: kept (with phoenix wrapper).
- `FCk_Registry` public type: kept, but the renamed `Get_RegistryView` makes value semantics impossible to miss.
- Microbenchmark: added (Task 8.4) — for record, not go/no-go.
- Affected `.uasset` paths: enumerated as a PR-description deliverable (Task 8.5).

**Execution model:**
- Phases 0–2, 4–8 run via subagent-driven execution (one subagent per task).
- **Phase 3 runs inline.** Layout cascade across 80+ modules requires accumulated context for efficient triage.

---

## CTO-Approval Guardrails (final round)

The CTO approved the plan with four additional guardrails. All baked into the tasks below:

1. **Rationale comment in `CkRegistry_SlotTable.cpp`** — Task 1.3 Step 3. 6–8 line block at file top covering the "why static + phoenix and not `UEngineSubsystem`" argument. Future engineers won't have to re-litigate this in six months.

2. **Hard gate after Phase 3** — new Task 3.5. Editor-cycle smoke pass (open editor → PIE-start/stop ×10 → save level → close editor → reopen → verify) before Phase 4 begins. Catches reflection-driven UPROPERTY round-trip loss that AutoTests won't surface.

3. **Pre-Flight section** — branch + tag + AutoTest baseline. `pre-generational-handle-migration` tag on `main` parent commit before branching to `feature/generational-handle-migration`. Single PR landing; per-phase commits preserved for archaeology; merge button disabled until Task 8.7 complete.

4. **Postmortem at completion** — Task 8.6. One-page postmortem at `docs/postmortems/2026-05-05-uobject-gc-vs-shared-resource-handles.md` documenting the bug class and the rule going forward, so future subsystems with non-UObject-owned resources don't repeat the pattern.

**Operational details (per user direction):**
- Single PR landing strategy.
- Branch name: `feature/generational-handle-migration`.
- Postmortem location: `docs/postmortems/`.
- AutoTest filter scope during migration: `Ck.Registry.*` (C++) + `CkAutoTest_Registry_*` (AS) for per-task iteration. Full `CkAutoTest_*` sweep only in Phase 6 / Phase 8, diffed against baseline from Pre-Flight PF.3.
- BusterBlock's diagnostic-era code is left as-is; bumping the submodule pointer post-migration overwrites it.

---

## Self-Review Notes

- **Scope coverage** vs the 7 UObject classes / 32 USTRUCT fragments / 77 typesafe handles found by Explore: all are covered transitively. The `CK_GENERATED_BODY_HANDLE_TYPESAFE` macro change covers all 77 typesafe types in one shot. The 32 USTRUCT fragment fields don't need individual changes — their `FCk_Handle` member just becomes the new layout transparently. The 7 UObject classes likewise.
- **Asset compat** is intentionally not covered (per user direction — no backward compat needed; clean slate). Affected `.uasset` paths are enumerated as a PR-description deliverable in Task 8.5.
- **Microbenchmark added in Task 8.4** for record-keeping per CTO recommendation. Not gating; just numbers we can cite later.
- **Type consistency check**: `FCk_RegistryHandle` (Phase 1) ↔ `FCk_Registry::_RegistryHandle` (Phase 2) ↔ `FCk_Handle::_RegistryHandle` (Phase 3) ↔ `UCk_EcsWorld_Subsystem_UE::_RegistryHandle` (Phase 4) — all consistent. The `Get_Registry` → `Get_RegistryView` rename is consistent across the subsystem (Phase 4) and the handle (Phase 3).
- **Slot-table access from non-game-thread** is enforced via `check(IsInGameThread())` in Allocate/Free and `ensureMsgf` in TryResolve. Parallel processor reads need verification in Phase 6 — if any hit this, the follow-up is to atomic-ize the slot generation rather than to relax the check.
- **The `_TransientEntity` field** is the subject of a Phase 0 audit (Task 0.6). The plan currently shows it on the view as a default; Task 0.6 may move it to the subsystem (recommended) or `entt::ctx` based on the audit's findings.
- **Phoenix-singleton storage** (Task 1.3) eliminates the static-destruction-order class of bug at the slot-table level — the same class of bug we set out to fix at the handle level.
- **Free-on-stale ensure** (Task 1.3) catches subsystem-level double-deinit in non-shipping; the operation remains idempotent in all configurations so safety isn't compromised in shipping.
