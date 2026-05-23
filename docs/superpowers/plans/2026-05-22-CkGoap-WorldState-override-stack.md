# CkGoap WorldState Override Stack

**Status:** PLANNING. Implementation starts after this commit.
**Companion docs:** Spec §5 (WorldState resolution). This feature is additive — no spec rewrite needed; add a §5.4 entry recognising the stack as first-class.

---

## Why

The current GOAP `FFragment_Goap_WorldState_Values` is a single flat key→bool store per WS entity. To experiment with "what if `EnemyVisible=true`?" the user has to mutate the live WS and hope gameplay doesn't immediately overwrite it. No undo, no clear "what's hypothetical vs real" distinction.

A push/pop stack of WS layers gives:

- **Debugger:** click WS keys in the rail to push a named `"DebugUI"` layer; one-click Reset pops it.
- **AI deliberation (future):** push a hypothesis, ask the planner, pop. Foundation for second-order reasoning.
- **Tests:** `FScope_GoapWS_Override` RAII helper replaces imperative Set→test→reset.
- **Net replay / save-load (future):** push a snapshot layer for one frame to drive replay.

Library-level. **Not** debug-only.

---

## Decisions locked in

1. **Library-level.** API in `UCk_Utils_Goap_WorldState_UE`, not gated by `WITH_EDITOR`.
2. **Named layers.** `Push_Override(WS, FName Name, Map)` / `Pop_Override_ByName(WS, Name)`. Idempotent for debug UI; named for AI scopes coexisting.
3. **Debug layers reset on PIE start** (no persistence).
4. **Debugger UX:** auto-create `"DebugUI"` layer on first toggle from the WS rail. Show override-active badge prominently.
5. **Two AutoTests** in the regression net.

---

## API surface

In `UCk_Utils_Goap_WorldState_UE` (`Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/WorldState/CkGoap_WorldState_Utils.{h,cpp}`):

```cpp
// Push a named override layer. If a layer with this name already exists,
// REPLACE its contents (idempotent — debug UI re-clicks are stable).
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|WorldState",
          DisplayName = "[Ck][Goap|WorldState] Push Override")
static FCk_Handle_Goap_WorldState Push_Override(
    UPARAM(ref) FCk_Handle_Goap_WorldState& InWS,
    FName InLayerName,
    const TMap<FGameplayTag, bool>& InOverrideValues);

// Pop a named layer. No-op if no layer with that name.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|WorldState",
          DisplayName = "[Ck][Goap|WorldState] Pop Override")
static FCk_Handle_Goap_WorldState Pop_Override_ByName(
    UPARAM(ref) FCk_Handle_Goap_WorldState& InWS,
    FName InLayerName);

// Clear all override layers. Restores base-only reads.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap|WorldState",
          DisplayName = "[Ck][Goap|WorldState] Clear Overrides")
static FCk_Handle_Goap_WorldState Clear_Overrides(
    UPARAM(ref) FCk_Handle_Goap_WorldState& InWS);

// Number of layers currently pushed.
static int32 Get_OverrideDepth(const FCk_Handle_Goap_WorldState& InWS);

// Names of currently-pushed layers, bottom-to-top. Debugger UI uses this.
static TArray<FName> Get_OverrideLayerNames(const FCk_Handle_Goap_WorldState& InWS);

// Does any override layer currently shadow this key? Debugger UI uses this
// to mark overridden keys visually.
static bool Has_KeyOverride(const FCk_Handle_Goap_WorldState& InWS, FGameplayTag InKey);
```

**Singular `Set_Override` convenience** (avoids constructing a `TMap` for the common single-key flip):

```cpp
// Push or update a single key into the named layer. Creates the layer if missing.
// Debugger uses this on every per-key toggle.
static FCk_Handle_Goap_WorldState Push_Override_SingleKey(
    UPARAM(ref) FCk_Handle_Goap_WorldState& InWS,
    FName InLayerName,
    FGameplayTag InKey,
    bool InValue);
```

---

## New fragment

In `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/WorldState/CkGoap_WorldState_Fragment.h`:

