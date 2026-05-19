# Continuation Prompt — CkGoap Bundle/Tier Refactor (Phase 7+)

**One-line summary:** Phases 0-6 complete and build-verified green. The Bundle/Tier model is functionally complete — Add/AddBundle/AddTier/AddAction work, six processors (Tier Setup/AutoReplan/HandleRequests/Execute/HandleResult + Bundle Setup/ChainUpdate) are registered, WS subscriber plumbing is wired through tier activation/deactivation, and bundle dependency-cycle detection runs at setup time. **No tests yet.** Phase 7 (14 new AS tests + retarget 5 baseline) is the remaining validation work.

---

## What's done (all committed on `dev`, none pushed)

| Phase | Commit (CkFoundation) | Description |
|---|---|---|
| 0 | (multi-repo) | Cleanup: 15+16 AS files deleted; CkGoapDebugger + CkInspector_Goap stubbed |
| 1 | `68ae3adf5` | Additive new types: handles, params, fragments, requests, signals, FActionDef.ActionTag |
| 2 | `f511c38c1` | Old planner-per-entity API removed; FCk_Handle_Goap repurposed as root container |
| 3 | `be707461b` | Bundle + Tier utils (full imperative API surface) |
| 4 | `9dd80f08c` | All 6 processors (Tier Setup/AutoReplan/HandleRequests/Execute/HandleResult + Bundle ChainUpdate) |
| 5 | `0ce34094c` | WS subscribers retyped to generic; tier subscribe/unsubscribe wired into AddTier + ChainUpdate |
| 6 | `67c2c9137` | Bundle dependency-cycle detection (FProcessor_Goap_Bundle_Setup) |

All build-verified via `./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --output=Saved/Logs/Build-Editor.log --project=D:\Repos\CkPlugins`.

### What the new model does end-to-end (in theory)

```cpp
// 1. Construct
auto Goap = UCk_Utils_Goap_UE::Add(InOwner, RootParams);
auto Bundle = UCk_Utils_Goap_Bundle_UE::AddBundle(Goap, BundleParams{Tag_MyBundle});

// 2. Root tier (first tier on a bundle = root)
auto RootParams = FCk_Fragment_Goap_TierParamsData{Tag_Strategic};
RootParams.Set_WorldStateSource_Override(MyWS);
RootParams.Set_InitialGoal_RootOnly({FCk_GoapWS_Condition_Authored{Tag_GoalKey, true}});
auto RootTier = UCk_Utils_Goap_Tier_UE::AddTier(Bundle, RootParams);

// 3. Sub-tier (catalog only; activated on tag-match)
auto SubParams = FCk_Fragment_Goap_TierParamsData{Tag_OperateShop};
// No override → inherits parent's resolved WS at activation
auto SubTier = UCk_Utils_Goap_Tier_UE::AddTier(Bundle, SubParams);

// 4. Actions
UCk_Utils_Goap_Tier_UE::AddAction(RootTier, UCk_GoapAction_OperateShop::StaticClass());
//   The action's CDO must call SetActionTag(Tag_OperateShop) — that's what
//   wires the chain: Plan[0]==OperateShop → SubTier activates.

UCk_Utils_Goap_Tier_UE::AddAction(SubTier, UCk_GoapAction_ServeCustomer::StaticClass());
// ...etc.

// 5. Bind signals
UCk_Utils_Goap_Tier_UE::BindTo_OnPlanComplete(RootTier, MyDelegate);
UCk_Utils_Goap_Bundle_UE::BindTo_OnActiveTiersChanged(Bundle, MyDelegate);

// At runtime: WS changes → dirty tag stamped on subscribed tiers →
//   AutoReplan enqueues Plan → A* runs → HandleResult writes _Plan →
//   ChainUpdate looks at Plan[0]'s ActionTag → if it matches SubTier's
//   TierTag, append SubTier to ActiveTiers + synchronously inject SubTier's
//   _Goal from OperateShop_action.Effects. SubTier subscribes to its WS,
//   plans next frame. Hierarchy emerges.
```

### Things NOT yet implemented

**Phase 7 — Tests.** No AS tests exist for the new API. The CkTests/Script/CkGoap directory is empty. Spec §9 lists 14 tests to write:

1. `CkAutoTest_Goap_BundleTier_RootOnly.as`
2. `CkAutoTest_Goap_BundleTier_ChainGrowth.as`
3. `CkAutoTest_Goap_BundleTier_ChainTruncation.as`
4. `CkAutoTest_Goap_BundleTier_NoMatchingTier.as`
5. `CkAutoTest_Goap_BundleTier_GoalInjection.as`
6. `CkAutoTest_Goap_BundleTier_WSInheritance.as`
7. `CkAutoTest_Goap_BundleTier_WSOverride.as`
8. `CkAutoTest_Goap_BundleTier_InvalidGoal.as`
9. `CkAutoTest_Goap_BundleTier_DirtyPropagation.as`
10. `CkAutoTest_Goap_BundleTier_MultiBundle.as`
11. `CkAutoTest_Goap_BundleTier_BundleToggle.as`
12. `CkAutoTest_Goap_BundleTier_OwnerCascadeDestroy.as`
13. `CkAutoTest_Goap_BundleTier_DeferOneFrame.as`
14. `CkAutoTest_Goap_BundleTier_RootReset.as`

