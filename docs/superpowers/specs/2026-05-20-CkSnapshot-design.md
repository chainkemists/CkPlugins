# CkSnapshot — Save/Load for CkFoundation (Design)

**Status:** Design v2 — addresses CTO review CHANGES REQUESTED (2026-05-21). Pending re-review.
**Date:** 2026-05-20 (v1), 2026-05-21 (v2)
**Author:** Saad (via brainstorming session)
**Module:** New tier-2 module `CkSnapshot` under `Plugins/CkFoundation/Source/`.
**Review:** `Plugins/CkFoundation/docs/reviews/2026-05-20-CkSnapshot-design-CTO-review.md`

## v2 changelog (what changed since CTO review)

Addressing sign-off conditions 1–6 and incorporating one issue uncovered during verification:

- **Section 3 rewritten.** Introduces `SerializeSnapshot(FArchive&, FSnapshotContext&)` intrusive method as the universal serialize entry point. USTRUCT fragments with no entity refs get a default implementation that calls `SerializeItem` (one-line opt-in survives for that class). Non-USTRUCT templated families (`TFragment_Attribute<>`, `TFragment_RecordOfEntities<>`, `FFragment_InventoryItem`, etc.) and any fragment carrying entity-handle refs implement `SerializeSnapshot` explicitly. Resolves sign-off #1.
- **Section 3 entity-handle remap.** Added the missing entity-ID remap path via `FSnapshotContext` wrapping `entt::continuous_loader`. Applies to USTRUCT fragments holding handle refs too (e.g. `FCk_InventoryItem_Spatial_ReplicatedEntry`), not only the non-USTRUCT record fragments — flagged during v1→v2 verification, not in original CTO review. Required for correctness.
- **Section 1 path #3 detailed.** Spells out (a) class-path persistence (`UClass*` serialized separately), (b) drop-spawn-params policy with author contract, (c) `_AssociatedEntity` rebind step on restore. Resolves sign-off #2.
- **Section 1 adds the fourth AS surface.** Long-lived signal subscriptions made post-`BeginPlay` do not survive snapshot; documented as an AS-author contract. Per CTO design observation.
- **Section 5 defines `FCk_Snapshot_Header` USTRUCT shape.** Field list committed. Resolves sign-off #6.
- **Section 6 adds two new autotests:** `Test_Snapshot_AS_UPROPERTY_SaveGame_Smoke` (sign-off #3 — verifies the AS frontend translates `UPROPERTY(SaveGame)` to `CPF_SaveGame`) and `Test_Snapshot_NormalWorkload_DoesNotTripFlush` (sign-off #4).
- **EnTT version 3.15.0 → 3.16.0 throughout.** Resolves sign-off #5.
- **Open Question 2 resolved** in spec body: blanket-snapshotable at the carrier level; payload USTRUCT gates fields via `meta=(SaveGame)`.
- **Open Question 3 resolved** in spec body: log + skip + LoadReport by default; `bRefuseLoadOnUnresolvedTypes` as opt-in setting on `UCk_Snapshot_Settings`.
- **Non-blocker improvements landed:** Convergent-flush iteration cap as CVar `ck.Snapshot.ConvergentFlush.MaxIterations` with per-iteration dirty counts; orphan-sweep gate moved to `UCk_Snapshot_Settings`; CkEcs tier-1 touch (the `FragmentIsSnapshotable` concept header) called out explicitly.
- **Out-of-scope follow-up:** Trivial PR to fix `Plugins/CkFoundation/CLAUDE.md:3,376` entt 3.15 references is queued separately, not landed in this spec.

---

## Goal

Add save/load to CkFoundation with the smallest possible per-feature dev burden. A feature author should opt a fragment into persistence with **one line** in the header (`using IsSnapshotable = void;`) and **per-field UPROPERTY `meta=(SaveGame)` tags**. Everything else — entity-graph traversal, byte serialization, version drift, actor↔entity rebinding on load, replication interplay — is framework infrastructure that the author doesn't touch.

Honest caveat (v2): the "one-line" promise holds for **USTRUCT fragments with no entity-handle refs** (most feature Params structs — the common author-facing case). Fragments that are non-USTRUCT templated families (e.g. `TFragment_Attribute<>`) or that hold entity handles also need a `SerializeSnapshot(FArchive&, FSnapshotContext&)` method on the fragment. For V1 the framework-author writes these methods once per templated family inside the `CkSnapshot` module work — feature authors above the templated layer do not. Full mechanism in Section 3.

V1 vertical-slice covers both **`CkFloatAttribute`** and **`CkInventory`** end-to-end (save, load, autotests, gym). Other CkFoundation features get tagged later as separate per-feature CTO-reviewed tasks.

## Non-goals (V1)

- Per-fragment custom migration callbacks (deferred — UE tagged-property + `CoreRedirects` covers V1 cases).
- Runtime-spawned-but-savable actors (deferred — V1 supports editor-placed bridged actors and pure-ECS sub-graphs).
- Cross-feature entity references via raw handle (deferred — V1 features use `Record` traversal, not raw cross-handle references).
- Client-side snapshot generation (V1 is server-only; clients re-derive via replication).
- Buffer compression.
- Per-feature rollout beyond CkFloatAttribute + CkInventory.

## Decisions locked-in during brainstorming

| # | Decision | Rationale |
|---|---|---|
| 1 | **Whole-world snapshot model** (Skyrim-style — entire ECS state captured/restored). | Matches the Rewind99 sim shape; alternative per-owner / per-feature models add coordination cost without payoff. |
| 2 | **Opt-in via fragment-level marker** (`using IsSnapshotable = void;`). | Explicit but copy-paste cheap; mirrors existing CkFoundation tag-type-alias conventions (`MarkedDirtyBy`). |
| 3 | **Per-field opt-in via UPROPERTY `meta=(SaveGame)`** inside a marked fragment. | UE convention; composes natively with `FObjectAndNameAsStringProxyArchive`. |
| 4 | **Level respawns actors; ECS re-binds via a `SaveKey` GUID fragment** (Option A from brainstorming). | Reuses UE's actor lifecycle, keeps level-designers in control, plays naturally with multiplayer client rejoin. |
| 5 | **Server-only save/load with "anytime, flush first" timing.** Server runs convergent flush of the processor graph, then snapshots; clients re-derive from server replication. | Server is the canonical state owner; replication path is the same one used at initial join — no separate client-restore code path. |
| 6 | **V1 feature coverage: `CkFloatAttribute` + `CkInventory` together.** | Inventory uses attributes (item stack-count is an `IntegerAttribute`); proving both validates the nested-graph case. |
| 7 | **Module name `CkSnapshot`** (chosen over `CkSave` / `CkPersistence` / `CkArchive`). | Matches the EnTT mechanism we're building on (`snapshot.hpp`). |

## Architecture summary

```
CkSnapshot/                                       (new tier-2 module)
├── Public/CkSnapshot/
│   ├── Context/
│   │   ├── CkSnapshot_Context.h/cpp              FSnapshotContext (entt continuous_loader wrapper)
│   │   └── CkSnapshot_FragmentRegistry.h/cpp     UScriptStruct* ↔ EnTT type ID ↔ DoSerializeSnapshot dispatch
│   ├── SaveKey/
│   │   ├── CkSnapshot_SaveKey_Fragment.h/cpp     FFragment_SaveKey (FGuid carrier)
│   │   └── CkSnapshot_SaveKey_Utils.h/cpp        Add/Get/Resolve utilities
│   ├── Archive/
│   │   ├── CkSnapshot_Archive_Writer.h/cpp       EnTT archive wrapping FObjectAndNameAsStringProxyArchive
│   │   └── CkSnapshot_Archive_Reader.h/cpp       Symmetric inverse
│   ├── Subsystem/
│   │   └── CkSnapshot_Subsystem.h/cpp            UGameInstanceSubsystem: Request_Save / Request_Load
│   ├── Settings/
│   │   └── CkSnapshot_Settings.h/cpp             UCk_Snapshot_Settings : UDeveloperSettings
│   ├── SaveGame/
│   │   ├── CkSnapshot_SaveGame.h/cpp             UCk_Snapshot_SaveGame : USaveGame
│   │   └── CkSnapshot_Header.h/cpp               FCk_Snapshot_Header USTRUCT (committed shape in Section 5)
│   └── Snapshot/
│       ├── CkSnapshot_Capture.h/cpp              EnTT integration + convergent flush
│       └── CkSnapshot_Restore.h/cpp              Load-side rebuild + SaveKey resolver publish + EntityScript rebind
└── CkSnapshot.Build.cs / Module / Log boilerplate

CkEcs/Public/CkEcs/Concepts/
└── CkSnapshot_Concepts.h                         FragmentIsSnapshotable, FragmentHasCustomSnapshotSerialize,
                                                  FragmentIsUStructSnapshotable (tier-1 touch — flagged)
```

**Dependency tier:** T2 — depends only on `CkEcs`, `CkCore`, `CkLog`, `CkThirdParty`. The `FragmentIsSnapshotable` concept lives in `CkEcs/Concepts/` so a `using IsSnapshotable = void;` line in a feature header doesn't add a dependency edge to `CkSnapshot`. Feature modules that need `SerializeSnapshot` (Tier B / C from Section 3) get `FSnapshotContext` via forward-declaration in a header-only helper — also no dep edge. Runtime registry discovers savable types via UE reflection at module startup.

**CkEcs tier-1 touch:** the concept header in `CkEcs/Concepts/` is the only edit outside the new module. The implementation plan calls this out explicitly so downstream consumers of `CkEcs` see the new header.

### The six new primitives

1. `ck::concepts::FragmentIsSnapshotable` + companion concepts (C++20 concepts; lives in `CkEcs`).
2. `ck::FSnapshotContext` (wraps `entt::continuous_loader` / `entt::snapshot` for entity-handle remap; passed to every `SerializeSnapshot` call).
3. `FFragment_SaveKey` (GUID fragment) + matching `_SaveKey` UPROPERTY on the EntityBridge component.
4. `UCk_Snapshot_SaveGame : USaveGame` (header + opaque snapshot buffer).
5. `UCk_Snapshot_Settings : UDeveloperSettings` (orphan-sweep gate, strict-mode toggles, build-hash refusal).
6. `UCk_Snapshot_Subsystem : UGameInstanceSubsystem` (5 UFUNCTIONs + 4 signals).

Every other piece is plumbing — including the per-family `SerializeSnapshot` methods on the V1-touched templated fragments listed in Section 6.

---

## Section 1 — The savable-fragment contract

### The opt-in line, honestly

A fragment opts in with **one line** + sometimes one additional method, depending on the fragment's shape:

```cpp
// Tier A — USTRUCT with no entity-handle refs (most Params structs):
USTRUCT(BlueprintType)
struct CKINVENTORY_API FCk_Fragment_Inventory_Spatial_ParamsData
{
    GENERATED_BODY()
    using IsSnapshotable = void;   // ← one line. No additional method needed.
    // Framework dispatches automatically through UScriptStruct::SerializeItem.

    UPROPERTY(EditAnywhere, meta=(SaveGame))
    FIntPoint _Dimensions;
    // ...
};

// Tier B / C — USTRUCT with entity refs, or non-USTRUCT templated family:
struct CKINVENTORY_API FFragment_InventoryItem  // non-USTRUCT
{
    CK_GENERATED_BODY(FFragment_InventoryItem);
    using IsSnapshotable = void;   // ← marker, plus...

    auto SerializeSnapshot(FArchive& Ar, FSnapshotContext& Ctx) -> void  // ← method
    {
        // (body — see Section 3 examples)
    }
    // ...
};
```

The framework dispatches per-tier (Section 3 explains the mechanism). The "one-line opt-in" is honest for **Tier A** (the common case — feature Params USTRUCTs with plain UPROPERTY data); **Tier B / C** require one additional method on the fragment. AS authors only ever see Tier A (Params USTRUCTs they declare) and the dynamic-fragment path #2 — they don't write SerializeSnapshot.

Detected via:

```cpp
namespace ck::concepts
{
    template <typename T>
    concept FragmentIsSnapshotable = requires { typename T::IsSnapshotable; };
}
```

Per-field opt-in via UE's existing `meta=(SaveGame)` convention. Both layers are intentional — the fragment marker says "this whole component matters across saves", the UPROPERTY meta picks the individual fields. UE's `ArIsSaveGame=true` flag on the archive (set by our writer) makes `SerializeItem` honor the meta automatically (for Tier A; Tiers B and C author the routing explicitly).

### Auto-discovery at startup

At `CkSnapshot` module startup we iterate `TObjectIterator<UScriptStruct>` (for Tier-A discovery) plus consult a hand-maintained type-erased registration table built up by `CK_REGISTER_SNAPSHOTABLE(FragmentType)` macros (catches Tiers B and C, which have no `UScriptStruct`). One `CK_REGISTER_SNAPSHOTABLE` line per snapshotable fragment goes in the existing `*_Fragment.cpp`. Startup logs the full list (`"CkSnapshot: 47 snapshotable fragments registered."`), so forgetting the macro is loud, not silent.

### Anti-misuse guardrails

- **Compile-time:** if a fragment has `IsSnapshotable` but lacks both `StaticStruct()` AND a `SerializeSnapshot` method, the registration `CK_REGISTER_SNAPSHOTABLE(T)` fails to compile with a `static_assert` pointing at the missing method.
- **Startup-time:** Registry diff against expected count; warn on unexpected change.
- **Save-time:** Entities with zero snapshotable fragments are skipped with a per-batch summary (not per-entity log spam).

### ⚠️ CTO REVIEW REQUIRED — AngelScript surface (revised for v2)

The v1 spec claimed AS gets snapshotting for free across three surfaces. After v1 review and verification, the claim is partially true: paths #1 and #2 hold cleanly. Path #3 is implementable but needs extra mechanism beyond a marker. A fourth surface (signal subscriptions) was also missed in v1. This subsection details all four.

#### Path #1 — AS calling into C++ fragments via `utils_*`

AS mutates the same C++ fragments that C++ mutates. The `IsSnapshotable` marker on the C++ side covers them — but as Section 3 explains, **most C++ fragments behind `utils_*` are non-USTRUCT templated families** (`TFragment_Attribute<>`, `TFragment_RecordOfEntities<>`, etc.) and need a `SerializeSnapshot` method on the C++ side. AS authors still see no change — the work is on the framework C++ side.

#### Path #2 — AS-declared dynamic fragments via `UCkDynamic_HandleDefinition` + `Add_Fragment`

We mark `FCk_Fragment_DynamicFragment_Data` (the carrier) snapshotable, blanket. The carrier IS a USTRUCT (verified) and `FInstancedStruct::Serialize` is type-aware:

```cpp
struct FCk_Fragment_DynamicFragment_Data
{
    GENERATED_BODY()
    using IsSnapshotable = void;
    UPROPERTY(meta=(SaveGame))
    FInstancedStruct _StructData;
};
```

**Open Question 2 resolved:** blanket-snapshotable at the **carrier** level; payload USTRUCTs gate fields at field-level via `meta=(SaveGame)`. Anything more granular adds AS-author surface area for no payoff. A payload USTRUCT with no `SaveGame` fields persists *structure but no data* — equivalent to a tag, which is sometimes useful and never harmful.

#### Path #3 — AS-authored `UCk_EntityScript_UE` subclasses (revised)

`FFragment_EntityScript_Current` (the fragment that holds the script instance) is **non-USTRUCT** and stores `TStrongObjectPtr<UCk_EntityScript_UE> _Script`. It serializes via a custom `SerializeSnapshot` method (Tier C from Section 3). The method handles all three sub-concerns:

**(a) Class path persistence.** The `UClass*` of the script is serialized separately from the instance:

```cpp
auto FFragment_EntityScript_Current::SerializeSnapshot(FArchive& Ar, FSnapshotContext& Ctx) -> void
{
    // 1. Class path. UE's archive serializes UClass* by path name when ArIsSaveGame=true.
    UClass* Class = (Ar.IsSaving() && _Script.IsValid()) ? _Script->GetClass() : nullptr;
    Ar << Class;
    if (Ar.IsLoading() && Class == nullptr)
    {
        // Class missing → loader records to LoadReport.SkippedScriptClasses, leaves _Script null.
        Ctx.RecordSkippedScriptClass_OnRead();
        return;
    }

    // 2. Instance UPROPERTYs. On load, spawn a fresh instance of the restored class
    //    before Serialize. UCk_EntityScript_UE inherits UObject::Serialize which honors
    //    ArIsSaveGame on UPROPERTY meta.
    if (Ar.IsLoading())
    {
        auto* Outer = /* world or context owner per spawn convention */;
        _Script = TStrongObjectPtr<UCk_EntityScript_UE>(NewObject<UCk_EntityScript_UE>(Outer, Class));
    }
    if (_Script.IsValid()) { _Script->Serialize(Ar); }

    // 3. _AssociatedEntity rebind. Handled by the orchestrator (Section 4) AFTER all
    //    fragments restore — not inside SerializeSnapshot, because the freshly-restored
    //    handle isn't known at fragment-deserialize time.
}
```

**(b) Spawn-params: dropped.** Per the v2 design decision (recorded in the v2 changelog), `FInstancedStruct _SpawnParams` from `FRequest_EntityScript_Replicate` is **not persisted**. Construct does NOT re-run on restore. The contract for authors:

> *Every Construct-side effect that should survive snapshot must be captured by a `UPROPERTY(SaveGame)` field on the EntityScript subclass. If a script needs to remember a value from spawn-params, it stores that value as its own UPROPERTY in `Construct`.*

This contract is documented in `Plugins/CkFoundation/Script/CLAUDE.md`'s Persistence section (V1 deliverable). The rationale: re-running Construct enqueues requests by convention (Construct typically calls `utils_*::Add` operations), so re-running it double-applies. Dropping spawn-params + relying on UPROPERTY(SaveGame) is the only safe default without forcing every author to make Construct idempotent.

**(c) `_AssociatedEntity` rebind.** `UCk_EntityScript_UE::_AssociatedEntity` is `UPROPERTY(Transient)` (verified), so `Serialize` correctly skips it under `ArIsSaveGame=true`. The orchestrator's load flow (Section 4 step 5.5) walks every restored EntityScript instance and sets `_AssociatedEntity` to the rebound `FCk_Handle` before any AS code calls `DoGet_ScriptEntity()`.

#### Path #4 (new — fourth AS-touching surface) — Long-lived signal subscriptions

When an AS script binds to a `CkSignal` mid-game (post-`BeginPlay`), the binding is held in the framework's signal delegate fragment, NOT on the EntityScript instance. On load, the entity is destroyed + re-restored via fresh `NewObject<>` + `Serialize`. Signal-binding fragments are runtime-only and not tagged snapshotable. **The subscription is gone.**

**Contract** (documented in `Script/CLAUDE.md` Persistence section, V1 deliverable):

> *Signal bindings do not survive snapshot. Bind in `BeginPlay` (which runs on every restore) — bindings made in response to gameplay events that fired post-BeginPlay are lost on save/load. Either re-issue them from `BeginPlay` using state captured in `UPROPERTY(SaveGame)` fields, or accept the loss.*

This is consistent with how UE actors handle level-reload (delegates bound in `BeginPlay` get re-bound on reload; ad-hoc bindings are lost). Calling it out explicitly prevents the first AS author who saves mid-tutorial-with-bound-UI-delegate from filing a "snapshot corrupts state" bug.

#### Type-rename caveat (Open Question 3 resolved)

If an AS class is renamed or deleted between save and load, that entity's script/fragment can't be restored. Default behavior: **log + skip + surface in `FCk_Snapshot_LoadReport.SkippedScriptClasses` / `SkippedDynamicTypes`.** The entity is counted in `EntitiesPartiallyRestored`.

Strict-mode opt-in: `UCk_Snapshot_Settings::bRefuseLoadOnUnresolvedTypes` (off by default, on for shipping builds that can't accept silent data loss). Same shape as `RefuseLoadsBelowBuildHash`. Players hitting strict-mode failure are shown a "save incompatible with this build" dialog rather than silently losing entities.

#### AS verification gate (Sign-off #3)

The AS frontend's behaviour on `UPROPERTY(SaveGame)` is **currently unverified in this codebase** (zero existing usages in `Plugins/CkFoundation/Script/`). V1 ships with a smoke autotest (`Test_Snapshot_AS_UPROPERTY_SaveGame_Smoke`, Section 6) that exercises an AS class with one `UPROPERTY(SaveGame)` field through a full save→mutate→load→assert cycle. If the AS frontend doesn't translate `(SaveGame)` to `CPF_SaveGame` on the compiled UClass, the smoke test fails and V1 cannot ship — we'd either patch the AS frontend or pivot path #3 to an alternative mechanism (e.g., a typed AS-side `Snapshot_*` interface).

---

## Section 2 — `FFragment_SaveKey` lifecycle

### Who needs a SaveKey

Only **bridged entities whose actor side re-appears via the level**:

- Player pawns (joined by PlayerState matching profile).
- Editor-placed actors with `EntityBridge` (shelves, registers, the back-office door).
- Runtime-spawned actors that should re-attach on load (deferred in V1 — see Non-goals).

Child entities (items inside an inventory, modifiers on an attribute, sub-attributes Min/Max/Current) do **not** get a SaveKey. They're restored by walking the parent's `Record` after the parent's SaveKey resolves.

### Assignment

| Spawn origin | When assigned | Where stored |
|---|---|---|
| Editor-placed actor | First `EntityBridge::SpawnEntity` (generates GUID if bridge has none); stamped on both fragment AND bridge UPROPERTY. UE serializes the bridge UPROPERTY into the level file. | Entity fragment + bridge UPROPERTY |
| Runtime-spawned bridged actor | Caller supplies GUID OR bridge generates and replicates. (V1 deferred — but design accommodates.) | Entity fragment + bridge UPROPERTY |
| Pure ECS entity (no bridge) | Never | n/a |

### Fragment shape

```cpp
struct CKSNAPSHOT_API FFragment_SaveKey
{
    CK_GENERATED_BODY(FFragment_SaveKey);
    using IsSnapshotable = void;

private:
    UPROPERTY(meta=(SaveGame))
    FGuid _Key;

public:
    CK_PROPERTY_GET(_Key);
    CK_DEFINE_CONSTRUCTORS(FFragment_SaveKey, _Key);
};
```

### Load-time resolution

```
1. Snapshot loader rehydrates entities into the EnTT registry (fresh IDs).
2. Loader sweeps view<FFragment_SaveKey> and publishes TMap<FGuid, FCk_Handle>
   into UCk_Snapshot_Subsystem::_SaveKey_Resolver.
3. Level streams in. Each EntityBridge::BeginPlay reads its own UPROPERTY _SaveKey:
   - Resolves → bind actor to existing entity; skip default "spawn fresh".
   - Present but unresolved → log warning, spawn fresh.
   - Absent → first-ever spawn, generate one, spawn fresh.
4. After grace window (2s or OnAllPlayersReady), sweep resolver — unconsumed
   entries are orphans (actor deleted from level between save and load).
   Default: destroy with summary log; optional PreserveOrphans setting.
```

### Drift handling

| Scenario | Behavior |
|---|---|
| Designer placed new actor after save | New actor spawns fresh entity (no match). Working as intended. |
| Designer deleted actor that was placed at save time | Saved entity orphaned → destroyed with summary log. |
| Designer moved a placed actor | Actor keeps UPROPERTY GUID → re-binds to saved entity at new transform. Entity state preserved, transform follows actor. |
| Designer duplicated an actor | Duplicates inherit GUID (UE quirk). **Detect at first BeginPlay, regenerate one, spawn fresh.** Documented in CkActor CLAUDE.md as gotcha. |

### Networking

`_SaveKey` is a `Replicated` UPROPERTY on the bridge. Server-assigned; clients never write the resolver — server's restored entity flows via standard fragment replication; the client's bridge reads its replicated `_SaveKey` for diagnostics only.

### Rationale for split storage

- **Fragment on entity** is what the snapshot reads/writes — ECS is source of truth.
- **UPROPERTY on bridge** is what the .umap persists — actor side lives in actor properties.
- Save time: read from entity fragment.
- Load time: rendezvous through the resolver.

Collapsing storage (e.g. only on actor) would mean the snapshot can't recover the mapping without the level loaded — we want snapshots to be diagnosable in isolation.

---

## Section 3 — The serialization adapter (EnTT ↔ UE)

### The honest opt-in story (revised after CTO v1 review)

The v1 spec claimed a single mechanism (`T::StaticStruct()->SerializeItem(...)`) would cover every snapshotable fragment. That doesn't compile against the fragment families CkFoundation actually uses — most ECS fragments below the per-feature Params USTRUCTs are non-USTRUCT C++ templates (`TFragment_Attribute<>`, `TFragment_RecordOfEntities<>`, `FFragment_InventoryItem`, `FFragment_EntityScript_Current`, etc.) declared with `CK_GENERATED_BODY` only — they have no `StaticStruct()`. Separately, ANY fragment (USTRUCT or not) that holds entity-handle references needs entity-ID remap on load — EnTT's snapshot specifically supports this via `continuous_loader`, but only if we route through it.

The revised mechanism: every snapshotable fragment implements **one method** — `SerializeSnapshot(FArchive&, FSnapshotContext&)`. The framework provides automatic dispatch for the trivial case (USTRUCT with no entity refs). Everything else writes the method explicitly.

```cpp
namespace ck::concepts
{
    template <typename T>
    concept FragmentIsSnapshotable = requires { typename T::IsSnapshotable; };

    template <typename T>
    concept FragmentHasCustomSnapshotSerialize =
        FragmentIsSnapshotable<T> &&
        requires (T& Fragment, FArchive& Ar, FSnapshotContext& Ctx)
            { Fragment.SerializeSnapshot(Ar, Ctx); };

    template <typename T>
    concept FragmentIsUStructSnapshotable =
        FragmentIsSnapshotable<T> &&
        NOT FragmentHasCustomSnapshotSerialize<T> &&
        requires { T::StaticStruct(); };
}
```

### The three opt-in tiers

| Tier | Fragment shape | What the author writes |
|---|---|---|
| **A — Auto** | USTRUCT, no entity-handle refs (most Params structs). | One line: `using IsSnapshotable = void;`. No SerializeSnapshot method needed. Framework dispatches to `T::StaticStruct()->SerializeItem(Ar, &Fragment, nullptr)` with `ArIsSaveGame=true`. |
| **B — Custom-USTRUCT** | USTRUCT with entity-handle refs (e.g. `FCk_InventoryItem_Spatial_ReplicatedEntry` carrying item handles, replicated-entry structs in general). | One line marker + a `SerializeSnapshot(Ar, Ctx)` method. Serializes fields one at a time; handle refs route through `Ctx.Snapshot_Handle(Ar, Handle)` so entity IDs remap on load. |
| **C — Non-USTRUCT** | Templated families like `TFragment_Attribute<H, T, Dir>`, `TFragment_RecordOfEntities<T>`, `FFragment_InventoryItem`, `FFragment_EntityScript_Current`. | One line marker + `SerializeSnapshot(Ar, Ctx)` defined on the template. Plain values use `Ar << _Field`; handle refs use `Ctx.Snapshot_Handle`. The body is written once per template, not per specialization. |

Honest cost per family:

- **Per-feature Params USTRUCTs (the common case)** — one `using` line. V1 promise stands for this class.
- **Non-USTRUCT templated families** — framework author writes the method once each for `TFragment_Attribute<>`, `TFragment_Attribute_PreviousValues<>`, `TFragment_AttributeModifier<>`, `TFragment_RecordOfEntities<T>`, `FFragment_InventoryItem`, `FFragment_EntityScript_Current`, `FFragment_RefillAccumulator`, and a handful of others. Each is ~5–15 lines. Total V1 surface is well under 200 lines across all the templated fragments V1 touches.
- **AS authors** — never see Tier C; they only encounter Tier A (Params USTRUCTs) and the dynamic-fragment payload path from Section 1.

### Dispatch

```cpp
namespace ck::detail
{
    template <typename T>
        requires ck::concepts::FragmentHasCustomSnapshotSerialize<T>
    auto DoSerializeSnapshot(FArchive& Ar, FSnapshotContext& Ctx, T& Fragment) -> void
    {
        Fragment.SerializeSnapshot(Ar, Ctx);
    }

    template <typename T>
        requires ck::concepts::FragmentIsUStructSnapshotable<T>
    auto DoSerializeSnapshot(FArchive& Ar, FSnapshotContext& /*Ctx*/, T& Fragment) -> void
    {
        T::StaticStruct()->SerializeItem(Ar, &Fragment, /*Defaults=*/nullptr);
    }
}
```

A fragment with only the `IsSnapshotable` marker but no `StaticStruct()` and no `SerializeSnapshot` method fails to match either overload → compile error at registration site, pointing at the missing method. Loud failure mode, no silent skip.

### EnTT integration

EnTT 3.16.0's `snapshot` / `snapshot_loader` iterate entities and dispatch each component to a user-supplied archive callable. We drive iteration from `CkSnapshot_FragmentRegistry`:

```cpp
for (const auto& Registered : SnapshotRegistry.GetAll())
    Registered.Save(EnttSnapshot, Archive);  // per-fragment closure
```

Each `Registered` knows its EnTT type ID and dispatches to `DoSerializeSnapshot<T>` for its fragment type.

### Archive + FSnapshotContext

```cpp
namespace ck
{
    // Threaded through every SerializeSnapshot call. Carries the entity-ID remap path.
    class FSnapshotContext
    {
        // On save: nullptr (writes raw entity IDs into the buffer).
        // On load: wraps entt::basic_continuous_loader, which remaps saved entity IDs
        // to freshly-allocated IDs in the live registry. Use for ANY handle ref.
        entt::basic_continuous_loader<entt::entity>* _Loader = nullptr; // load-only
        entt::basic_snapshot<entt::registry>*       _Saver  = nullptr; // save-only

    public:
        template <typename T_Handle>
        auto Snapshot_Handle(FArchive& Ar, T_Handle& InOutHandle) -> void;

        auto Snapshot_Entity(FArchive& Ar, entt::entity& InOutEntity) -> void;
    };

    class FSnapshotArchive_Writer
    {
        FObjectAndNameAsStringProxyArchive& _Proxy;
        FSnapshotContext*                   _Context = nullptr;

    public:
        explicit FSnapshotArchive_Writer(FObjectAndNameAsStringProxyArchive& InProxy)
            : _Proxy(InProxy) { _Proxy.ArIsSaveGame = true; }

        auto operator()(entt::entity InEntity) -> void;
        auto operator()(std::underlying_type_t<entt::entity> InSize) -> void;

        template <typename T> requires ck::concepts::FragmentIsSnapshotable<T>
        auto operator()(T& InFragment) -> void
        {
            ck::detail::DoSerializeSnapshot(_Proxy, *_Context, InFragment);
        }
    };
    // Reader is the symmetric inverse, with _Context._Loader populated instead of _Saver.
}
```

### Why entity-handle remap matters

EnTT entity IDs are integers allocated by the registry. After `registry.clear()` on load and a fresh re-allocation pass, the saved IDs no longer point at anything meaningful — they're garbage that happens to fit in a `uint32`. Without `continuous_loader`, an `FCk_Handle_Item` field saved as raw bytes restores as a stale ID. `ck::IsValid` would even still pass (the underlying registry might happen to have an entity at that ID), but it'd be the *wrong* entity — silent data corruption.

`continuous_loader` solves this by maintaining a save-time-ID → load-time-ID map as it deserializes; calls to `Snapshot_Handle` consult the map and rewrite the handle's entity ID. This only works if the entity ID is round-tripped through the loader, hence the `FSnapshotContext::Snapshot_Handle` indirection.

### Example author writeups

**Tier A — Params USTRUCT (no method needed):**

```cpp
USTRUCT(BlueprintType)
struct CKATTRIBUTE_API FCk_Fragment_FloatAttribute_ParamsData
{
    GENERATED_BODY()
    using IsSnapshotable = void;  // ← that's it

    UPROPERTY(EditAnywhere, meta=(SaveGame))
    FGameplayTag _Name;

    UPROPERTY(EditAnywhere, meta=(SaveGame))
    float _DefaultBase = 0.f;
    // ...
};
```

**Tier C — Non-USTRUCT templated family:**

```cpp
template <concepts::ValidAttributeHandleType T_HandleType, typename T_AttributeType, ECk_MinMaxCurrent T_ComponentTag>
struct TFragment_Attribute
{
    CK_GENERATED_BODY(TFragment_Attribute<T_HandleType COMMA T_AttributeType COMMA T_ComponentTag>);
    using IsSnapshotable = void;
    // ... existing fields _Base, _Final ...

    auto SerializeSnapshot(FArchive& Ar, FSnapshotContext& /*Ctx*/) -> void
    {
        Ar << _Base;
        Ar << _Final;
    }
};

template <typename T_Handle>
struct TFragment_RecordOfEntities
{
    CK_GENERATED_BODY(TFragment_RecordOfEntities<T_Handle>);
    using IsSnapshotable = void;
    TArray<T_Handle> _Entities;

    auto SerializeSnapshot(FArchive& Ar, FSnapshotContext& Ctx) -> void
    {
        int32 Num = _Entities.Num();
        Ar << Num;
        if (Ar.IsLoading()) { _Entities.SetNum(Num); }
        for (auto& Handle : _Entities) { Ctx.Snapshot_Handle(Ar, Handle); }
    }
};
```

**Tier B — USTRUCT with handle refs:**

```cpp
USTRUCT()
struct FCk_InventoryItem_Spatial_ReplicatedEntry
{
    GENERATED_BODY()
    using IsSnapshotable = void;

    UPROPERTY(meta=(SaveGame)) FCk_Handle_Item _ItemHandle;
    UPROPERTY(meta=(SaveGame)) FIntPoint _Coordinate;
    UPROPERTY(meta=(SaveGame)) ECk_CardinalRotation _Rotation;

    auto SerializeSnapshot(FArchive& Ar, FSnapshotContext& Ctx) -> void
    {
        Ctx.Snapshot_Handle(Ar, _ItemHandle);
        Ar << _Coordinate;
        Ar << _Rotation;
    }
};
```

### On-disk format

```cpp
UCLASS()
class CKSNAPSHOT_API UCk_Snapshot_SaveGame : public USaveGame
{
    GENERATED_BODY()
    UPROPERTY() FCk_Snapshot_Header _Header;       // see Section 5 for committed field list
    UPROPERTY() TArray<uint8> _SnapshotBytes;      // EnTT-driven proxy-archive output (entity topology + payloads)
};
```

The header USTRUCT is always readable — even if `_SnapshotBytes` is from an incompatible build, the header still parses, so version checks and LoadReport diagnostics work. The buffer holds the entity topology + every snapshotable fragment's payload via the three-tier dispatch above. Header field list is committed in Section 5.

### Why two layers (USaveGame wrapping byte buffer)

USaveGame requires fixed UPROPERTY layout, but the savable-fragment set is dynamic (~47+ types in V1, growing per feature module). The byte-buffer-inside-USaveGame approach lets the savable set be a runtime decision while keeping the wrapper schema stable. Cost: we own our own version check against the header (no UE auto-versioning of the buffer contents). Migration mechanism in Section 5.

### UObject reference handling

`FObjectAndNameAsStringProxyArchive` serializes UObject refs by full path name. Works for:

- Asset refs (`TObjectPtr<UDataAsset>`, `TSoftObjectPtr<UMaterial>`) — stable asset paths.
- World-stable refs (level actors, persistent components) — path includes level + actor name.

Does **NOT** round-trip transient runtime UObjects (anonymous `NewObject<>`s with no stable name). Rule for snapshotable fragments: any `UObject*` UPROPERTY tagged `meta=(SaveGame)` must point to an asset or stable-path object. Transient cached refs leave the meta off — re-derived on load. Documented in `CkSnapshot/CLAUDE.md` anti-patterns.

---

## Section 4 — Orchestration (save + load + error reporting)

`UCk_Snapshot_Subsystem` (UGameInstanceSubsystem) owns the API.

### Save flow

```
Server only. Async — returns immediately, work runs across 1–N frames.

[Frame N]
  1. AUTH CHECK
     - Ensure-fail if Get_HasAuthority() is false. Server only.
     - Acquire global "snapshot in progress" tag on world entity to block concurrent saves.

  2. FIRE PRE-SAVE SIGNAL
     - UUtils_Signal_Snapshot_OnPreSave(WorldHandle).
     - Synchronous handlers only (no deferred work).

  3. CONVERGENT FLUSH (fixed-point loop)
     - Run processor graph one tick.
     - If any *Requests fragment still has entries → tick again.
     - Iteration cap from CVar ck.Snapshot.ConvergentFlush.MaxIterations (default 8).
       On cap: log every fragment still dirty WITH per-iteration dirty-count history
       (so a feature that grows dirty work each iteration is distinguishable from
       one that's just slow), fail with ECk_SnapshotResult::Failed_NotQuiescent.

[Frame N+1]  (or same frame for small worlds)
  4. CAPTURE
     - FMemoryWriter → FObjectAndNameAsStringProxyArchive → FSnapshotArchive_Writer.
     - Drive entt::snapshot over every registered snapshotable fragment.
     - Header stamped: format version, engine version, plugin build hash, UTC
       timestamp, world asset path, manifest (per-fragment-type counts + byte lengths).

  5. WRITE
     - UCk_Snapshot_SaveGame populated with header + bytes.
     - UGameplayStatics::SaveGameToSlotAsync.
     - Callback: release tag, broadcast OnSaveComplete, fire user delegate.
```

### Load flow

```
Server only. Clients re-derive via standard replication.

[Frame M]
  1. AUTH CHECK + OnPreLoad signal (features tear down session-specific UI).

  2. READ
     - LoadGameFromSlotAsync, header validated first.
     - Incompatible header → bail with Failed_IncompatibleSave. No partial attempts.

[Frame M+1+]
  3. WIPE ECS
     - Destroy every entity with FFragment_SaveKey OR a snapshotable fragment
       (via normal teardown path so EndPlay processors run).
     - Pure-runtime entities also destroyed; they respawn from level reload.
     - registry.clear() resets EnTT ID space.

  4. RESTORE
     - Reverse of capture, driven by entt::continuous_loader (the "continuous" form,
       which performs entity-ID remapping as each component is deserialized — see
       Section 3 entity-handle remap rationale). FSnapshotContext._Loader carries the
       loader; SerializeSnapshot methods route handle refs through Ctx.Snapshot_Handle.
     - Missing types in manifest (new build) → zero-init.
     - Extra types in manifest (old build) → skip via recorded byte length.

  5. PUBLISH SAVEKEY RESOLVER
     - Sweep view<FFragment_SaveKey>, populate TMap<FGuid, FCk_Handle>.

  5.5. REBIND ENTITYSCRIPT _AssociatedEntity
     - Sweep view<FFragment_EntityScript_Current>. For each restored script instance,
       set _AssociatedEntity (UPROPERTY(Transient), skipped by Serialize) to the
       freshly-rebound FCk_Handle. Must complete BEFORE level reload so any AS code
       running in BeginPlay can call DoGet_ScriptEntity() safely.

  6. RELOAD LEVEL
     - OpenLevel(header.WorldName) or soft transition.
     - EntityBridge::BeginPlay resolution per Section 2.

  7. ORPHAN SWEEP + REPORT
     - After post-load gate fires (UCk_Snapshot_Settings._OrphanSweepGate — either a
       fixed duration, default 2 seconds, OR a named signal handle the project wires
       up, e.g. an "all-players-ready" gameplay signal for 4-player co-op rejoin),
       destroy unconsumed resolver entries.
     - Broadcast OnLoadComplete with FCk_Snapshot_LoadReport.
```

### LoadReport struct

```cpp
USTRUCT(BlueprintType)
struct CKSNAPSHOT_API FCk_Snapshot_LoadReport
{
    ECk_SnapshotResult Result;          // Success / Failed_IncompatibleSave / Failed_IO / Failed_Corrupt
    int32 EntitiesRestored;
    int32 EntitiesOrphaned;
    int32 EntitiesPartiallyRestored;
    TArray<FString> SkippedFragmentTypes;
    TArray<FString> SkippedDynamicTypes;
    TArray<FString> SkippedScriptClasses;
    FCk_Snapshot_Header LoadedHeader;
};
```

**Single source of truth for "what didn't make it."** Never a silent skip. Every drop attributable to a UScriptStruct or UClass path; surfaces in the delegate.

### Public API (entire BP / AS surface)

```cpp
UCLASS()
class CKSNAPSHOT_API UCk_Snapshot_Subsystem : public UGameInstanceSubsystem
{
public:
    UFUNCTION(...) void Request_Save(FName InSlotName, const FCk_Delegate_OnSaveComplete& InDelegate);
    UFUNCTION(...) void Request_Load(FName InSlotName, const FCk_Delegate_OnLoadComplete& InDelegate);
    UFUNCTION(...) bool Get_HasSaveSlot(FName InSlotName) const;
    UFUNCTION(...) FCk_Snapshot_Header Get_SaveSlotHeader(FName InSlotName) const;
    UFUNCTION(...) bool TryResolve_SaveKey(FGuid InKey, FCk_Handle& OutHandle) const;

    // Signals: OnPreSave, OnSaveComplete, OnPreLoad, OnLoadComplete.
};
```

Five UFUNCTIONs, four signals. That's the entire user-visible API.

### Replication interplay

The replication path **is** the client-side restore path. Save captures server's truth; on load, server restores its ECS; clients receive standard initial-replication (the same code that runs on first join). No separate client-restore code path.

Two caveats explicitly scoped in:

- **Pending client RPCs are dropped.** Server flushes its own queue; it doesn't know about RPCs in flight from clients. Save UX must brief-block client input (standard "saving..." dialog already does).
- **Convergent flush is a real precondition.** Feedback-looping processors (one whose handler enqueues a new request that re-fires the same processor) will hit the iteration cap. We surface this as a framework health invariant — failing loud is the right answer.

---

## Section 5 — Migration & versioning

Three drift axes, three rules:

| Drift type | Mechanism | Default |
|---|---|---|
| **Format version** (snapshot layout itself) | `_Header.FormatVersion : uint16` | Hard incompat. Bumped manually only when archive contract changes. Old saves refused with `Failed_IncompatibleSave`. |
| **Schema additions** (new UPROPERTY field, new fragment type, new EntityScript class) | Tagged-property serialization via `ArIsSaveGame=true` | Tolerated silently. New UPROPERTYs default-init on load; new fragment types simply absent on old entities. |
| **Schema removals** (removed UPROPERTY, removed fragment type, deleted AS class) | Per-fragment byte length in manifest; `SkippedTypes` recorded in LoadReport | Tolerated but **surfaced**. Removed fields dropped; removed fragment types skipped via byte-length jump; affected entity → `EntitiesPartiallyRestored++`. |

The middle row is the headline: **adding a new `UPROPERTY(meta=(SaveGame))` to an existing snapshotable fragment is a non-event for old saves.** UE's tagged-property serialization keys each field by name + type.

### Renames — `CoreRedirects`

```ini
[CoreRedirects]
+PropertyRedirects=(OldName="FFragment_Inventory.OldFieldName", NewName="FFragment_Inventory.NewFieldName")
+StructRedirects=(OldName="FFragment_Inventory_Old", NewName="FFragment_Inventory_New")
+ClassRedirects=(OldName="UCk_Inventory_OldScript", NewName="UCk_Inventory_NewScript")
```

Proxy archive consults these during `SerializeItem`. We don't write any redirect machinery — UE's mechanism.

### When to bump `FormatVersion`

Only when the **archive contract** changes (manifest layout, header layout, archive flags, etc.). Adding a new fragment type / field / EntityScript class — **none of these bump the version.**

On version bump: ship a v(n-1) → v(n) migration loader (chained for multi-version saves).

### Plugin build hash

`_Header.PluginBuildHash : FGuid` is a **diagnostic hint** in every LoadReport (not a strict gate). Optional `RefuseLoadsBelowBuildHash` setting for shipping builds.

### `FCk_Snapshot_Header` committed shape

```cpp
USTRUCT(BlueprintType)
struct CKSNAPSHOT_API FCk_Snapshot_Header
{
    GENERATED_BODY()

    // Bumped only when archive contract changes (see "When to bump FormatVersion" above).
    UPROPERTY()
    uint16 _FormatVersion = 1;

    // UE engine version captured via FEngineVersion::Current(). Diagnostic-only.
    UPROPERTY()
    FEngineVersion _EngineVersion;

    // Plugin build hash, generated at plugin build time via a CK_SNAPSHOT_BUILD_HASH define.
    // Diagnostic by default; gateable via UCk_Snapshot_Settings._RefuseLoadsBelowBuildHash.
    UPROPERTY()
    FGuid _PluginBuildHash;

    // Wall-clock UTC at save time. For load UI / slot-listing.
    UPROPERTY()
    FDateTime _TimestampUTC;

    // World asset path (e.g. "/Game/Maps/StoreMain") — used by load step 6 OpenLevel.
    UPROPERTY()
    FSoftObjectPath _WorldAssetPath;

    // Per-fragment-type manifest. For each savable fragment type registered at save
    // time, we record its UScriptStruct path-name (for SkippedFragmentTypes attribution),
    // its EnTT type ID (for snapshot_loader dispatch), its entity count at save time
    // (sanity check), and its on-disk byte length (so removed types can be skipped on
    // load by byte-jumping rather than failing).
    UPROPERTY()
    TArray<FCk_Snapshot_Header_FragmentManifestEntry> _Manifest;

    // Entity count at save time (for LoadReport diff + capacity reservation on load).
    UPROPERTY()
    int32 _EntityCount = 0;
};

USTRUCT()
struct CKSNAPSHOT_API FCk_Snapshot_Header_FragmentManifestEntry
{
    GENERATED_BODY()

    UPROPERTY() FString  _ScriptStructPath;   // e.g. "FCk_Fragment_FloatAttribute_ParamsData"
    UPROPERTY() uint32   _EnttTypeId = 0;
    UPROPERTY() int32    _EntityCount = 0;
    UPROPERTY() int64    _ByteLength = 0;     // bytes in _SnapshotBytes occupied by this type
    UPROPERTY() int64    _ByteOffset = 0;     // start offset into _SnapshotBytes
};
```

The header is always read first. If `_FormatVersion` is unsupported, `_Manifest` and `_SnapshotBytes` are both ignored and the load fails with `Failed_IncompatibleSave`. If the format matches, the manifest drives the per-fragment-type dispatch and is the single source of truth for byte-jumping past types the current build no longer has.

### V1 migration commitments

1. Adding `UPROPERTY(meta=(SaveGame))` to an existing snapshotable fragment is safe across saves.
2. Adding a new snapshotable fragment is safe.
3. Adding a new feature module with new savable fragments is safe.
4. Adding a new `UCk_EntityScript_UE` subclass is safe.
5. Removing any of the above is tolerated with full drop attribution in LoadReport.
6. Renaming any of the above requires a `CoreRedirects` entry — enforced via PR template / CLAUDE.md, not code.

---

## Section 6 — V1 scope, deferrals, testing

### V1 in scope

**Framework (full CkSnapshot module):**

- Module directory layout from Architecture summary.
- `FragmentIsSnapshotable` concept (added to `CkEcs/Concepts/` — tier-1 touch — flagged explicitly in implementation plan so downstream consumers see the new header).
- `FSnapshotContext` carrying entt::continuous_loader / entt::snapshot for entity-handle remap.
- Three-tier dispatch (`DoSerializeSnapshot<T>` overloads for SerializeSnapshot-method vs UStruct-auto cases).
- SerializeSnapshot methods authored on the non-USTRUCT templated families V1 touches (`TFragment_Attribute<>`, `TFragment_Attribute_PreviousValues<>`, `TFragment_AttributeModifier<>`, `TFragment_RefillAccumulator`, `TFragment_RecordOfEntities<T>`, `FFragment_InventoryItem`, `FFragment_EntityScript_Current` — the last including the class-path + UPROPERTY round-trip flow from Section 1 path #3).
- `FFragment_SaveKey` + bridge UPROPERTY wiring.
- `FSnapshotArchive_Writer` / `_Reader`.
- `UCk_Snapshot_Subsystem` (5-UFUNCTION + 4-signal API).
- `UCk_Snapshot_SaveGame` USaveGame subclass with the `FCk_Snapshot_Header` committed in Section 5.
- `UCk_Snapshot_Settings : UDeveloperSettings` with `_OrphanSweepGate`, `_RefuseLoadOnUnresolvedTypes`, `_RefuseLoadsBelowBuildHash`.
- Convergent flush loop (CVar-controlled cap, per-iteration dirty diagnostics).
- `FCk_Snapshot_LoadReport` + signal payload.
- EntityScript `_AssociatedEntity` rebind step (load-flow step 5.5).
- `CoreRedirects`-based rename support (no code; documented).
- Format version 1; build-hash diagnostic.

**Feature coverage:**

- **`CkFloatAttribute`** — Params, Min / Max / Current sub-fragments, modifier sub-entities (revocable + non-revocable), refill state. State-bearing fragments tagged `IsSnapshotable`; transient (`*_Requests`, replication snapshots, signal subscribers) explicitly untagged.
- **`CkInventory`** — Params (Spatial + DataOnly variants), `RecordOfInventoryItems`. Per-item entity: Definition reference, traits (Stackable's IntegerAttribute, Dimensions, Tags). Spatial-specific: `FIntPoint` + `ECk_CardinalRotation`. DataOnly-specific: bound mode + limit. Container fragments on outer entity persist.
- **`CkIntegerAttribute`** ride-along: stack-count integer attributes inside items are sub-entities and pick up the same family treatment. Near-zero cost.
- **`FFragment_DynamicFragment_Data`** marked snapshotable from day one. ⚠️ AS-touching — needs CTO sign-off.

**AngelScript surface:** No new AS API. AS authors use the same `meta=(SaveGame)` discipline. `Plugins/CkFoundation/Script/CLAUDE.md` gets a "Persistence" section documenting all four surfaces from Section 1 (C++ via utils, dynamic fragments, EntityScript UPROPERTYs with drop-spawn-params contract, signal-subscriptions-don't-survive contract).

### V1 explicitly deferred

- Per-fragment custom `Migrate(int32 FromVersion, FArchive&)` callbacks.
- Runtime-spawned-but-savable actors.
- Cross-feature entity references via raw handle (V1 features use `Record` traversal only).
- Client-side snapshot generation.
- `_SnapshotBytes` compression.
- Per-feature rollout beyond CkFloatAttribute + CkInventory.

### Testing strategy

**AutoTests (CkTests, headless):**

1. `Test_Snapshot_FloatAttribute_RoundTrip` — Min/Max/Current + revocable + non-revocable modifier round-trip. Exercises the Tier-C `SerializeSnapshot` on `TFragment_Attribute<>`.
2. `Test_Snapshot_FloatAttribute_RefillState_RoundTrip` — refill rate + paused state.
3. `Test_Snapshot_Inventory_Spatial_RoundTrip` — N items with varied placements + rotations + per-item stack counts. Exercises Tier-B `SerializeSnapshot` on `FCk_InventoryItem_Spatial_ReplicatedEntry` (entity-handle remap path) and Tier-C on `TFragment_RecordOfEntities<>`.
4. `Test_Snapshot_Inventory_DataOnly_Bounded_RoundTrip` — bound limit + slot occupancy + stack counts.
5. `Test_Snapshot_Inventory_StackCount_AttributePath` — inventory → item → stack-count integer attribute → modifier chain (deepest sub-graph). Asserts that an item handle restored in the inventory's record points to the correctly-remapped item entity (validates the `continuous_loader` integration end-to-end).
6. `Test_Snapshot_LoadReport_PartialRestore` — save with fragment type test build registers under different name; load; assert LoadReport surfaces skip.
7. `Test_Snapshot_OrphanedSaveKey` — two SaveKey'd entities; on load, simulate one actor missing; assert orphan sweep destroys saved entity + logs.
8. `Test_Snapshot_ConvergentFlush_Cap` — author deliberately feedback-looping processor (test-only); assert `Failed_NotQuiescent` after the configured cap + per-iteration dirty-count history is logged + the named dirty fragment is identified.
9. **`Test_Snapshot_NormalWorkload_DoesNotTripFlush`** (sign-off #4) — exercise a representative V1 workload (add several items to an inventory, mutate a modifier, save in-flight). Assert that the convergent flush completes within 3 iterations under normal load. Guards against a future processor accidentally regressing into pathological behavior under saves during gameplay.
10. **`Test_Snapshot_AS_UPROPERTY_SaveGame_Smoke`** (sign-off #3) — define one AS-authored `UCk_EntityScript_UE` subclass with a single `UPROPERTY(SaveGame) int _Value = 0;` field. Spawn an entity with the script, mutate `_Value` to a non-default, save, destroy world, load, assert `_Value` round-trips. Validates that the AS frontend translates `(SaveGame)` to `CPF_SaveGame` on the compiled UClass. **If this test fails, V1 cannot ship as designed — we'd either patch the AS frontend or pivot path #3.**

**Gym (interactive):**

- `CkSnapshotGym` — single station. Inventory + player character with float attributes (health, stamina). Hotkeys: 1=add items / spend stamina / take damage; F5=save; F9=load. HUD shows attribute values + inventory contents. After save → destructive actions → load, HUD snaps back. Gameplay-realistic + headless-testable via autotest harness.

**Cross-cutting guard:**

- One AutoTest loading the previous CkSnapshot build's save (`legacy_save.sav` checked in once V1 lands; updated whenever format intentionally changes). Catches accidental format breakage in PRs.

### Acceptance criteria

- All 10 autotests pass — including the two new gates (`Test_Snapshot_NormalWorkload_DoesNotTripFlush`, `Test_Snapshot_AS_UPROPERTY_SaveGame_Smoke`).
- Gym round-trip visually correct + asserted via HUD in auto-mode.
- **CTO re-review signs off** on Section 1 (revised four-surface AS analysis), Section 3 (three-tier serialize mechanism + entity-handle remap), and Section 5 (committed header shape).
- `Plugins/CkFoundation/Source/CkSnapshot/CLAUDE.md` documents the three-tier contract, deferrals, rename discipline, LoadReport-driven debugging.
- `Plugins/CkFoundation/Script/CLAUDE.md` Persistence section landed — documents the four AS surfaces, the drop-spawn-params contract, and the signal-subscriptions-don't-survive contract.

---

## Open questions

### Resolved from v1 review

1. ~~AngelScript "three surfaces, all free"?~~ → Revised in Section 1: now **four** surfaces explicitly enumerated; path #3 spelled out with class-path + drop-spawn-params + `_AssociatedEntity` rebind; path #4 (signal subscriptions) added.
2. ~~`FFragment_DynamicFragment_Data` blanket-snapshotable?~~ → **Yes** at carrier level; payload USTRUCT gates fields via `meta=(SaveGame)`. Resolved in Section 1 path #2.
3. ~~AS class rename — log+skip or refuse?~~ → **Log + skip** default; opt-in strict mode via `UCk_Snapshot_Settings._RefuseLoadOnUnresolvedTypes`. Resolved in Section 1.
4. ~~Convergent-flush cap of 8 iterations?~~ → **CVar-controlled** (`ck.Snapshot.ConvergentFlush.MaxIterations`, default 8) with per-iteration dirty-count history on failure. Plus new test #9 ensures normal workload doesn't trip it. Resolved in Section 4 + Section 6.
5. ~~Orphan sweep grace window?~~ → **Default 2-second post-load timer**; project-overridable via `UCk_Snapshot_Settings._OrphanSweepGate`, which accepts either a duration or a named signal handle (e.g. "all-players-ready" for 4-player co-op rejoin). Resolved in Section 4.
6. ~~V1 deferring cross-feature handle references — will it bite V1?~~ → **No.** Inventory's intra-feature handle refs (`FFragment_Item_ParentInventory`, `FFragment_InventorySlot_ItemRef`, EntityHolder chain) are within `CkInventory`; CkAttribute's modifier-subentity refs are within `CkAttribute`. The deferral targets *cross-feature* refs (e.g. a future CkAbility referencing an `FCk_Handle_FloatAttribute` on another feature's tree). The first feature that crosses that line gets a follow-up task. Resolved.

### New for v2 review (none blocking)

The v2 spec resolves all six v1 open questions and addresses all six CTO sign-off conditions. No new open questions are raised. The CTO re-review should focus on whether the **Section 3 three-tier mechanism** and **Section 1 path #3 EntityScript flow** address blockers 1–2 satisfactorily, and whether the **`Test_Snapshot_AS_UPROPERTY_SaveGame_Smoke`** test is sufficient as the AS-frontend verification gate.