```cpp
struct CKGOAP_API FFragment_Goap_WorldState_OverrideStack
{
public:
    CK_GENERATED_BODY(FFragment_Goap_WorldState_OverrideStack);
    friend class ::UCk_Utils_Goap_WorldState_UE;
    friend class FProcessor_Goap_Planner_HandleRequests;   // for snapshot flattening

private:
    struct FLayer
    {
        FName Name;
        TMap<FCk_GoapKey, bool> Values;   // resolved keys only
    };
    TArray<FLayer> _Layers;   // bottom-to-top; _Layers.Last() is the top
};
```

**Stamped on every WS entity at creation.** Empty by default. Adds <100 bytes of overhead when no overrides are active.

---

## Read path

`Get_Value(WS, Key)` rewrites to:

```cpp
const auto& Stack = WS.Get<FFragment_Goap_WorldState_OverrideStack>();
for (auto i = Stack._Layers.Num() - 1; i >= 0; --i)
{
    if (const auto* V = Stack._Layers[i].Values.Find(Key))
    { return *V; }
}
return WS.Get<FFragment_Goap_WorldState_Values>()._Values.FindRef(Key);
```

Order: top layer first, falling through to base.

---

## Write path (mutation discipline)

`Set_Value(WS, Key, NewValue)` continues to mutate ONLY the base (`FFragment_Goap_WorldState_Values._Values`). Override layers are read-overlay only.

This is the non-negotiable rule. Document loudly in the CLAUDE.md anti-patterns.

---

## A\* hot-path snapshotting

`FProcessor_Goap_Planner_HandleRequests` already builds an A* seed snapshot. Augment that snapshot to flatten the stack:

```cpp
// At seed time, before kicking A*:
auto FlatWS = TMap<FCk_GoapKey, bool>{};
// Walk base first, then layers bottom-to-top, overwriting.
FlatWS = WS.Get<FFragment_Goap_WorldState_Values>()._Values;
for (const auto& Layer : Stack._Layers)
    for (const auto& Kv : Layer.Values)
        FlatWS.Add(Kv.Key, Kv.Value);
// FlatWS is now the effective view A* will read.
```

A* reads the flat snapshot exclusively. Stack walks stay out of the inner loop. Should be a no-op cost change for plans.

---

## Dirty signal

Every push/pop/clear that changes the EFFECTIVE view fires `FTag_Goap_Planner_Dirty_WorldState` on every Planner subscribed to this WS.

Concretely: before the mutation, snapshot the effective view for every key in the affected layer. After the mutation, diff. For each key whose effective value changed, fire dirty.

Simple shape:

```cpp
auto Push_Override(WS, Name, NewLayer) -> void
{
    auto Before = SnapshotEffective(WS, KeysIn(NewLayer));
    // ... mutate the stack ...
    auto After = SnapshotEffective(WS, KeysIn(NewLayer));
    for (auto& Kv : NewLayer) if (Before[Kv.Key] != After[Kv.Key]) FireDirty();
}
```

Pop: same shape, snapshot the layer being popped.

---

## Multi-Planner reuse semantics

If two Planners share a WS, both replan when the override pushes. Documented as feature, not a bug. ("Push affects all subscribers.")

---

## Tests

In `Plugins/CkTests/Script/CkGoap/`:

### `CkAutoTest_Goap_Planner_WSOverrideStack_BasicPushPop.as`

1. Build a top-level Planner with goal `{Goal=true}` and 2 candidate operators:
   - `OpA` precondition `{KeyA=true}`, effect `{Goal=true}`, cost 1.
   - `OpB` precondition `{KeyB=true}`, effect `{Goal=true}`, cost 1.
2. Initial WS: `{KeyA=true, KeyB=false}`. Plan resolves to `[OpA]`.
3. `Push_Override("test", {KeyA=false, KeyB=true})`. Wait for replan.
4. Assert plan now resolves to `[OpB]` (KeyA shadowed false → OpA blocked; KeyB shadowed true → OpB viable).
5. `Pop_Override_ByName("test")`. Wait for replan.
6. Assert plan reverts to `[OpA]`.
7. Finish success.

### `CkAutoTest_Goap_Planner_WSOverrideStack_DirtyFiresOnPushPop.as`

