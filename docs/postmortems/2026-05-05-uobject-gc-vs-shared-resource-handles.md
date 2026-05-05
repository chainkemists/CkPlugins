# Postmortem: UObject GC ordering vs SharedPtr-owned external state

## Date

2026-05-05 (bug discovered) → 2026-05-05 (migration completed)

## Summary

A `TSharedPtr<entt::basic_registry>` was held both by the world subsystem (intended owner) and by `FCk_Handle` value copies stored as UPROPERTY fields on UObjects across the codebase. During PIE-stop the engine's GC purge destroyed those UObjects in an order that wasn't guaranteed to follow ownership intent — the registry's last legitimate SharedPtr release ran before the dangling handles' destructors did, freeing the TSharedPtr's control block and leaving subsequent dtors decrementing freed memory. Eventually the heap allocator detected corruption and fast-fail-terminated the process. UE's normal crash handler was bypassed (no dump, no reporter UI) because `__fastfail` skips SEH.

## Symptoms

- Editor would silently vanish after several PIE start/stop cycles.
- No crash dump in `Saved/Crashes/`, no Windows Error Reporting popup.
- Logs ended mid-frame with no `Fatal` markers.
- Reproduction was timing-sensitive — depended on which UObject-holders happened to be GC'd before the world subsystem.

## Root cause

**UObject GC ordering does not respect SharedPtr ownership semantics.** Multiple UObjects holding `TSharedPtr` copies of the same control block will be destroyed in arbitrary order during a GC sweep. If the "primary owner" UObject is destroyed before any of the secondary holders, the secondary holders become dangling SharedPtrs whose destructors will scribble on freed memory.

The specific instance here: `FCk_Handle`'s `_Registry` member was a `ck::TPtrWrapper<TSharedPtr<entt::basic_registry>>`. Every handle copy bumped the SharedPtr refcount; every destroy decremented it. The world subsystem (`UCk_EcsWorld_Subsystem_UE`) was the canonical owner, but the refcount counted **every UObject that copied a handle** as a co-owner. When PIE-stop GC ordered subsystem destruction before some of those secondary UObjects (notably `UCk_Processor_Script_Base_UE._Handle`), the registry's control block was already freed by the time those UObjects' `FCk_Handle` destructors ran.

## Why we didn't catch it sooner

- TSharedPtr's safety contract (refcount accounting on copy/destroy) is correct in isolation — there's no static check that catches "two UObjects sharing a control block in arbitrary GC order is unsafe."
- The bug only manifested across a PIE start/stop boundary, in a reproducer that required ~3 minutes of PIE activity. Single-session testing didn't surface it.
- AutoTests didn't cover the "UObject lifecycle inversion" class of scenario.
- The bug surfaced as silent process termination because `__fastfail` (the heap-corruption tripwire) bypasses UE's SEH-based crash reporter.

## Fix

Replaced `FCk_Handle`'s SharedPtr storage with a generational handle (slot index + generation). The world subsystem owns the registry (`TUniquePtr`) and a slot in a process-static slot table. UObject holders carry only POD slot+gen pairs — no smart-pointer ownership. On stale handle access, the slot table's generation mismatch returns a clean nullptr instead of dereferencing freed memory.

Key migration milestones:
- **Phase 1** — `ck::registry_table` slot allocator with phoenix-singleton storage that outlives the entire UObject lifecycle, sentinel-flipped from `FCkEcsModule::ShutdownModule`. The phoenix pattern means UObject destructors firing during DLL teardown find a sentinel-dead table and silent-no-op rather than dereferencing freed static state.
- **Phase 2** — `FCk_Registry` becomes a non-owning view (POD `(slot, gen, transient_entity)`). All registry access routes through `slot_table::Resolve` which fires a strict ensure on stale handles in non-shipping.
- **Phase 3** — `FCk_Handle` storage swapped from `TOptional<FCk_Registry>` to `FCk_RegistryHandle`. Type becomes trivially copyable with no special members; the handle's destructor is now a no-op memcpy of bytes.
- **Phase 4** — Subsystems own `TUniquePtr<entt::basic_registry>` and call `Allocate`/`Free` in `Initialize`/`Deinitialize`. `ck::FEcsWorld` is the RAII helper for editor-only subsystems and per-fragment private worlds.

## The rule, going forward

> **If a subsystem owns a non-UObject resource (registry, handle pool, external library state) and exposes references to that resource to UObjects, those references must NOT be smart pointers that share ownership. Use a generational-handle pattern with the subsystem retaining sole ownership.**

Applies to any future subsystem that:
- Owns a `TSharedPtr` / `TUniquePtr` to non-UObject state.
- Hands out references that may end up stored on UObject UPROPERTY fields.
- Has a teardown ordering that interleaves with UE's GC purge.

The generational handle pattern is documented in [`CkRegistry_SlotTable.cpp`](../../Plugins/CkFoundation/Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp) as the reference implementation for this codebase.

## Diagnostics that found it (for the next investigation)

- Per-instance lifecycle logging on copy/move/destroy + the registry's refcount at each event. Made the dangling-decrement pattern visible: refcount marched 0 → -1 → -2 → ... → -12.
- Static-flag check on `STRUCT_CopyNative` ruled out copy-semantics bugs early; let us focus on lifetime.
- Stack trace at first negative refcount pointed straight at the UObject holder via `FObjectPurge::DestroyObjects` → `<UObjectClass>::~vector_deleting_destructor`.

## Detection going forward

The migration leaves `ck::registry_table::Resolve` as a strict-by-default API that fires `CK_ENSURE_IF_NOT` in non-shipping when a stale handle is used. New instances of this bug class would surface as a stale-handle ensure rather than silent corruption.

A C++ unit test (`Ck.Registry.LifetimeInversion.HandleSurvivesRegistry`) explicitly reproduces the lifetime-inversion pattern using a synthetic UObject holder. The test was deliberately written to fail on the pre-migration code; post-migration it passes by demonstrating that a slot-freed registry is harmless to outstanding handle holders.

## Side-discoveries during migration

The bug above was the load-bearing motivation, but the migration uncovered a related class of latent issues that share the same root cause — **default-constructed `FCk_Registry` was implicitly safe** pre-migration because its `TPtrWrapper<TSharedPtr<entt>>` auto-allocated. Post-migration, default-construct produces a slot-Unset view that crashes on first use. Affected sites: `UCk_GameSession_Subsystem_UE::_InternalRegistry`, `ck::FFragment_2dGridSystem_Current::_CellRegistry`. Both fixed by switching to explicit `TUniquePtr<ck::FEcsWorld>` ownership.

Going-forward rule for this latent class: **any class that holds an `FCk_Registry` member should either (a) be a subsystem that allocates/frees its slot in `Initialize`/`Deinitialize`, or (b) own a `TUniquePtr<ck::FEcsWorld>` instead.** Default-construct of `FCk_Registry` is now an unbound view by design.