Test surface notes:
- Each test wraps an EntityScript subclass that exercises the API and asserts via `FinishSuccess()` / `FinishFailure()`.
- AS namespaces:
  - `utils_goap` (root): `Add`, `Has`, `Cast`, `Find_Bundle`.
  - `utils_goap_bundle`: `AddBundle`, `Has`, `Find_Tier`, `Get_ActiveTiers`, `Get_EnableToggle`, `Get_DependencyCycles`, `Request_SetEnableToggle`, `Request_ResetActiveTiers`, `BindTo_/UnbindFrom_OnActiveTiersChanged`.
  - `utils_goap_tier`: `AddTier`, `AddAction`, `Has`, `Get_PlanStatus`, `Get_Plan`, `Get_PlanCost`, `Get_WorldStateSource`, `Get_ActiveParentAction`, `Get_InvalidGoal`, all `Request_*` (SetGoalWorldState/Plan/CancelPlan/SetActionCost/SetReplanInterval/SetReplanPolicy/SetSearchBudget/SetCostThreshold), `BindTo_/UnbindFrom_` for OnPlanComplete/OnPlanFailed/OnTierActivated/OnTierDeactivated.
  - `utils_goap_world_state` (unchanged surface; gained `Request_AddSubscriber`/`Request_RemoveSubscriber` taking generic FCk_Handle).
- Each test needs an `_TimeoutSeconds` on the actor wrapper (default 5-10s).
- Toolbox `--test --test-pattern Goap` discovery via the regenerated `CkTests_AutoTestActors.as` (run on first AS post-compile after writing the test files).

**Phase 8 — Docs polish.** `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` still reflects the old planner-per-entity model. Needs rewrite.

**Out-of-scope from the original plan:**
- Mid-life cycle detection (Phase 6 only re-runs on AddBundle/AddTier — AddAction after that gap won't re-trigger). v2.
- Runtime tuning via `Request_SetReplanInterval` / `Request_SetReplanPolicy` (currently warned-stub). v2.

---

## Things to know for the agent

- **Each tier's request append is inlined** in `UCk_Utils_Goap_Tier_UE` because the helper namespace can't access private members of `FFragment_Goap_Tier_Requests`. If you add new tier requests, inline the append likewise.
- **`FCk_Handle_Goap_*` typesafe handles implicitly upcast to `FCk_Handle&`** — pass directly, no `CopyFrom` dance. `FCk_Handle::CopyFrom` doesn't exist on the base type.
- **Empty USTRUCTs trip a static_assert** in the framework ("Tags must derive from ck::TTag"). If a fragment ends up empty, either add a placeholder field or use a tag instead. `FCk_Fragment_Goap_RootParamsData` is currently empty and is NOT used as an ECS fragment — only `FFragment_RecordOfGoapBundles` marks "this entity is a Goap root."
- **Record-of-entities fragments live in private headers** (`Bundle/CkGoap_Bundle_Record_Internal.h`, `Tier/CkGoap_Tier_Record_Internal.h`) included only by `*_Utils.cpp`. Don't promote to public headers — would drag CkRecord transitively.
- **`UCk_GoapAction_EntityScript._ActionTag`** is populated via the builder `SetActionTag(FGameplayTag)` inside `DefineAction()`. The CDO is read at Setup time; the action's `ActionTag` lives on `goap::FActionDef.ActionTag` thereafter. AS-side: `Get_ActionTag()` is bound via the dynamic-handle registry but the **action class** is a UClass, not a dynamic handle — AS subclasses of `UCk_GoapAction_EntityScript` define actions and the test runner registers them via `AddAction(Tier, MyActionClass::StaticClass())`.
- **The new bundle's `RequiresSetup` tag triggers cycle detection** but the setup processor DEFERS if any catalog tier still has `FTag_Goap_Tier_RequiresSetup`. So tier-level Setup completes first, then bundle setup detects cycles. Tests that exercise cycles need a couple of frames to let both pass.
- **`FProcessor_Goap_Tier_Setup` is friended on `UCk_GoapAction_EntityScript`** — it reads `_Preconditions`, `_Effects`, `_Cost` from the CDO directly.
- **Synchronous goal injection in `Bundle_ChainUpdate`** uses `FKeyRegistry::GetTag` to round-trip parent-keyed Effect → raw tag → child registry key. Unrecognized keys land in `_InvalidGoal` for the new tier (diagnostic, not error).
- **The first frame after AddTier**: root tier has `FTag_Goap_Tier_RequiresSetup` + `FTag_Goap_Tier_RequiresInitialPlan` (if PlanOnStart) + subscribed to WS. Frame 1: Setup extracts actions, AutoReplan fires Plan request, HandleRequests builds graph & starts A*, Execute runs A*. Frame 2+: HandleResult writes the plan, ChainUpdate sees Plan[0] and may append a sub-tier (mark sub-tier RequiresSetup + RequiresInitialPlan + subscribe). Frame 3: sub-tier plans for the first time. So **first plan visible at frame 1-2; first sub-tier active at frame 2-3.** Tests that check chain extension need at least 3-4 frames or should bind `OnTierActivated` / `OnActiveTiersChanged`.

---

## How to resume

```
Continue the CkGoap Bundle/Tier refactor. Phases 0-6 are committed and
build-green on `dev`. Spec at
docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md.
Plan at docs/superpowers/plans/2026-05-19-CkGoap-BundleTierRefactor-plan.md.

Start Phase 7 — write the 14 new AS tests listed in spec §9. Begin with
CkAutoTest_Goap_BundleTier_RootOnly.as as the smoke test: a single bundle
with a root tier holding one action whose effect satisfies the goal, asserts
OnPlanComplete fires with the action in _Plan[0]. Once that's green via
toolbox `--test --test-pattern Goap`, expand to the rest.

After each test (or small batch), verify with:
  ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor \
    --test --test-pattern Goap --output=Saved/Logs/BuildTest.log \
    --project=D:\Repos\CkPlugins
```