1. Build any Planner. Bind to `OnPlanComplete` and count fires.
2. Initial WS, initial plan completes → count = 1.
3. `Push_Override("test", {SomeKey=NewValue})` where NewValue differs from base.
4. Assert OnPlanComplete fires again (count = 2). Replan was triggered by the push.
5. `Push_Override("test", {SomeKey=NewValue})` AGAIN with same values (idempotent re-push).
6. Assert count stays 2 (no spurious replan when effective view didn't change).
7. `Pop_Override_ByName("test")`.
8. Assert OnPlanComplete fires (count = 3). Effective view changed on pop.
9. Finish success.

---

## Debugger UI changes

In `Plugins/CkGameplayDebugger/Source/CkGoapDebugger/Public/CkGoapDebugger/Window/SCkGoapDebugger_WorldStateRail.{h,cpp}`:

1. **Per-key click handler.** Today's WS rail rows are read-only. Add an `OnClicked` to each row that calls `utils_goap_world_state::Push_Override_SingleKey(WS, "DebugUI", Key, !CurrentEffectiveValue)`. Single click flips the key in the DebugUI layer.
2. **Override marker on row.** When `Has_KeyOverride(WS, Key)` returns true, render the row with a distinct color (orange/amber) and an `OVERRIDE` pill.
3. **Reset button** at the top of the rail. Visible (and bright) when `Get_OverrideDepth(WS) > 0`. Tooltip lists currently-pushed layer names. Click → calls `Pop_Override_ByName(WS, "DebugUI")` specifically (preserves any AI-deliberation layers that might exist).
4. **Header badge.** `WS · 3 keys [+1]` when DebugUI is active.

---

## Acceptance criteria

- New fragment + utils + read-path rewrite + dirty signal land. 22/22 existing `Goap_Planner_*` AutoTests still pass.
- 2 new AutoTests pass in both Development and DebugGame configs. Total: 24/24.
- Debugger WS rail rows clickable. Reset button shows when overrides active. Override-marked keys visually distinct.
- `CkGoap/CLAUDE.md` gets a new "World State override stack" section under §5 (WorldState resolution). Anti-pattern entry: "Don't write to override layers; writes always go to the base store."
- `CkGoapDebugger/CLAUDE.md` documents the WS rail's debug-toggle behaviour.

---

## Risks + mitigations

| Risk | Mitigation |
|---|---|
| Read-path overhead in A* hot loop | Flatten stack to snapshot at A* seed; A* reads flat snapshot only |
| Dirty signal misfires (replan storms or stale plans) | Diff-then-fire pattern in push/pop; idempotent re-push is a no-op |
| Editor-only consumers leaking into shipping | Pure runtime code; no `WITH_EDITOR` gates needed |
| Future debug layer persistence ask | Defer; debug layers reset on PIE start |

---

## Phasing

**Phase 1 — Framework** (single dispatch, ~3-4 hours):
- New fragment, utils functions, read-path rewrite, dirty signal, A* snapshot integration, 2 new AutoTests.
- Verify 24/24 in both configs.

**Phase 2 — Debugger UI** (single dispatch, ~2 hours):
- WS rail clickable rows, Reset button, override markers, header badge.

**Phase 3 — Docs** (small dispatch, ~30 min):
- CkGoap CLAUDE.md §5 update, anti-pattern entry, debugger CLAUDE.md update.

Each phase verifies independently. Phase 2 cannot start until Phase 1 lands the API; Phase 3 follows.

---

## Spec impact

`docs/superpowers/specs/2026-05-21-CkGoap-PlannerActionCollapse-design.md` §5 (WorldState resolution & dirty/replan) gets a new §5.4:

> ### 5.4 Override stack
>
> A WS entity carries a stack of named override layers (`FFragment_Goap_WorldState_OverrideStack`). Reads walk the stack top-down, falling through to the base store. Writes via `Set_Value` always mutate the base. Push/pop fire dirty signals for the keys whose effective value changed. A* snapshots the flattened view at seed time so the inner loop stays single-indirection.
>
> Layers are named. The same name can be pushed multiple times — re-push replaces the layer's contents idempotently. The debugger UI uses a fixed layer named `"DebugUI"`; AI deliberation code typically uses anonymous ad-hoc names.
>
> Override layers reset on PIE start. Persisting them across sessions is out of scope.
