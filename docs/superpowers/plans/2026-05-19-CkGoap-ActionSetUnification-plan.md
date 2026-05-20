# CkGoap ActionSet/Action Unification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Unify the just-shipped Bundle/Tier model into a single ActionSet/Action model, deleting the matching layer (`_ActionTag`/`_TierTag` strict-equality match) in favour of a tree-structured catalog where `Plan[0]` IS a child Action handle directly.

**Architecture:** Two typesafe handles (`FCk_Handle_Goap_ActionSet`, `FCk_Handle_Goap_Action`). ActionSet contains a flat catalog of Actions; each Action carries `_ParentAction` + `_ChildActions` for the tree. Composite (has children) vs atomic (no children) is set at registration time. Chain extension reads `Plan[0]` directly — no tag matching.

**Tech Stack:** Unreal Engine 5.5, C++20, EnTT 3.15.0 (via CkEcs), AngelScript runtime (CkFoundation), CkAStar (time-sliced A*), UnrealToolbox CLI for build/test.

**Spec:** [docs/superpowers/specs/2026-05-19-CkGoap-ActionSetUnification-design.md](../specs/2026-05-19-CkGoap-ActionSetUnification-design.md)

**Supersedes (partially):** Bundle/Tier refactor — Phases 0–6 already committed on `dev`. This plan undoes/redoes parts of Phases 1, 3, 4, 6, and rewrites Phase 7's tests.

---

## Context for an engineer with zero context

**Repo layout.** All work happens in `Plugins/CkFoundation/` (a submodule) and `Plugins/CkTests/Script/CkGoap/` (also a submodule). The host project at `D:\Repos\CkPlugins\` ties them together. Workflow per phase:

1. Edit code inside `Plugins/CkFoundation/Source/CkGoap/`.
2. Verify build + tests via UnrealToolbox.
3. Commit *inside the submodule* (`cd Plugins/CkFoundation && git commit`).
4. Bump the CkPlugins root pointer (`cd D:\Repos\CkPlugins && git add Plugins/CkFoundation && git commit -m "chore(submodule): bump CkFoundation"`).
5. Same dance for `Plugins/CkTests` when test files change.

**Build/test command.** Run from CkPlugins root with the editor closed:

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --test --test-pattern Goap_ActionSet `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

For Phase U0 (pure rename, no functional change), use the same command but no `--test-pattern` flag — we just want a green build, no tests run.

**Editor must be closed.** A `PreToolUse` hook blocks `Build.bat` when UnrealEditor has this project's log open. If you wrote any code that the editor will hot-reload (AS bindings, for example), close the editor before the next build. The toolbox detects AS hot-reload failure within ~12s and aborts cleanly.

**Wait pattern for shared editor.** Another Claude session may have the editor open on this project. Before running the toolbox, probe `Saved/Logs/CkPlugins.log` for an exclusive write lock; if held, poll every ~60s until free, then proceed.

**Each phase ends with build-test green.** Commits land in the order: submodule changes → submodule commit → CkPlugins pointer bump. Multiple commits per phase are fine.

**AS bindings regeneration.** After any change to public C++ types (handles, UFUNCTIONs, USTRUCTs exposed to AS), the editor must be launched once to regenerate the bindings via `UCkDynamicHandleSubsystem::GenerateHandleTypeRegistry()`. Then close the editor before the next toolbox build.

---

## File structure overview

Every directory rename and file rename in one place. Keep this as a reference while reading the phase tasks.

### Phase U0 — Bundle → ActionSet rename only

**Directory:** `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/` → `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/`

**Files renamed (Bundle → ActionSet inside the filename):**

| Before | After |
|---|---|
| `Bundle/CkGoap_Bundle_Fragment_Data.h` | `ActionSet/CkGoap_ActionSet_Fragment_Data.h` |
| `Bundle/CkGoap_Bundle_Fragment.h` | `ActionSet/CkGoap_ActionSet_Fragment.h` |
| `Bundle/CkGoap_Bundle_Utils.h` | `ActionSet/CkGoap_ActionSet_Utils.h` |
| `Bundle/CkGoap_Bundle_Utils.cpp` | `ActionSet/CkGoap_ActionSet_Utils.cpp` |
| `Bundle/CkGoap_Bundle_Processor.h` | `ActionSet/CkGoap_ActionSet_Processor.h` |
| `Bundle/CkGoap_Bundle_Processor.cpp` | `ActionSet/CkGoap_ActionSet_Processor.cpp` |
| `Bundle/CkGoap_Bundle_Record_Internal.h` | `ActionSet/CkGoap_ActionSet_Record_Internal.h` |

**Symbols renamed (inside any file that references them):**

| Before | After |
|---|---|
| `FCk_Handle_Goap_Bundle` | `FCk_Handle_Goap_ActionSet` |
| `FCk_Fragment_Goap_BundleParamsData` | `FCk_Fragment_Goap_ActionSetParamsData` |
| `FFragment_Goap_Bundle_Params` | `FFragment_Goap_ActionSet_Params` |
| `FFragment_Goap_Bundle_Current` | `FFragment_Goap_ActionSet_Current` |
| `FFragment_Goap_Bundle_ActiveTiers` | `FFragment_Goap_ActionSet_ActiveTiers` (tier name stays — U1 renames Tier→Action) |
| `FFragment_Goap_Bundle_TierCatalogIndex` | `FFragment_Goap_ActionSet_TierCatalogIndex` (tier name stays — U1 renames) |
| `FFragment_RecordOfGoapBundles` | `FFragment_RecordOfGoapActionSets` |
| `FTag_Goap_Bundle_RequiresChainUpdate` | `FTag_Goap_ActionSet_RequiresChainUpdate` |
| `UCk_Utils_Goap_Bundle_UE` | `UCk_Utils_Goap_ActionSet_UE` |
| `UUtils_Signal_Goap_OnBundle_*` (signal type names) | `UUtils_Signal_Goap_OnActionSet_*` |
| `FCk_Delegate_Goap_OnBundle_*` (delegate type names) | `FCk_Delegate_Goap_OnActionSet_*` |
| `_BundleTag` member | `_ActionSetTag` |
| `Get_BundleTag` | `Get_ActionSetTag` |
| `AddBundle` UFUNCTION | `AddActionSet` UFUNCTION |
| `Find_Bundle` UFUNCTION | `Find_ActionSet` UFUNCTION |
| Gameplay tag category `Goap.Bundle` | `Goap.ActionSet` |
| AS namespace `utils_goap_bundle` | `utils_goap_action_set` |
| Display name strings `"[Ck][Goap] Add Bundle"` etc. | `"[Ck][Goap] Add ActionSet"` etc. |

**Test files renamed:**

| Before | After |
|---|---|
| `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_BundleTier_RootOnly.as` | `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_ActionSet_RootOnly.as` |
| `Plugins/CkTests/Script/CkGoap/CkAutoTestAction_Goap_BundleTier_Simple.as` | `Plugins/CkTests/Script/CkGoap/CkAutoTestAction_Goap_ActionSet_Simple.as` |

(The ECS Class name inside the test file changes only enough for AS bindings to find it — actual functional rewrite is Phase U6.)

### Phase U1 — Tier → Action collapse

**Directory:** `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Tier/` → `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/`

**Files renamed:**

| Before | After |
|---|---|
| `Tier/CkGoap_Tier_Fragment.h` | `Action/CkGoap_Action_Fragment.h` |
| `Tier/CkGoap_Tier_Utils.h` | `Action/CkGoap_Action_Utils.h` |
| `Tier/CkGoap_Tier_Utils.cpp` | `Action/CkGoap_Action_Utils.cpp` |
| `Tier/CkGoap_Tier_Processor.h` | `Action/CkGoap_Action_Processor.h` |
| `Tier/CkGoap_Tier_Processor.cpp` | `Action/CkGoap_Action_Processor.cpp` |
| `Tier/CkGoap_Tier_Record_Internal.h` | `Action/CkGoap_Action_Record_Internal.h` |

**Files deleted (per-Tier param struct is gone; per-Action params live on CDO + per-Action fragments):**

| Deleted |
|---|
| `Tier/CkGoap_Tier_Fragment_Data.h` |

**New file:**

| Created |
|---|
| `Action/CkGoap_Action_Fragment_Data.h` — holds `FCk_Handle_Goap_Action` (typesafe handle) + any optional Action params struct used by Request_* verbs |

**Symbols renamed:**

| Before | After |
|---|---|
| `FCk_Handle_Goap_Tier` | `FCk_Handle_Goap_Action` |
| `FCk_Fragment_Goap_TierParamsData` | (deleted — no replacement struct) |
| `FFragment_Goap_Tier_Params` | `FFragment_Goap_Action_Params` |
| `FFragment_Goap_Tier_Current` | `FFragment_Goap_Action_Current` |
| `FFragment_Goap_Tier_Actions` | (collapses into `FFragment_Goap_Action_Tree` + parent-side `_ChildActions`) |
| `FFragment_Goap_Tier_ActionClasses` | (collapses; one Action entity per registered class) |
| `FFragment_Goap_Tier_Requests` | `FFragment_Goap_Action_Requests` |
| `FFragment_Goap_Tier_ReplanThrottle` | `FFragment_Goap_Action_ReplanThrottle` |
| `FFragment_Goap_Tier_SearchState` / `_Result` / `_PlanContext` | `FFragment_Goap_Action_SearchState` / `_Result` / `_PlanContext` |
| `FFragment_Goap_Bundle_ActiveTiers` (carried over from U0) | `FFragment_Goap_ActionSet_ActiveChain` |
| `FFragment_Goap_Bundle_TierCatalogIndex` | (deleted — no matching layer needed) |
| `FFragment_RecordOfGoapTiers` | `FFragment_RecordOfGoapActions` |
| `FTag_Goap_Tier_RequiresSetup` | `FTag_Goap_Action_RequiresSetup` |
| `FTag_Goap_Tier_RequiresInitialPlan` | `FTag_Goap_Action_RequiresInitialPlan` |
| `FTag_Goap_Tier_PlanRequested` | `FTag_Goap_Action_PlanRequested` |
| `UCk_Utils_Goap_Tier_UE` | `UCk_Utils_Goap_Action_UE` |
| `_TierTag` member | (deleted — class-derived `Get_ActionTagForClass` replaces it) |
| `_ActionTag` member (on `UCk_GoapAction_EntityScript`) | (deleted — class-derived identity) |
| `SetActionTag(...)` builder | (deleted) |
| `Get_ActionTag()` getter on entity script | `Get_ActionTagForClass(InClass)` static + non-static `Get_ActionTag()` (delegates to the static) |
| `AddTier` UFUNCTION | (deleted — replaced by `SetRootAction` + `AddAction_ToActionSet` + `AddAction_ToAction` in U2) |
| `AddAction(Tier, ...)` UFUNCTION | (deleted — replaced in U2) |
| `Find_Tier` | `Find_Action` |
| `Get_ActiveTiers` | `Get_ActiveChain` |
| `OnTierActivated` / `OnTierDeactivated` signals | `OnActionActivated` / `OnActionDeactivated` |
| `OnActiveTiersChanged` signal | `OnActiveChainChanged` |
| `Request_ResetActiveTiers` | `Request_ResetActiveChain` |
| `Goap.Tier.*` gameplay tag category | (still exists for class-derived tags but is no longer authored on params) |

**New tree fragment:**

```cpp
// Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h
struct CKGOAP_API FFragment_Goap_Action_Tree
{
    CK_GENERATED_BODY(FFragment_Goap_Action_Tree);
    friend class UCk_Utils_Goap_Action_UE;
    friend class FProcessor_Goap_ActionSet_ChainUpdate;
    friend class FProcessor_Goap_Action_Setup;
    friend class FProcessor_Goap_Action_HandleResult;

private:
    FCk_Handle_Goap_Action _ParentAction;   // invalid for top-level Actions
    TArray<FCk_Handle_Goap_Action> _ChildActions;

public:
    CK_PROPERTY_GET(_ParentAction);
    CK_PROPERTY_GET(_ChildActions);
};
```

### Phase U2 — New API

**Modified files (no creates / no deletes):**

| File | Change |
|---|---|
| `ActionSet/CkGoap_ActionSet_Utils.h/cpp` | Add `SetRootAction`, `AddAction_ToActionSet`, `Request_ResetActiveChain`, `Request_SetRootAction`. |
| `Action/CkGoap_Action_Utils.h/cpp` | Add `AddAction_ToAction`, per-Action `Request_*` verbs (including `Request_SetWorldStateSource`). |
| `EntityScripts/CkGoapAction_EntityScript.h/cpp` | Add `Get_ActionTagForClass(TSubclassOf<...>)` static + `Get_ActionTag()` instance helper. |

### Phase U3 — Processors against unified model

**Modified files:**

| File | Change |
|---|---|
| `Action/CkGoap_Action_Processor.h/cpp` | Per-Action Setup / AutoReplan / HandleRequests / HandleResult. HandleResult now builds `_Plan: TArray<FCk_Handle_Goap_Action>` from A* path (mapping each chosen ActionDef back to its child Action handle via `_ChildActions` membership). |
| `ActionSet/CkGoap_ActionSet_Processor.h/cpp` | `FProcessor_Goap_ActionSet_ChainUpdate` rewritten — no tag matching; `Plan[0]` already a handle. |

### Phase U4 — Subscriber + dirty plumbing

**Modified files:**

| File | Change |
|---|---|
| `WorldState/CkGoap_WorldState_Fragment.h` | (Already retyped to `TArray<FCk_Handle>` per the BundleTier refactor — no change.) |
| `WorldState/CkGoap_WorldState_Utils.cpp` | Subscriber adds/removes now use Action handles. Symbol renames only. |
| `Action/CkGoap_Action_Processor.cpp` (`FProcessor_Goap_Action_AutoReplan`) | Consume `FTag_Goap_Dirty_WorldState` per Action handle. |

### Phase U5 — Diagnostics

**Modified files:**

| File | Change |
|---|---|
| `Action/CkGoap_Action_Processor.cpp` | `FProcessor_Goap_Action_Setup` — populate `_InvalidGoal` (effects-vs-WS check). |
| `ActionSet/CkGoap_ActionSet_Processor.cpp` | `FProcessor_Goap_ActionSet_Setup` — populate `_DependencyCycles` via Tarjan SCC on `_ChildActions`. |
| `ActionSet/CkGoap_ActionSet_Utils.cpp` | Setup-time validation: duplicate `ActionClass` in `AddAction_ToActionSet` / `AddAction_ToAction`. |

### Phase U6 — Smoke test rewrite

| File | Change |
|---|---|
| `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_ActionSet_RootOnly.as` | Rewrite against `AddActionSet` + `SetRootAction`. |
| `Plugins/CkTests/Script/CkGoap/CkAutoTestAction_Goap_ActionSet_Simple.as` | Drop `SetActionTag` call from `DefineAction` (class-derived identity now). |

### Phase U7 — Remaining 13 tests

13 new `.as` files in `Plugins/CkTests/Script/CkGoap/` matching the table in spec §9. Each test typically needs 1–3 supporting action subclass files.

### Phase U8 — Polish + docs

| File | Change |
|---|---|
| `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md` | Rewrite for ActionSet/Action model. |
| `Plugins/CkFoundation/CLAUDE.md` | Update CkGoap quick-reference section. |
| `docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md` | Add a banner at top: "**Superseded by ActionSetUnification-design.md**". Leave on disk for history. |

---

## Phase U0 — Bundle → ActionSet rename

**Goal:** Pure rename, zero semantic change. After this phase, every `Bundle` reference is `ActionSet`. `Tier` references stay as-is — that's U1.

**Build expectation at phase end:** Editor builds clean. AS bindings regenerate. No tests run (no behavior change).

### Task U0.1 — Rename Bundle directory

**Files:**
- Rename: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Bundle/` → `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/`

- [ ] **Step 1: Rename the directory using git**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git mv Source/CkGoap/Public/CkGoap/Bundle Source/CkGoap/Public/CkGoap/ActionSet
```

- [ ] **Step 2: Verify the rename landed in git status**

```bash
git status
```

Expected: 7 file renames listed (Fragment_Data.h, Fragment.h, Utils.h, Utils.cpp, Processor.h, Processor.cpp, Record_Internal.h).

### Task U0.2 — Rename Bundle filenames inside ActionSet/

**Files:**
- Rename: `ActionSet/CkGoap_Bundle_*.h/cpp` → `ActionSet/CkGoap_ActionSet_*.h/cpp` (7 files)

- [ ] **Step 1: Rename each file**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git mv Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_Bundle_Fragment_Data.h     Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Fragment_Data.h
git mv Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_Bundle_Fragment.h          Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Fragment.h
git mv Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_Bundle_Utils.h             Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Utils.h
git mv Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_Bundle_Utils.cpp           Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Utils.cpp
git mv Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_Bundle_Processor.h         Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Processor.h
git mv Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_Bundle_Processor.cpp       Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Processor.cpp
git mv Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_Bundle_Record_Internal.h   Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Record_Internal.h
```

- [ ] **Step 2: Verify all renames in git status**

```bash
git status | grep -E "renamed.*Bundle|renamed.*ActionSet"
```

Expected: 7 lines showing each file renamed from Bundle to ActionSet.

### Task U0.3 — Symbol search/replace inside CkGoap

**Files:**
- Modify: every `.h` and `.cpp` file under `Plugins/CkFoundation/Source/CkGoap/` that mentions `Bundle`.

The Grep tool can locate them; the Edit tool applies each rename. Do all renames as case-sensitive whole-word replacements unless noted otherwise.

- [ ] **Step 1: Find every file referencing Bundle**

Run via the Grep tool: pattern `Bundle`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `files_with_matches`. Save the list.

- [ ] **Step 2: For each file, apply the symbol renames**

Use Edit with `replace_all: true`. Apply each rename below in order. Reuse the same approach for every file in the list from step 1.

Symbol pairs (apply to every file from step 1):

```
FCk_Handle_Goap_Bundle              → FCk_Handle_Goap_ActionSet
FCk_Fragment_Goap_BundleParamsData  → FCk_Fragment_Goap_ActionSetParamsData
FFragment_Goap_Bundle_Params        → FFragment_Goap_ActionSet_Params
FFragment_Goap_Bundle_Current       → FFragment_Goap_ActionSet_Current
FFragment_Goap_Bundle_ActiveTiers   → FFragment_Goap_ActionSet_ActiveTiers
FFragment_Goap_Bundle_TierCatalogIndex → FFragment_Goap_ActionSet_TierCatalogIndex
FFragment_RecordOfGoapBundles       → FFragment_RecordOfGoapActionSets
FTag_Goap_Bundle_RequiresChainUpdate → FTag_Goap_ActionSet_RequiresChainUpdate
UCk_Utils_Goap_Bundle_UE            → UCk_Utils_Goap_ActionSet_UE
UUtils_Signal_Goap_OnBundle         → UUtils_Signal_Goap_OnActionSet  (matches both Activated/Deactivated/Changed variants by prefix)
FCk_Delegate_Goap_OnBundle          → FCk_Delegate_Goap_OnActionSet
_BundleTag                          → _ActionSetTag
Get_BundleTag                       → Get_ActionSetTag
Goap.Bundle                         → Goap.ActionSet         (in tag categories and string literals)
AddBundle                           → AddActionSet
Find_Bundle                         → Find_ActionSet
Bundle/CkGoap_Bundle_                → ActionSet/CkGoap_ActionSet_  (in #include paths)
"[Ck][Goap] Add Bundle"             → "[Ck][Goap] Add ActionSet"   (DisplayName meta)
"[Ck][Goap] Find Bundle"            → "[Ck][Goap] Find ActionSet"
"[Ck][Goap] Set Bundle"             → "[Ck][Goap] Set ActionSet"
"[Ck][Goap] Reset Active Tiers"     → "[Ck][Goap] Reset Active Tiers"  (stays — U1 will retouch)
```

For each file, after applying the renames, save and move to the next.

- [ ] **Step 3: Sanity check — no stray Bundle references remain in CkGoap code**

Grep: pattern `Bundle`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `content`, head_limit `50`.

Expected: zero matches in the source files. If matches remain (e.g. in unrelated comments mentioning "the previous Bundle/Tier refactor"), evaluate each — if it's a historical comment, replace with the new term or leave only if the comment is explicitly about historical Bundle/Tier history.

### Task U0.4 — Rename smoke test + helper

**Files:**
- Rename: `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_BundleTier_RootOnly.as` → `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_ActionSet_RootOnly.as`
- Rename: `Plugins/CkTests/Script/CkGoap/CkAutoTestAction_Goap_BundleTier_Simple.as` → `Plugins/CkTests/Script/CkGoap/CkAutoTestAction_Goap_ActionSet_Simple.as`

- [ ] **Step 1: Rename both files via git in the CkTests submodule**

```bash
cd D:/Repos/CkPlugins/Plugins/CkTests
git mv Script/CkGoap/CkAutoTest_Goap_BundleTier_RootOnly.as          Script/CkGoap/CkAutoTest_Goap_ActionSet_RootOnly.as
git mv Script/CkGoap/CkAutoTestAction_Goap_BundleTier_Simple.as      Script/CkGoap/CkAutoTestAction_Goap_ActionSet_Simple.as
```

- [ ] **Step 2: Update class names + AS namespace references inside both files**

Open `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_ActionSet_RootOnly.as`. Edit `replace_all: true`:

```
class Ck_AutoTest_Goap_BundleTier_RootOnly      → class Ck_AutoTest_Goap_ActionSet_RootOnly
class ACk_AutoTest_Goap_BundleTier_RootOnly     → class ACk_AutoTest_Goap_ActionSet_RootOnly
utils_goap_bundle                                → utils_goap_action_set
FCk_Handle_Goap_Bundle                           → FCk_Handle_Goap_ActionSet
AddBundle                                        → AddActionSet
Find_Bundle                                      → Find_ActionSet
```

Repeat for `CkAutoTestAction_Goap_ActionSet_Simple.as`:

```
class Ck_AutoTestAction_Goap_BundleTier_Simple   → class Ck_AutoTestAction_Goap_ActionSet_Simple
```

(The action subclass file itself shouldn't reference Bundle — but run the renames listed in U0.3 against it anyway for safety.)

- [ ] **Step 3: Regenerate the AS auto-test actor wrapper**

The `CkTests_AutoTestActors.as` wrapper file is generated. Update via the toolbox:

```powershell
./CkAuto/UnrealToolbox.exe --discover-fresh `
    --output=Saved/Logs/Discover.log `
    --project="D:\Repos\CkPlugins"
```

Expected: wrapper file updated to reflect the renamed test class.

### Task U0.5 — Build and verify

- [ ] **Step 1: Confirm UnrealEditor is closed**

The PreToolUse hook blocks Build.bat when the editor is open. If you have an editor running on this project, close it.

- [ ] **Step 2: Run the toolbox build (no test pattern — just compile)**

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

Expected: build succeeds. Watch for compile errors mentioning `Bundle` — those indicate a missed rename.

- [ ] **Step 3: If build fails on a missed rename, fix it and re-run**

Open `Saved/Logs/BuildTest.log`; locate the failing line. Apply the rename. Re-run step 2 until green.

- [ ] **Step 4: Launch editor briefly to regenerate AS bindings**

Open the editor (the UE launcher or via runreal). Wait for AS post-compile to complete. Verify in the editor's output log that no errors mention Bundle.

Close the editor cleanly.

- [ ] **Step 5: Re-run toolbox build to confirm AS bindings are stable**

Same command as step 2. Expected: green.

### Task U0.6 — Commit Phase U0

- [ ] **Step 1: Inspect changes in CkFoundation submodule**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git status
git diff --stat
```

Expected: ~30 files modified, 7 files renamed.

- [ ] **Step 2: Commit in CkFoundation submodule**

```bash
git add -A
git commit -m "$(cat <<'EOF'
refactor(CkGoap): Phase U0 — rename Bundle → ActionSet

Pure rename pass — no semantic change. Directory Bundle/ → ActionSet/,
file names CkGoap_Bundle_* → CkGoap_ActionSet_*, and all symbol references
(handles, fragments, utils, signals, delegates, UFUNCTIONs, gameplay tag
categories, AS namespaces).

Tier remains Tier in this phase — Phase U1 handles the Tier → Action
collapse and the structural changes that come with it.

Build-test green via UnrealToolbox.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 3: Commit in CkTests submodule**

```bash
cd D:/Repos/CkPlugins/Plugins/CkTests
git add -A
git commit -m "$(cat <<'EOF'
test(CkGoap): rename BundleTier smoke test to ActionSet

Phase U0 of the ActionSet unification refactor — rename only.
Functional rewrite happens in Phase U6.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 4: Bump submodule pointers in CkPlugins root**

```bash
cd D:/Repos/CkPlugins
git add Plugins/CkFoundation Plugins/CkTests
git commit -m "$(cat <<'EOF'
chore(submodule): bump CkFoundation + CkTests (U0 Bundle → ActionSet rename)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 5: Confirm clean state**

```bash
git status
```

Expected: "nothing to commit, working tree clean" (or only the unrelated pre-existing `Config/DefaultGameplayTags.ini` modification noted in the continuation prompt).

**End of Phase U0.**

---

## Phase U1 — Tier → Action collapse

**Goal:** Delete tier-specific entity type, promote tier fragments to Action equivalents, add the tree fragment (`_ParentAction` + `_ChildActions`), change `_Plan` element type from `TSubclassOf<...>` to `FCk_Handle_Goap_Action`, delete `_ActionTag` matching layer. After this phase: code compiles, but the API surface still uses the (now-renamed) old verbs `AddTier`/`AddAction(Tier,...)` until Phase U2 replaces them.

**Build expectation:** Compiles clean. Smoke test still in BundleTier form — `--test-pattern Goap_ActionSet` will return zero tests (we renamed the file but won't run it until U6).

### Task U1.1 — Rename Tier directory

- [ ] **Step 1: Rename directory**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git mv Source/CkGoap/Public/CkGoap/Tier Source/CkGoap/Public/CkGoap/Action
```

### Task U1.2 — Delete TierParamsData

The per-Tier params struct goes away entirely. Per-Action params live on the action class CDO + per-Action fragments.

**Files:**
- Delete: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Fragment_Data.h`

- [ ] **Step 1: Delete file**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git rm Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Fragment_Data.h
```

- [ ] **Step 2: Find every #include of the deleted file and remove them**

Grep: pattern `CkGoap_Tier_Fragment_Data\.h`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `files_with_matches`.

For each file in the result, delete the corresponding `#include "CkGoap/Tier/CkGoap_Tier_Fragment_Data.h"` line via Edit.

### Task U1.3 — Rename Tier files

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git mv Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Fragment.h         Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h
git mv Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Utils.h            Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Utils.h
git mv Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Utils.cpp          Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Utils.cpp
git mv Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Processor.h        Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.h
git mv Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Processor.cpp      Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.cpp
git mv Source/CkGoap/Public/CkGoap/Action/CkGoap_Tier_Record_Internal.h  Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Record_Internal.h
```

- [ ] Verify renames in git status.

### Task U1.4 — Create new Action_Fragment_Data.h with typesafe handle

**File:**
- Create: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment_Data.h`

The handle was previously in the deleted `CkGoap_Tier_Fragment_Data.h`. Recreate it.

- [ ] **Step 1: Create the file**

```cpp
// Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment_Data.h
#pragma once

#include "CkEcs/Handle/CkHandle_TypeSafe.h"

#include "CkGoap_Action_Fragment_Data.generated.h"

// ====================================================================================================================
// TYPESAFE HANDLE — one entity per registered Action in an ActionSet's catalog.
// Action entities carry the action's def (CDO-extracted), tree edges
// (_ParentAction, _ChildActions), runtime planner state, and an active-parent
// breadcrumb used by the bundle's ChainUpdate processor.
// ====================================================================================================================

USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Handle_Goap_Action : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_Goap_Action);
};

// ====================================================================================================================
```

- [ ] **Step 2: Find every reference to FCk_Handle_Goap_Tier and update includes**

Grep: pattern `FCk_Handle_Goap_Tier`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `files_with_matches`.

For each file, Edit:
- Replace `#include "CkGoap/Tier/CkGoap_Tier_Fragment_Data.h"` with `#include "CkGoap/Action/CkGoap_Action_Fragment_Data.h"`
- Replace `FCk_Handle_Goap_Tier` with `FCk_Handle_Goap_Action` (replace_all)

### Task U1.5 — Symbol search/replace for Tier → Action

Run the same renames as U0.3 but for Tier symbols. Use Grep to find files then Edit replace_all.

- [ ] **Step 1: Find every Tier reference**

Grep: pattern `Tier`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `files_with_matches`.

- [ ] **Step 2: Apply renames per the table**

For each file, apply (replace_all):

```
FFragment_Goap_Tier_Params           → FFragment_Goap_Action_Params
FFragment_Goap_Tier_Current          → FFragment_Goap_Action_Current
FFragment_Goap_Tier_Requests         → FFragment_Goap_Action_Requests
FFragment_Goap_Tier_ReplanThrottle   → FFragment_Goap_Action_ReplanThrottle
FFragment_Goap_Tier_SearchState      → FFragment_Goap_Action_SearchState
FFragment_Goap_Tier_Result           → FFragment_Goap_Action_Result
FFragment_Goap_Tier_PlanContext      → FFragment_Goap_Action_PlanContext
FFragment_Goap_Bundle_ActiveTiers    → FFragment_Goap_ActionSet_ActiveChain
FFragment_RecordOfGoapTiers          → FFragment_RecordOfGoapActions
FTag_Goap_Tier_RequiresSetup         → FTag_Goap_Action_RequiresSetup
FTag_Goap_Tier_RequiresInitialPlan   → FTag_Goap_Action_RequiresInitialPlan
FTag_Goap_Tier_PlanRequested         → FTag_Goap_Action_PlanRequested
UCk_Utils_Goap_Tier_UE               → UCk_Utils_Goap_Action_UE
OnTierActivated                      → OnActionActivated
OnTierDeactivated                    → OnActionDeactivated
OnActiveTiersChanged                 → OnActiveChainChanged
Request_ResetActiveTiers             → Request_ResetActiveChain
Get_ActiveTiers                      → Get_ActiveChain
Find_Tier                            → Find_Action
Tier/CkGoap_Tier_                    → Action/CkGoap_Action_   (#include paths)
"[Ck][Goap] Reset Active Tiers"      → "[Ck][Goap] Reset Active Chain"
"[Ck][Goap] Get Active Tiers"        → "[Ck][Goap] Get Active Chain"
"[Ck][Goap] Find Tier"               → "[Ck][Goap] Find Action"
utils_goap_tier                      → utils_goap_action       (AS namespace)
```

### Task U1.6 — Delete _TierTag from TierParams equivalent (now ActionParams)

The renamed `FFragment_Goap_Action_Params` still has a `_TierTag` field carried over from before. Delete it — Action identity is class-derived now.

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h`

- [ ] **Step 1: Locate the FFragment_Goap_Action_Params struct and remove the _TierTag field + its accessors**

The struct currently contains something like:

```cpp
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true, Categories = "Goap.Tier"))
    FGameplayTag _TierTag;
    // ... other fields ...

public:
    CK_PROPERTY_GET(_TierTag);
    // ... other accessors ...
```

Remove the `_TierTag` UPROPERTY block and the `CK_PROPERTY_GET(_TierTag);` line. Leave the rest of the struct intact.

Also remove from any `CK_DEFINE_CONSTRUCTORS(...)` macro call inside the struct — `_TierTag` should not be a constructor parameter anymore.

### Task U1.6b — Restructure the action-def fragment

The pre-unification code has `FFragment_Goap_Tier_Actions` carrying `TArray<FActionDef>` — the tier's catalog of action defs scanned from CDOs. In the unified model, each Action entity carries its OWN def (preconditions, effects, cost) — there's one def per Action entity, not a list. Restructure the fragment to per-Action shape.

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h`

- [ ] **Step 1: Rename `FFragment_Goap_Action_Actions` (auto-renamed in U1.5 from `FFragment_Goap_Tier_Actions`) to `FFragment_Goap_Action_Definition`**

```cpp
struct CKGOAP_API FFragment_Goap_Action_Definition
{
public:
    CK_GENERATED_BODY(FFragment_Goap_Action_Definition);

    friend class ck::FProcessor_Goap_Action_Setup;
    friend class ck::FProcessor_Goap_Action_HandleRequests;
    friend class ck::FProcessor_Goap_ActionSet_ChainUpdate;
    friend class ck::FProcessor_Goap_Action_HandleResult;

private:
    // This Action's own def — extracted once from the CDO at Setup time.
    TArray<ck::goap::FWorldStateCondition_Raw> _Preconditions;
    TArray<ck::goap::FWorldStateEffect_Raw>    _Effects;
    float _Cost = 1.0f;

    // Pre-resolved form of _Effects keyed against this Action's resolved WS.
    // Populated at Setup, read at chain extension to assign to ChildCurrent._Goal.
    FCk_GoapWS_Conditions _GoalFromEffects;

    // Effect keys not in the resolved WS — populated at Setup, surfaced via
    // Get_InvalidGoal.
    TArray<FCk_GoapWS_Condition_Authored> _InvalidGoal;

    // Pre-built ActionDef cache (the same Preconditions/Effects/Cost packed
    // into the FActionDef shape the A* planner expects). Built once at Setup
    // and inherited by parents that aggregate this Action as a planning
    // candidate.
    ck::goap::FActionDef _CachedActionDef;

public:
    CK_PROPERTY_GET(_Preconditions);
    CK_PROPERTY_GET(_Effects);
    CK_PROPERTY_GET(_Cost);
    CK_PROPERTY_GET(_GoalFromEffects);
    CK_PROPERTY_GET(_InvalidGoal);

    auto AsActionDef() const -> const ck::goap::FActionDef& { return _CachedActionDef; }
};
```

- [ ] **Step 2: Remove the old TArray<FActionDef>-based field**

The carried-over struct previously had `TArray<FActionDef> _Actions;` (which represented the tier's catalog of action defs). Delete that field entirely — the new shape is per-Action, not per-collection. Also delete the corresponding `CK_PROPERTY_GET(_Actions);` line.

- [ ] **Step 3: Add the friend declaration in CkGoap_Action_Processor.h**

The Setup processor needs access to `_Preconditions`, `_Effects`, `_Cost`, `_GoalFromEffects`, `_InvalidGoal`, `_CachedActionDef`. The friend list in step 1 already covers it.

### Task U1.6c — Add ActionSet WorldStateSource fragment

`SetRootAction(ActionSet, Class, InitialWS)` stores the WS on the ActionSet. Create the fragment that holds it.

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Fragment.h`

- [ ] **Step 1: Add the fragment near the other ActionSet fragments**

```cpp
struct CKGOAP_API FFragment_Goap_ActionSet_WorldStateSource
{
public:
    CK_GENERATED_BODY(FFragment_Goap_ActionSet_WorldStateSource);
    friend class ::UCk_Utils_Goap_ActionSet_UE;
    friend class ck::FProcessor_Goap_ActionSet_ChainUpdate;

private:
    FCk_Handle_Goap_WorldState _WorldStateSource;

public:
    CK_PROPERTY_GET(_WorldStateSource);
};
```

- [ ] **Step 2: Add to the ActionSet creation path**

In `UCk_Utils_Goap_ActionSet_UE::AddActionSet` (the renamed AddBundle), stamp this fragment at construction time:

```cpp
ActionSetEntity.AddOrGet<FFragment_Goap_ActionSet_WorldStateSource>();
```

The actual handle gets set later by `SetRootAction` per Task U2.1.

### Task U1.7 — Add the tree fragment

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h`

- [ ] **Step 1: Add FFragment_Goap_Action_Tree near the existing Action fragments**

After the existing fragment definitions in `CkGoap_Action_Fragment.h`, add:

```cpp
// ====================================================================================================================
// TREE — parent pointer + children list. Established at AddAction time and
// read by ChainUpdate (for chain extension) and HandleResult (when mapping
// A* path elements to child handles).
// ====================================================================================================================

namespace ck
{
    class FProcessor_Goap_Action_Setup;
    class FProcessor_Goap_Action_HandleResult;
    class FProcessor_Goap_ActionSet_ChainUpdate;
}

struct CKGOAP_API FFragment_Goap_Action_Tree
{
public:
    CK_GENERATED_BODY(FFragment_Goap_Action_Tree);

    friend class ::UCk_Utils_Goap_Action_UE;
    friend class ::UCk_Utils_Goap_ActionSet_UE;
    friend class ck::FProcessor_Goap_Action_Setup;
    friend class ck::FProcessor_Goap_Action_HandleResult;
    friend class ck::FProcessor_Goap_ActionSet_ChainUpdate;

private:
    FCk_Handle_Goap_Action _ParentAction;
    TArray<FCk_Handle_Goap_Action> _ChildActions;

public:
    CK_PROPERTY_GET(_ParentAction);
    CK_PROPERTY_GET(_ChildActions);
};
```

### Task U1.8 — Change Plan element type to action handles

The `_Plan` field on `FFragment_Goap_Action_Current` is currently `TArray<TSubclassOf<UCk_GoapAction_EntityScript>>` (carried over from Tier). Change to `TArray<FCk_Handle_Goap_Action>`.

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h`

- [ ] **Step 1: Locate the _Plan field**

Look inside `FFragment_Goap_Action_Current` for:

```cpp
TArray<TSubclassOf<UCk_GoapAction_EntityScript>> _Plan;
```

- [ ] **Step 2: Replace with handle-typed plan**

```cpp
// _Plan elements are child Action handles, in plan execution order. The handles
// belong to this Action's _ChildActions (set up at AddAction time). HandleResult
// populates this by mapping each A* path element (a child ActionDef) back to
// its registered child Action handle.
TArray<FCk_Handle_Goap_Action> _Plan;
```

Update the `CK_PROPERTY_GET(_Plan)` line — no change needed (the macro still works with the new type).

- [ ] **Step 3: Add a convenience getter for the action classes**

```cpp
public:
    // Returns the action classes corresponding to _Plan's handles, in the same
    // order. Convenience for consumers that want classes (e.g. the action runner).
    auto Get_PlanClasses() const -> TArray<TSubclassOf<UCk_GoapAction_EntityScript>>;
```

Implement in the .cpp:

```cpp
// CkGoap_Action_Fragment.cpp
auto
    FFragment_Goap_Action_Current::
    Get_PlanClasses() const
    -> TArray<TSubclassOf<UCk_GoapAction_EntityScript>>
{
    auto Result = TArray<TSubclassOf<UCk_GoapAction_EntityScript>>{};
    Result.Reserve(_Plan.Num());
    for (const auto& ActionHandle : _Plan)
    {
        const auto& Params = ActionHandle.template Get<FFragment_Goap_Action_Params>();
        Result.Add(Params.Get_ActionClass());
    }
    return Result;
}
```

(If `_ActionClass` doesn't yet live on `FFragment_Goap_Action_Params`, add it as a field — see Task U1.10.)

### Task U1.9 — Delete _ActionTag from UCk_GoapAction_EntityScript

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.cpp`

- [ ] **Step 1: Remove the SetActionTag builder declaration**

In the .h, delete this block:

```cpp
// Declares which sub-tier (if any) this action delegates to. ...
UFUNCTION(BlueprintCallable, Category = "Ck|GOAP|Action",
    DisplayName = "[Ck][GOAP] Set Action Tag")
void
SetActionTag(UPARAM(meta = (Categories = "Goap.Tier")) FGameplayTag InTag);
```

- [ ] **Step 2: Remove the _ActionTag member**

In the .h's private section, delete:

```cpp
// Set via SetActionTag(...) in DefineAction. ...
FGameplayTag _ActionTag;
```

- [ ] **Step 3: Remove the instance Get_ActionTag()**

Delete:

```cpp
auto Get_ActionTag() const -> FGameplayTag { return _ActionTag; }
```

- [ ] **Step 4: In the .cpp, remove SetActionTag's implementation + the _ActionTag reset**

In `Reset()` or equivalent, delete:

```cpp
_ActionTag = FGameplayTag{};
```

Delete the `SetActionTag(...)` function definition entirely.

- [ ] **Step 5: Search for any remaining references to _ActionTag / Get_ActionTag**

Grep: pattern `_ActionTag|Get_ActionTag`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `content`.

Each match is either:
- A processor that called `CDO->Get_ActionTag()` — needs to change to `UCk_GoapAction_EntityScript::Get_ActionTagForClass(ActionClass)` (we'll add this in Task U1.11).
- A debug-print that referenced the value — update to use `Get_ActionTagForClass` or remove.

Mark the remaining references with a `// FIXME(U1.11)` comment; we'll fix them in Task U1.11.

### Task U1.10 — Add ActionClass to Action_Params

`FFragment_Goap_Action_Params` needs to know which action class this entity represents.

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Fragment.h`

- [ ] **Step 1: Add the field**

In `FFragment_Goap_Action_Params`, add a private member:

```cpp
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSubclassOf<UCk_GoapAction_EntityScript> _ActionClass;

public:
    CK_PROPERTY_GET(_ActionClass);
```

Add `_ActionClass` to the `CK_DEFINE_CONSTRUCTORS(...)` macro arguments if present, so the struct can be constructed with the class up front. Otherwise initialise it via a setter.

### Task U1.11 — Add Get_ActionTagForClass static helper

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/EntityScripts/CkGoapAction_EntityScript.cpp`

- [ ] **Step 1: Declare the static helper + an instance forwarder**

In the .h, after the builders section:

```cpp
public:
    // The action's identity tag, derived from its class name via
    // UCk_Utils_Object_UE::Get_TagFromClassName. Mirrors SM's pattern
    // (UCk_SmState_EntityScript::Get_StateTagForClass). Used for debug
    // identification and not for chain matching — chain extension in the
    // unified model reads Plan[0] handles directly.
    UFUNCTION(BlueprintPure,
        Category = "Ck|GOAP|Action",
        DisplayName = "[Ck][GOAP] Get Action Tag For Class")
    static FGameplayTag
    Get_ActionTagForClass(
        TSubclassOf<UCk_GoapAction_EntityScript> InClass);

    UFUNCTION(BlueprintPure,
        Category = "Ck|GOAP|Action",
        DisplayName = "[Ck][GOAP] Get Action Tag")
    FGameplayTag
    Get_ActionTag() const;
```

- [ ] **Step 2: Implement in the .cpp**

```cpp
#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

auto
    UCk_GoapAction_EntityScript::
    Get_ActionTagForClass(
        TSubclassOf<UCk_GoapAction_EntityScript> InClass)
    -> FGameplayTag
{
    CK_ENSURE_IF_NOT(ck::IsValid(InClass),
        TEXT("Invalid action class in Get_ActionTagForClass"))
    { return {}; }

    return UCk_Utils_Object_UE::Get_TagFromClassName(
        InClass,
        ck::Format_UE(TEXT("Auto-generated action tag for {}"), InClass));
}

auto
    UCk_GoapAction_EntityScript::
    Get_ActionTag() const
    -> FGameplayTag
{
    return Get_ActionTagForClass(GetClass());
}
```

- [ ] **Step 3: Fix the FIXME-tagged callers from Task U1.9**

For each FIXME location, replace `CDO->Get_ActionTag()` with:

```cpp
const auto ActionTag = UCk_GoapAction_EntityScript::Get_ActionTagForClass(ActionClass);
```

(Where `ActionClass` is whatever local variable holds the `TSubclassOf<UCk_GoapAction_EntityScript>` in that scope.)

### Task U1.12 — Fix the TierCatalogIndex usage in ChainUpdate (temporary; U3 deletes it)

The bundle processor (`CkGoap_ActionSet_Processor.cpp` after U0) still uses `FFragment_Goap_ActionSet_TierCatalogIndex` and the `_ActionTag` matching logic. We need it to keep compiling through U1; Phase U3 replaces the whole ChainUpdate function.

- [ ] **Step 1: Make TierCatalogIndex temporarily ChildActions-aware**

In `CkGoap_ActionSet_Processor.cpp`, in the chain-update body, the lookup currently looks like:

```cpp
const auto NextActionTag = CDO->Get_ActionTag();
const auto* MatchingTierPtr = CatalogIndex.Find(NextActionTag);
```

Change to:

```cpp
// FIXME(U3): this whole match-via-tag block is replaced in Phase U3 by a
// direct read of NextChildHandle from the current Action's _Plan.
const auto NextActionTag = UCk_GoapAction_EntityScript::Get_ActionTagForClass(NextActionClass);
const auto* MatchingActionPtr = CatalogIndex.Find(NextActionTag);
```

This keeps the file compiling. The CatalogIndex is still keyed by `_TierTag` (now stale because we deleted that field in U1.6), so the index will be empty at runtime — but we're not running tests in U1.

### Task U1.13 — Build and verify Phase U1 compiles

- [ ] **Step 1: Close editor if open, then run toolbox build**

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

Expected: build succeeds. If failures mention `_TierTag` / `Tier_` / `_ActionTag`, locate and apply the missing rename or deletion.

- [ ] **Step 2: Iterate until build is green**

### Task U1.14 — Commit Phase U1

- [ ] **Step 1: Commit in CkFoundation**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git add -A
git commit -m "$(cat <<'EOF'
refactor(CkGoap): Phase U1 — collapse Tier into Action

· Renamed Tier/ directory → Action/
· Renamed CkGoap_Tier_* → CkGoap_Action_*
· Deleted CkGoap_Tier_Fragment_Data.h (no separate Action params struct)
· Created CkGoap_Action_Fragment_Data.h with FCk_Handle_Goap_Action
· Added FFragment_Goap_Action_Tree (parent + children)
· Changed _Plan element type to FCk_Handle_Goap_Action
· Deleted _ActionTag field + SetActionTag builder on UCk_GoapAction_EntityScript
· Added Get_ActionTagForClass static (mirrors SM's Get_StateTagForClass)

ChainUpdate body in ActionSet processor temporarily kept via tag lookup
through Get_ActionTagForClass — replaced fully in Phase U3.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 2: Bump submodule pointer**

```bash
cd D:/Repos/CkPlugins
git add Plugins/CkFoundation
git commit -m "$(cat <<'EOF'
chore(submodule): bump CkFoundation (U1 Tier → Action collapse)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

**End of Phase U1.**

---

## Phase U2 — New API surface

**Goal:** Replace the carried-over `AddTier` / `AddAction(Tier, ...)` verbs with the unified API: `AddActionSet`, `SetRootAction`, `AddAction_ToActionSet`, `AddAction_ToAction`. Update query / request verbs to use Action handles throughout. AS bindings regenerate. The smoke test still doesn't run (rewrite in U6) but the new API must compile.

### Task U2.1 — Replace AddTier with SetRootAction + AddAction_ToActionSet

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Utils.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Utils.cpp`

- [ ] **Step 1: Delete the AddTier UFUNCTION (declaration + definition)**

In `.h`, remove:

```cpp
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Add Tier")
static FCk_Handle_Goap_Action AddTier(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InBundle,
    const FCk_Fragment_Goap_TierParamsData& InParams);
```

(Note: the input type `FCk_Fragment_Goap_TierParamsData` was deleted in U1.2; the file may not compile right now if anything still references AddTier — Tasks below replace the call sites.)

In `.cpp`, remove the corresponding implementation.

- [ ] **Step 2: Add SetRootAction**

In `.h`:

```cpp
// Designate the entry-point Action of an ActionSet. Must be called once
// per ActionSet before any planning can happen. InInitialWorldState
// becomes the ActionSet's _WorldStateSource. The chosen ActionClass is
// added to the catalog and marked as the root.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Set Root Action")
static FCk_Handle_Goap_Action SetRootAction(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InRootActionClass,
    UPARAM(ref) FCk_Handle_Goap_WorldState& InInitialWorldState);
```

In `.cpp`:

```cpp
auto
    UCk_Utils_Goap_ActionSet_UE::
    SetRootAction(
        FCk_Handle_Goap_ActionSet& InActionSet,
        TSubclassOf<UCk_GoapAction_EntityScript> InRootActionClass,
        FCk_Handle_Goap_WorldState& InInitialWorldState)
    -> FCk_Handle_Goap_Action
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActionSet),
        TEXT("Invalid ActionSet handle [{}].{}"), InActionSet, ck::Context(this))
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InRootActionClass),
        TEXT("Invalid root action class in SetRootAction.{}"), ck::Context(this))
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InInitialWorldState),
        TEXT("Invalid initial WorldState handle in SetRootAction.{}"), ck::Context(this))
    { return {}; }

    // Set the ActionSet's WS source.
    auto& WSFragment = InActionSet.Get<FFragment_Goap_ActionSet_WorldStateSource>();
    WSFragment._WorldStateSource = InInitialWorldState;

    // Create the root Action entity (or reuse an existing catalog entry).
    auto RootHandle = DoCreateOrFindActionEntity(InActionSet, InRootActionClass);

    // Mark as the ActionSet's root.
    auto& Current = InActionSet.Get<FFragment_Goap_ActionSet_Current>();
    Current._RootAction = RootHandle;

    // Seed the active chain with the root.
    auto& ActiveChain = InActionSet.Get<FFragment_Goap_ActionSet_ActiveChain>();
    ActiveChain._Chain.Reset();
    ActiveChain._Chain.Add(RootHandle);

    return RootHandle;
}
```

(`DoCreateOrFindActionEntity` is a private helper added in Task U2.2.)

- [ ] **Step 3: Add AddAction_ToActionSet**

In `.h`:

```cpp
// Register an Action at the top level of an ActionSet (a sibling of the
// root). The new Action's _ParentAction stays invalid.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Add Action (To ActionSet)")
static FCk_Handle_Goap_Action AddAction_ToActionSet(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);
```

In `.cpp`:

```cpp
auto
    UCk_Utils_Goap_ActionSet_UE::
    AddAction_ToActionSet(
        FCk_Handle_Goap_ActionSet& InActionSet,
        TSubclassOf<UCk_GoapAction_EntityScript> InActionClass)
    -> FCk_Handle_Goap_Action
{
    CK_ENSURE_IF_NOT(ck::IsValid(InActionSet),
        TEXT("Invalid ActionSet handle [{}].{}"), InActionSet, ck::Context(this))
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InActionClass),
        TEXT("Invalid action class in AddAction_ToActionSet.{}"), ck::Context(this))
    { return {}; }

    return DoCreateOrFindActionEntity(InActionSet, InActionClass);
}
```

### Task U2.2 — Add the private helper DoCreateOrFindActionEntity

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Utils.cpp` (private file-scope helper)

- [ ] **Step 1: Add the helper**

```cpp
// ----------------------------------------------------------------------------------------------------------------
// Internal: create-or-find an Action entity in an ActionSet's catalog.
// Two AddAction_* calls with the same ActionClass return the SAME handle —
// the catalog is keyed by action class. Tree edges (parent / children) are
// established by the caller after this returns.
// ----------------------------------------------------------------------------------------------------------------

namespace ck_ckgoap_actionset_internal
{
    auto DoCreateOrFindActionEntity(
        FCk_Handle_Goap_ActionSet& InActionSet,
        TSubclassOf<UCk_GoapAction_EntityScript> InActionClass)
        -> FCk_Handle_Goap_Action
    {
        // Check if this class is already registered in the catalog.
        const auto Existing = UCk_Utils_Goap_ActionSet_UE::Find_ActionByClass(InActionSet, InActionClass);
        if (ck::IsValid(Existing))
        {
            ck::goap::Warning(
                TEXT("Action class [{}] is already registered in ActionSet [{}]; returning existing handle."),
                InActionClass, InActionSet);
            return Existing;
        }

        // Spawn a new Action entity as a record-child of the ActionSet.
        auto ActionEntity = UCk_Utils_EntityLifetime_UE::Spawn_Entity(
            InActionSet,
            /* lifetime owner */ InActionSet,
            FGameplayTag::EmptyTag);

        // Label with the class-derived tag.
        const auto ActionTag = UCk_GoapAction_EntityScript::Get_ActionTagForClass(InActionClass);
        UCk_Utils_GameplayLabel_UE::Add(ActionEntity, ActionTag);

        // Stamp the Action params.
        auto Params = FFragment_Goap_Action_Params{};
        Params.Set_ActionClass(InActionClass);
        ActionEntity.AddOrGet<FFragment_Goap_Action_Params>(Params);

        // Stamp empty Current + Tree fragments (filled at Setup time / by tree edges).
        ActionEntity.AddOrGet<FFragment_Goap_Action_Current>();
        ActionEntity.AddOrGet<FFragment_Goap_Action_Tree>();
        ActionEntity.AddOrGet<FFragment_Goap_Action_Requests>();
        ActionEntity.AddOrGet<FFragment_Goap_Action_ReplanThrottle>();
        ActionEntity.AddOrGet<FTag_Goap_Action_RequiresSetup>();

        // Add to the ActionSet's record of actions.
        auto& Record = InActionSet.Get<FFragment_RecordOfGoapActions>();
        Record.AddEntry(InActionSet, FCk_Handle_Goap_Action{ActionEntity});

        return FCk_Handle_Goap_Action{ActionEntity};
    }
}
```

### Task U2.3 — Add AddAction_ToAction

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Utils.h`
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Utils.cpp`

- [ ] **Step 1: Declare the verb**

In `.h`:

```cpp
// Add a child Action under an existing parent Action. The parent's planner
// can now pick this child. Returns the child Action handle. If InActionClass
// is already registered in the parent ActionSet's catalog, returns the
// existing handle (and adds it as another parent edge — but the tree
// invariant in v1 is one parent per Action, so this is an error). Use
// Has_ChildAction(Parent, Class) to check beforehand if needed.
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Add Action (To Action)")
static FCk_Handle_Goap_Action AddAction_ToAction(
    UPARAM(ref) FCk_Handle_Goap_Action& InParentAction,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);
```

- [ ] **Step 2: Implement in .cpp**

```cpp
auto
    UCk_Utils_Goap_Action_UE::
    AddAction_ToAction(
        FCk_Handle_Goap_Action& InParentAction,
        TSubclassOf<UCk_GoapAction_EntityScript> InActionClass)
    -> FCk_Handle_Goap_Action
{
    CK_ENSURE_IF_NOT(ck::IsValid(InParentAction),
        TEXT("Invalid parent Action handle [{}].{}"), InParentAction, ck::Context(this))
    { return {}; }

    CK_ENSURE_IF_NOT(ck::IsValid(InActionClass),
        TEXT("Invalid action class in AddAction_ToAction.{}"), ck::Context(this))
    { return {}; }

    // Walk up the parent's owner chain to find the containing ActionSet.
    auto ActionSetHandle = ck::GetOwnerEntity(InParentAction).Cast<FCk_Handle_Goap_ActionSet>();
    CK_ENSURE_IF_NOT(ck::IsValid(ActionSetHandle),
        TEXT("Parent Action [{}] has no owning ActionSet.{}"), InParentAction, ck::Context(this))
    { return {}; }

    // Create / find the child entity in the ActionSet's catalog.
    auto ChildHandle = ck_ckgoap_actionset_internal::DoCreateOrFindActionEntity(
        ActionSetHandle, InActionClass);

    // Wire up tree edges.
    auto& ChildTree = ChildHandle.Get<FFragment_Goap_Action_Tree>();

    CK_ENSURE_IF_NOT(NOT ck::IsValid(ChildTree._ParentAction),
        TEXT("Action [{}] already has parent [{}]; cannot reparent (v1 enforces tree).{}"),
        ChildHandle, ChildTree._ParentAction, ck::Context(this))
    { return ChildHandle; }

    ChildTree._ParentAction = InParentAction;

    auto& ParentTree = InParentAction.Get<FFragment_Goap_Action_Tree>();
    ParentTree._ChildActions.AddUnique(ChildHandle);

    return ChildHandle;
}
```

### Task U2.4 — Delete the old AddAction(Tier, ...) verb

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Utils.h/cpp` (the carried-over Tier_Utils renamed in U1.3)

- [ ] **Step 1: Find the old AddAction signature**

In `.h`, locate:

```cpp
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Add Action")
static FCk_Handle_Goap_Action AddAction(
    UPARAM(ref) FCk_Handle_Goap_Action& InTier,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);
```

(After U1's renames, `InTier` is now typed `FCk_Handle_Goap_Action` but the semantics are the old "add action class to this tier's catalog".)

Delete this declaration and its definition. The new replacement is `AddAction_ToAction` from U2.3.

### Task U2.5 — Update Find_Action and Find_ActionByClass

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Utils.h/cpp`

- [ ] **Step 1: Add (or update) Find_Action**

In `.h`:

```cpp
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Find Action (By Tag)")
static FCk_Handle_Goap_Action Find_Action(
    const FCk_Handle_Goap_ActionSet& InActionSet,
    FGameplayTag InActionTag);

UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Find Action (By Class)")
static FCk_Handle_Goap_Action Find_ActionByClass(
    const FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InActionClass);
```

In `.cpp`:

```cpp
auto
    UCk_Utils_Goap_ActionSet_UE::
    Find_Action(
        const FCk_Handle_Goap_ActionSet& InActionSet,
        FGameplayTag InActionTag)
    -> FCk_Handle_Goap_Action
{
    if (NOT ck::IsValid(InActionSet)) { return {}; }
    if (NOT InActionTag.IsValid())    { return {}; }

    const auto& Record = InActionSet.Get<FFragment_RecordOfGoapActions>();
    for (const auto& ActionHandle : Record.Get_Entries())
    {
        const auto Label = UCk_Utils_GameplayLabel_UE::Get_Label(ActionHandle);
        if (Label == InActionTag) { return ActionHandle; }
    }
    return {};
}

auto
    UCk_Utils_Goap_ActionSet_UE::
    Find_ActionByClass(
        const FCk_Handle_Goap_ActionSet& InActionSet,
        TSubclassOf<UCk_GoapAction_EntityScript> InActionClass)
    -> FCk_Handle_Goap_Action
{
    if (NOT ck::IsValid(InActionSet))   { return {}; }
    if (NOT ck::IsValid(InActionClass)) { return {}; }

    const auto& Record = InActionSet.Get<FFragment_RecordOfGoapActions>();
    for (const auto& ActionHandle : Record.Get_Entries())
    {
        const auto& Params = ActionHandle.Get<FFragment_Goap_Action_Params>();
        if (Params.Get_ActionClass() == InActionClass) { return ActionHandle; }
    }
    return {};
}
```

### Task U2.6 — Add per-Action Request verbs

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Utils.h/cpp`

- [ ] **Step 1: Add all Request verbs declared in spec §3.3**

Per the spec, add:

```cpp
UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_Plan(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction);

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_CancelPlan(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction);

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

UFUNCTION(BlueprintCallable, ...)
static FCk_Handle_Goap_Action Request_SetWorldStateSource(
    UPARAM(ref) FCk_Handle_Goap_Action& InAction,
    UPARAM(ref) FCk_Handle_Goap_WorldState& InWorldStateSource);
```

Implementations all push a variant onto `FFragment_Goap_Action_Requests` and add `FTag_Goap_Action_PlanRequested` (or equivalent tag for the specific request). Mirror the existing per-Tier Request_* implementations that were carried into the renamed file.

### Task U2.7 — Add Request_SetRootAction + Request_ResetActiveChain on ActionSet

**Files:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Utils.h/cpp`

- [ ] **Step 1: Declare + implement**

In `.h`:

```cpp
UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Request Set Root Action")
static FCk_Handle_Goap_ActionSet Request_SetRootAction(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet,
    TSubclassOf<UCk_GoapAction_EntityScript> InNewRootClass);

UFUNCTION(BlueprintCallable, Category = "Ck|Utils|Goap",
    DisplayName = "[Ck][Goap] Request Reset Active Chain")
static FCk_Handle_Goap_ActionSet Request_ResetActiveChain(
    UPARAM(ref) FCk_Handle_Goap_ActionSet& InActionSet);
```

In `.cpp`: push a request variant onto `FFragment_Goap_ActionSet_Requests` (add this fragment if it doesn't already exist).

### Task U2.8 — Build and verify Phase U2

- [ ] **Step 1: Close editor; toolbox build**

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

Expected: green. If any call site still references `AddTier` or `AddAction(FCk_Handle_Goap_Action, ...)` (the old form), update it.

- [ ] **Step 2: Launch editor, regenerate AS bindings, verify post-compile clean, close editor**

- [ ] **Step 3: Re-run toolbox build**

Expected: green.

### Task U2.9 — Commit Phase U2

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git add -A
git commit -m "$(cat <<'EOF'
refactor(CkGoap): Phase U2 — new unified API surface

· AddActionSet (was AddBundle)
· SetRootAction(ActionSet, Class, InitialWS) replaces first AddTier call
· AddAction_ToActionSet(ActionSet, Class)
· AddAction_ToAction(ParentAction, Class) — new, builds the tree
· Find_Action / Find_ActionByClass
· Per-Action Request_* verbs (CancelPlan, SetReplanPolicy/Interval,
  SetSearchBudget/CostThreshold, SetChildActionCost, SetWorldStateSource)
· ActionSet Request_SetRootAction + Request_ResetActiveChain

Removed: AddTier, AddAction(Tier, ...). AS bindings regenerated.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"

cd D:/Repos/CkPlugins
git add Plugins/CkFoundation
git commit -m "$(cat <<'EOF'
chore(submodule): bump CkFoundation (U2 unified API)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

**End of Phase U2.**

---

## Phase U3 — Processors against the unified model

**Goal:** Rewrite the chain-update logic so `Plan[0]` is a child handle directly (no tag matching). Per-Action HandleResult maps A* path elements to child handles. Setup populates `_ActionDef` per Action from the CDO.

### Task U3.1 — Rewrite ActionSet ChainUpdate processor

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Processor.cpp`

- [ ] **Step 1: Replace the ForEachEntity body with the §4.2 pseudocode (translated to C++)**

Locate `FProcessor_Goap_ActionSet_ChainUpdate::ForEachEntity` and replace its body:

```cpp
auto
    FProcessor_Goap_ActionSet_ChainUpdate::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InActionSet,
        const FFragment_Goap_ActionSet_Params& InParams,
        FFragment_Goap_ActionSet_Current& InCurrent,
        FFragment_Goap_ActionSet_ActiveChain& InActiveChain) const
    -> void
{
    if (InCurrent.Get_EnableToggle() == ECk_EnableDisable::Disable) { return; }

    auto& Chain = InActiveChain._Chain;
    if (Chain.IsEmpty()) { return; }

    const auto OldChainSnapshot = Chain;
    auto bChainChanged = false;

    auto i = 0;
    while (i < Chain.Num())
    {
        const auto CurrAction = Chain[i];
        const auto& CurrCurrent = CurrAction.Get<FFragment_Goap_Action_Current>();

        if (CurrCurrent.Get_PlanStatus() != ECk_GoapPlanStatus::PlanFound)
        {
            return;   // mid-decision; don't mutate past this point
        }

        const auto& Plan = CurrCurrent.Get_Plan();
        if (Plan.IsEmpty())
        {
            // Goal satisfied / no actions chosen — truncate everything past.
            if (i + 1 < Chain.Num())
            {
                DoTruncateChainFrom(InActionSet, Chain, i + 1);
                bChainChanged = true;
            }
            break;
        }

        const auto NextChild = Plan[0];

        if (i + 1 < Chain.Num())
        {
            const auto ExistingChild = Chain[i + 1];
            if (ExistingChild == NextChild)
            {
                ++i;
                continue;
            }

            DoTruncateChainFrom(InActionSet, Chain, i + 1);
            bChainChanged = true;
        }

        // Is the chosen child composite (has its own children)?
        const auto& ChildTree = NextChild.Get<FFragment_Goap_Action_Tree>();
        if (ChildTree.Get_ChildActions().IsEmpty())
        {
            // Atomic — terminate here.
            break;
        }

        // Composite — extend the chain.
        Chain.Add(NextChild);
        bChainChanged = true;

        auto& ChildCurrent = NextChild.Get<FFragment_Goap_Action_Current>();
        ChildCurrent._ActiveParent = CurrAction;
        ChildCurrent._Goal = NextChild.Get<FFragment_Goap_Action_Definition>().Get_GoalFromEffects();

        DoResolveAndAssignWorldStateSource(NextChild, CurrAction, InActionSet);
        DoSubscribeActionToWorldState(NextChild);

        NextChild.template AddOrGet<FTag_Goap_Action_RequiresInitialPlan>();

        UUtils_Signal_Goap_OnActionActivated::Broadcast(NextChild, FCk_Goap_Payload_OnActionActivated{NextChild});

        break;
    }

    if (bChainChanged)
    {
        UUtils_Signal_Goap_OnActiveChainChanged::Broadcast(
            InActionSet,
            FCk_Goap_Payload_OnActiveChainChanged{InActionSet, OldChainSnapshot});
    }
}
```

- [ ] **Step 2: Implement DoTruncateChainFrom + DoResolveAndAssignWorldStateSource + DoSubscribeActionToWorldState**

These are static/private helpers in the same .cpp:

```cpp
namespace ck_ckgoap_actionset_chainupdate_internal
{
    auto DoTruncateChainFrom(
        FCk_Handle_Goap_ActionSet& InActionSet,
        TArray<FCk_Handle_Goap_Action>& InChain,
        int32 InStartIndex)
        -> void
    {
        for (int32 i = InChain.Num() - 1; i >= InStartIndex; --i)
        {
            auto Action = InChain[i];

            // Unsubscribe from WS.
            DoUnsubscribeActionFromWorldState(Action);

            auto& Current = Action.Get<FFragment_Goap_Action_Current>();
            Current._Goal = {};
            Current._InvalidGoal = {};
            Current._ActiveParent = FCk_Handle_Goap_Action{};
            Current._WorldStateSource_Resolved = {};
            Current._Plan.Reset();
            Current._PlanStatus = ECk_GoapPlanStatus::Idle;

            UUtils_Signal_Goap_OnActionDeactivated::Broadcast(
                Action, FCk_Goap_Payload_OnActionDeactivated{Action});

            InChain.RemoveAt(i);
        }
    }

    auto DoResolveAndAssignWorldStateSource(
        FCk_Handle_Goap_Action& InChild,
        const FCk_Handle_Goap_Action& InParent,
        const FCk_Handle_Goap_ActionSet& InActionSet)
        -> void
    {
        auto& ChildCurrent = InChild.Get<FFragment_Goap_Action_Current>();
        const auto& ChildParams = InChild.Get<FFragment_Goap_Action_Params>();

        const auto& Override = ChildParams.Get_WorldStateSource_Override();
        if (ck::IsValid(Override))
        {
            ChildCurrent._WorldStateSource_Resolved = Override;
            return;
        }

        const auto& ParentCurrent = InParent.Get<FFragment_Goap_Action_Current>();
        if (ck::IsValid(ParentCurrent.Get_WorldStateSource_Resolved()))
        {
            ChildCurrent._WorldStateSource_Resolved = ParentCurrent.Get_WorldStateSource_Resolved();
            return;
        }

        const auto& SetWS = InActionSet.Get<FFragment_Goap_ActionSet_WorldStateSource>();
        ChildCurrent._WorldStateSource_Resolved = SetWS.Get_WorldStateSource();
    }

    auto DoSubscribeActionToWorldState(FCk_Handle_Goap_Action& InAction) -> void
    {
        auto& Current = InAction.Get<FFragment_Goap_Action_Current>();
        auto& WS = Current._WorldStateSource_Resolved;
        if (NOT ck::IsValid(WS)) { return; }

        auto& Subscribers = WS.template Get<FFragment_Goap_WorldState_Subscribers>();
        Subscribers.AddUnique(FCk_Handle{InAction});
    }

    auto DoUnsubscribeActionFromWorldState(FCk_Handle_Goap_Action& InAction) -> void
    {
        auto& Current = InAction.Get<FFragment_Goap_Action_Current>();
        auto& WS = Current._WorldStateSource_Resolved;
        if (NOT ck::IsValid(WS)) { return; }

        auto& Subscribers = WS.template Get<FFragment_Goap_WorldState_Subscribers>();
        Subscribers.Remove(FCk_Handle{InAction});
    }
}
```

- [ ] **Step 3: Delete the stale `TierCatalogIndex` references**

The FFragment_Goap_ActionSet_TierCatalogIndex fragment was kept compiling but unused in U1.12. Now delete:

In `CkGoap_ActionSet_Fragment.h`: remove `FFragment_Goap_ActionSet_TierCatalogIndex` struct definition.

In `CkGoap_ActionSet_Utils.cpp` / `CkGoap_ActionSet_Processor.cpp`: remove any remaining references to the catalog index.

### Task U3.2 — Rewrite per-Action HandleResult

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.cpp`

The old HandleResult wrote `_Plan` as `TArray<TSubclassOf<...>>`. The new version writes `TArray<FCk_Handle_Goap_Action>` — child handles in plan execution order.

- [ ] **Step 1: Replace the plan-write block**

Find the section that does (roughly):

```cpp
auto Plan = TArray<TSubclassOf<UCk_GoapAction_EntityScript>>{};
for (const auto& ActionDef : SearchResult.GetPath())
{
    Plan.Add(ActionDef.Get_ActionClass());
}
InCurrent._Plan = MoveTemp(Plan);
```

Replace with:

```cpp
// Map each ActionDef in the A* path back to its child Action handle.
// The parent's _ChildActions list is the canonical source — its order
// matches the order CDOs were extracted at Setup time.
auto Plan = TArray<FCk_Handle_Goap_Action>{};
Plan.Reserve(SearchResult.GetPath().Num());

const auto& ParentTree = InAction.Get<FFragment_Goap_Action_Tree>();
const auto& ChildHandles = ParentTree.Get_ChildActions();

for (const auto& ActionDef : SearchResult.GetPath())
{
    auto* ChildHandle = ChildHandles.FindByPredicate(
        [&](const FCk_Handle_Goap_Action& InCandidate)
        {
            const auto& CandidateParams = InCandidate.Get<FFragment_Goap_Action_Params>();
            return CandidateParams.Get_ActionClass() == ActionDef.Get_ActionClass();
        });

    CK_ENSURE_IF_NOT(ChildHandle != nullptr,
        TEXT("A* path contains ActionDef whose class [{}] is not in parent's child set.{}"),
        ActionDef.Get_ActionClass(), ck::Context(this))
    { continue; }

    Plan.Add(*ChildHandle);
}

InCurrent._Plan = MoveTemp(Plan);
```

### Task U3.3 — Update Setup to extract ActionDefs from this Action's children

The Setup processor used to scan a tier's `_ActionClasses` and produce `_Actions`. Now each Action's "available children for planning" are its `_ChildActions` from the tree fragment.

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.cpp`

- [ ] **Step 1: Update FProcessor_Goap_Action_Setup**

Find the Setup body. The CDO-extraction loop currently iterates `_ActionClasses`. Update to iterate `_ChildActions`:

```cpp
auto
    FProcessor_Goap_Action_Setup::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InAction,
        FFragment_Goap_Action_Params& InParams,
        FFragment_Goap_Action_Definition& InDefinition,
        FFragment_Goap_Action_Tree& InTree,
        FTag_Goap_Action_RequiresSetup) const
    -> void
{
    // Extract this Action's own def from the CDO (preconditions, effects, cost).
    const auto* SelfCDO = InParams.Get_ActionClass().GetDefaultObject();
    if (ck::IsValid(SelfCDO))
    {
        SelfCDO->DefineAction();   // populate _Preconditions, _Effects, _Cost on CDO

        // The Setup processor is already a friend of UCk_GoapAction_EntityScript
        // (declared on the class), so direct access to the underscore-prefixed
        // private fields works. If you're adding new processors that need this
        // access, friend them in CkGoapAction_EntityScript.h.
        InDefinition._Preconditions = SelfCDO->_Preconditions;
        InDefinition._Effects       = SelfCDO->_Effects;
        InDefinition._Cost          = SelfCDO->_Cost;

        // Pre-build the cached ActionDef for planner candidates.
        InDefinition._CachedActionDef.ActionClass    = InParams.Get_ActionClass();
        InDefinition._CachedActionDef.Preconditions  = InDefinition._Preconditions;
        InDefinition._CachedActionDef.Effects        = InDefinition._Effects;
        InDefinition._CachedActionDef.Cost           = InDefinition._Cost;
    }

    // The Action's _Goal (when active as a planner) is its own _Effects.
    // Compute the keyed form ahead of time so chain-update doesn't have to
    // re-resolve at activation.
    InDefinition._GoalFromEffects = ResolveEffectsToGoal(
        InDefinition._Effects,
        InParams.Get_WorldStateSource_Override()    // may be invalid; resolver falls back
    );

    // Extract each child Action's def — only needed because Action's planner
    // operates over child ActionDefs.
    InDefinition._ChildActionDefs.Reset();
    for (const auto& ChildHandle : InTree.Get_ChildActions())
    {
        const auto& ChildParams = ChildHandle.template Get<FFragment_Goap_Action_Params>();
        const auto& ChildDef    = ChildHandle.template Get<FFragment_Goap_Action_Definition>();
        InDefinition._ChildActionDefs.Add(ChildDef.AsActionDef());   // helper
    }

    InAction.template Remove<FTag_Goap_Action_RequiresSetup>();
}
```

(Field names `_GoalFromEffects` and `_ChildActionDefs` need to be added to `FFragment_Goap_Action_Definition`. Add them as private fields with `CK_PROPERTY_GET`.)

### Task U3.4 — Order processors correctly

- [ ] **Step 1: Verify CK_REGISTER_PROCESSOR ordering in CkGoap_Module.cpp**

Order in `FGroup_Gameplay_AI`:

```
FProcessor_Goap_Action_Setup
FProcessor_Goap_Action_AutoReplan
FProcessor_Goap_Action_HandleRequests
TProcessor_AStar_Execute<...>
FProcessor_Goap_Action_HandleResult
FProcessor_Goap_ActionSet_ChainUpdate
```

Edit `CkGoap_Module.cpp` to ensure that exact registration order. If any processor is missing a `CK_REGISTER_PROCESSOR` line, add it.

### Task U3.5 — Build and verify

- [ ] **Step 1: Toolbox build**

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

Expected: green.

### Task U3.6 — Commit Phase U3

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git add -A
git commit -m "$(cat <<'EOF'
refactor(CkGoap): Phase U3 — processors against unified model

· FProcessor_Goap_ActionSet_ChainUpdate — no tag matching; Plan[0] is a
  child Action handle directly. Atomic vs composite read from _ChildActions.
· FProcessor_Goap_Action_HandleResult — map A* path → child Action handles
  via parent's _ChildActions list (class equality).
· FProcessor_Goap_Action_Setup — extract own def from CDO; collect child
  ActionDefs for planner input.
· Deleted FFragment_Goap_ActionSet_TierCatalogIndex (no matching layer).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"

cd D:/Repos/CkPlugins
git add Plugins/CkFoundation
git commit -m "chore(submodule): bump CkFoundation (U3 processors)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

**End of Phase U3.**

---

## Phase U4 — Subscriber + dirty plumbing

**Goal:** WS subscriber list works with Action handles end-to-end. AutoReplan consumes per-Action `FTag_Goap_Dirty_WorldState`. Mostly already correct after the renames in U0/U1 — this phase is verification + small fixes.

### Task U4.1 — Verify subscriber list type

- [ ] **Step 1: Check FFragment_Goap_WorldState_Subscribers**

```bash
cd D:/Repos/CkPlugins
```

Grep: pattern `_Subscribers`, path `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/WorldState`, output_mode `content`, context `2`.

Expected: subscriber list typed `TArray<FCk_Handle>` (generic). If still typed as a tier-specific handle, update.

### Task U4.2 — Verify Subscribe/Unsubscribe call sites use Action handles

Already done in U3.1's `DoSubscribe/UnsubscribeActionFromWorldState`. Sanity-check no Tier-typed call sites remain.

- [ ] **Step 1: Grep for any remaining tier-typed subscribe calls**

Grep: pattern `Subscribe.*Tier|Tier.*Subscribe`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `content`.

Expected: zero matches (everything should be `SubscribeActionToWorldState` now). Fix any stragglers.

### Task U4.3 — AutoReplan consumes Action-tagged dirty marker

- [ ] **Step 1: Verify FProcessor_Goap_Action_AutoReplan reads FTag_Goap_Dirty_WorldState per Action**

Open `CkGoap_Action_Processor.cpp`. The AutoReplan ForEachEntity should include `FTag_Goap_Dirty_WorldState` in its required-fragments template arg list and remove the tag after enqueuing the replan request.

If the implementation still references `Tier` in any way, update.

### Task U4.4 — Build, commit

- [ ] **Step 1: Toolbox build (green)**

- [ ] **Step 2: Commit**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git add -A
git commit -m "refactor(CkGoap): Phase U4 — subscriber + dirty plumbing for Action handles

Verified WorldState subscriber list end-to-end with Action handles.
AutoReplan reads per-Action FTag_Goap_Dirty_WorldState.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"

cd D:/Repos/CkPlugins
git add Plugins/CkFoundation
git commit -m "chore(submodule): bump CkFoundation (U4 subscriber plumbing)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

**End of Phase U4.**

---

## Phase U5 — Diagnostics

### Task U5.1 — _InvalidGoal at Setup

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/Action/CkGoap_Action_Processor.cpp`

- [ ] **Step 1: In FProcessor_Goap_Action_Setup, after _Effects extraction, validate against the resolved WS**

```cpp
// _InvalidGoal: any effect key not registered in this Action's resolved WS.
InDefinition._InvalidGoal.Reset();

const auto& WS = InAction.Get<FFragment_Goap_Action_Current>().Get_WorldStateSource_Resolved();
if (ck::IsValid(WS))
{
    const auto& Registry = WS.Get<FFragment_Goap_WorldState_KeyRegistry>().Get_Registry();
    for (const auto& Effect : InDefinition._Effects)
    {
        const auto Key = Registry.Find(Effect.Get_RawTag());
        if (Key == goap::InvalidGoapKey)
        {
            InDefinition._InvalidGoal.Add(
                FCk_GoapWS_Condition_Authored{Effect.Get_RawTag(), Effect.Get_Value()});
        }
    }

    if (InDefinition._InvalidGoal.Num() > 0)
    {
        ck::goap::Verbose(
            TEXT("Action [{}] has [{}] effect keys not in resolved WS."),
            InAction, InDefinition._InvalidGoal.Num());
    }
}
```

### Task U5.2 — _DependencyCycles via Tarjan SCC

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/Public/CkGoap/ActionSet/CkGoap_ActionSet_Processor.cpp`

- [ ] **Step 1: Add FProcessor_Goap_ActionSet_Setup (if it doesn't exist)**

```cpp
class CKGOAP_API FProcessor_Goap_ActionSet_Setup : public ck_exp::TProcessor<
    FProcessor_Goap_ActionSet_Setup,
    FCk_Handle_Goap_ActionSet,
    FFragment_Goap_ActionSet_Current,
    FFragment_RecordOfGoapActions,
    FTag_Goap_ActionSet_RequiresSetup,
    CK_IGNORE_PENDING_KILL>
{ ... };
```

- [ ] **Step 2: In ForEachEntity, run Tarjan SCC on the _ChildActions graph**

```cpp
auto
    FProcessor_Goap_ActionSet_Setup::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InActionSet,
        FFragment_Goap_ActionSet_Current& InCurrent,
        const FFragment_RecordOfGoapActions& InRecord,
        FTag_Goap_ActionSet_RequiresSetup) const
    -> void
{
    // Build adjacency list keyed by Action handle.
    using ActionHandle = FCk_Handle_Goap_Action;
    auto Adj = TMap<ActionHandle, TArray<ActionHandle>>{};
    for (const auto& Action : InRecord.Get_Entries())
    {
        const auto& Tree = Action.template Get<FFragment_Goap_Action_Tree>();
        Adj.Add(Action, Tree.Get_ChildActions());
    }

    // Tarjan SCC.
    auto Cycles = TarjanScc(Adj);

    // Filter out trivial SCCs (single nodes with no self-loop).
    InCurrent._DependencyCycles.Reset();
    for (const auto& Scc : Cycles)
    {
        const auto IsTrivial = (Scc.Num() == 1) && (NOT Adj[Scc[0]].Contains(Scc[0]));
        if (IsTrivial) { continue; }

        auto CycleStrings = TArray<FString>{};
        for (const auto& Action : Scc)
        {
            const auto Label = UCk_Utils_GameplayLabel_UE::Get_Label(Action);
            CycleStrings.Add(Label.ToString());
        }
        InCurrent._DependencyCycles.Add(FString::Join(CycleStrings, TEXT(" → ")));
    }

    if (InCurrent._DependencyCycles.Num() > 0)
    {
        ck::goap::Warning(
            TEXT("ActionSet [{}] has [{}] dependency cycles. First: [{}]"),
            InActionSet, InCurrent._DependencyCycles.Num(),
            InCurrent._DependencyCycles[0]);
    }

    InActionSet.template Remove<FTag_Goap_ActionSet_RequiresSetup>();
}
```

`TarjanScc(Adj)` is a standard algorithm. Implement it iteratively (avoiding recursion to dodge Unreal stack-depth concerns for deep trees) as a free function in an anon namespace in the .cpp:

```cpp
namespace ck_ckgoap_actionset_setup_internal
{
    template<typename HandleT>
    auto TarjanScc(const TMap<HandleT, TArray<HandleT>>& InAdj)
        -> TArray<TArray<HandleT>>
    {
        // Tarjan SCC, iterative. References:
        //   https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
        //   https://www.geeksforgeeks.org/tarjans-algorithm-for-strongly-connected-components/
        //
        // The recursive form on Wikipedia translates to an explicit call stack
        // here. Each entry on the work stack represents a node's DFS frame —
        // (NodeHandle, ChildIndex). On entry, we initialise the node and push
        // it to the stack. On each child step, we either recurse (push the
        // child's frame) or merge the lowlink. When ChildIndex == children.Num()
        // we pop the frame and finalise the SCC if this is a root.

        TArray<TArray<HandleT>> Result;
        TMap<HandleT, int32> Index;
        TMap<HandleT, int32> Lowlink;
        TSet<HandleT> OnStack;
        TArray<HandleT> Stack;
        int32 NextIndex = 0;

        struct FFrame { HandleT Node; int32 ChildIdx; };
        TArray<FFrame> WorkStack;

        for (const auto& Pair : InAdj)
        {
            const auto& Root = Pair.Key;
            if (Index.Contains(Root)) { continue; }

            WorkStack.Push({Root, 0});
            Index.Add(Root, NextIndex);
            Lowlink.Add(Root, NextIndex);
            ++NextIndex;
            Stack.Push(Root);
            OnStack.Add(Root);

            while (NOT WorkStack.IsEmpty())
            {
                auto& Frame = WorkStack.Top();
                const auto& Children = InAdj[Frame.Node];

                if (Frame.ChildIdx < Children.Num())
                {
                    const auto Child = Children[Frame.ChildIdx];
                    ++Frame.ChildIdx;
                    if (NOT Index.Contains(Child))
                    {
                        Index.Add(Child, NextIndex);
                        Lowlink.Add(Child, NextIndex);
                        ++NextIndex;
                        Stack.Push(Child);
                        OnStack.Add(Child);
                        WorkStack.Push({Child, 0});
                    }
                    else if (OnStack.Contains(Child))
                    {
                        Lowlink[Frame.Node] = FMath::Min(Lowlink[Frame.Node], Index[Child]);
                    }
                    continue;
                }

                // All children processed — finalise.
                const auto NodeHandle = Frame.Node;
                if (Lowlink[NodeHandle] == Index[NodeHandle])
                {
                    TArray<HandleT> Scc;
                    while (true)
                    {
                        const auto Popped = Stack.Pop();
                        OnStack.Remove(Popped);
                        Scc.Add(Popped);
                        if (Popped == NodeHandle) { break; }
                    }
                    Result.Add(MoveTemp(Scc));
                }
                WorkStack.Pop();

                if (NOT WorkStack.IsEmpty())
                {
                    auto& ParentFrame = WorkStack.Top();
                    Lowlink[ParentFrame.Node] = FMath::Min(Lowlink[ParentFrame.Node], Lowlink[NodeHandle]);
                }
            }
        }
        return Result;
    }
}
```

### Task U5.3 — AddAction_* setup-time duplicate-class warning

Both `AddAction_ToActionSet` and `AddAction_ToAction` already check via `Find_ActionByClass` (Task U2.2). Verify:

- [ ] **Step 1: Grep**

Pattern: `already registered`, path `Plugins/CkFoundation/Source/CkGoap`, output_mode `content`.

Expected: matches in `DoCreateOrFindActionEntity`.

### Task U5.4 — Build, commit

- [ ] **Step 1: Toolbox build (green)**

- [ ] **Step 2: Commit**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git add -A
git commit -m "refactor(CkGoap): Phase U5 — diagnostics

· _InvalidGoal populated at Setup (effect keys not in resolved WS).
· _DependencyCycles populated via Tarjan SCC over _ChildActions.
· Setup-time duplicate ActionClass warning in AddAction_*.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"

cd D:/Repos/CkPlugins
git add Plugins/CkFoundation
git commit -m "chore(submodule): bump CkFoundation (U5 diagnostics)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

**End of Phase U5.**

---

## Phase U6 — Smoke test rewrite

**Goal:** `CkAutoTest_Goap_ActionSet_RootOnly.as` exercises the new unified API and passes end-to-end.

### Task U6.1 — Rewrite the smoke test

**File:**
- Modify: `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_ActionSet_RootOnly.as`

- [ ] **Step 1: Replace the file body with the new test shape**

Replace the existing class body with:

```angelscript
// CkAutoTest_Goap_ActionSet_RootOnly — smoke test for the unified ActionSet/Action model.
// Validates: root-only ActionSet with a single goal-satisfying action plans and fires OnPlanComplete.

class Ck_AutoTest_Goap_ActionSet_RootOnly : Ck_AutoTest_EntityScript
{
    default _TestName = n"Ck_AutoTest_Goap_ActionSet_RootOnly";

    UFUNCTION()
    void DoConstruct() override
    {
        auto Owner = GetSelfHandle();

        auto Goap = utils_goap::Add(Owner, FCk_Fragment_Goap_RootParamsData{});
        if (NOT utils_handle::IsValid(Goap))
        {
            ck::Error(f"Failed to add Goap root to {Owner}");
            FinishFailure(n"Failed to add Goap root");
            return;
        }

        auto WS = utils_goap_world_state::Add(Owner, FCk_Fragment_Goap_WorldStateParamsData{});
        utils_goap_world_state::Request_Set_Value(WS, n"Goap.Tier.CustomerServed", false);

        auto ActionSetParams = FCk_Fragment_Goap_ActionSetParamsData{};
        ActionSetParams.Set_ActionSetTag(n"Goap.ActionSet.Test");
        auto ActionSet = utils_goap_action_set::AddActionSet(Goap, ActionSetParams);

        auto RootAction = utils_goap_action_set::SetRootAction(
            ActionSet,
            Ck_AutoTestAction_Goap_ActionSet_Simple::StaticClass(),
            WS);

        utils_goap_action::BindTo_OnPlanComplete(RootAction, FCk_Delegate_Goap_OnPlanComplete(this, n"OnPlanComplete"));
        utils_goap_action::BindTo_OnPlanFailed  (RootAction, FCk_Delegate_Goap_OnPlanFailed  (this, n"OnPlanFailed"));
    }

    UFUNCTION()
    void OnPlanComplete(
        FCk_Handle_Goap_Action InAction,
        TArray<FCk_Handle_Goap_Action> InPlan,
        float32 InCost)
    {
        ck::Display(f"OnPlanComplete: tier {InAction.AsHandle()} produced plan of size {InPlan.Num()} cost {InCost}");
        FinishSuccess();
    }

    UFUNCTION()
    void OnPlanFailed(FCk_Handle_Goap_Action InAction)
    {
        ck::Error(f"OnPlanFailed on {InAction.AsHandle()}");
        FinishFailure(n"OnPlanFailed");
    }
}
```

### Task U6.2 — Strip SetActionTag from the helper action

**File:**
- Modify: `Plugins/CkTests/Script/CkGoap/CkAutoTestAction_Goap_ActionSet_Simple.as`

- [ ] **Step 1: Delete the SetActionTag(...) call from DefineAction**

The current body has something like:

```angelscript
class Ck_AutoTestAction_Goap_ActionSet_Simple : UCk_GoapAction_EntityScript
{
    UFUNCTION()
    void DoDefineAction() override
    {
        AddEffect(n"Goap.Tier.CustomerServed", true);
        SetCost(1.0);
        SetActionTag(n"Goap.Tier.CustomerServed");   // <- delete this line
    }
}
```

After removing:

```angelscript
class Ck_AutoTestAction_Goap_ActionSet_Simple : UCk_GoapAction_EntityScript
{
    UFUNCTION()
    void DoDefineAction() override
    {
        AddEffect(n"Goap.Tier.CustomerServed", true);
        SetCost(1.0);
    }
}
```

### Task U6.3 — Run the smoke test

- [ ] **Step 1: Toolbox build + test**

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --test --test-pattern Goap_ActionSet_RootOnly `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

Expected: 1 test runs, passes.

- [ ] **Step 2: If failing, inspect the log tail**

```powershell
Get-Content Saved/Logs/BuildTest.log -Tail 60
```

Likely failure modes:
- WS keys not registered: ensure the goal key `Goap.Tier.CustomerServed` is also stamped onto the WS via `Request_Set_Value` BEFORE `SetRootAction`.
- Plan never completes: check `_PlanStatus` via diagnostic logs; verify Setup processor ran and `_GoalFromEffects` populated.

### Task U6.4 — Commit Phase U6

```bash
cd D:/Repos/CkPlugins/Plugins/CkTests
git add Script/CkGoap/CkAutoTest_Goap_ActionSet_RootOnly.as Script/CkGoap/CkAutoTestAction_Goap_ActionSet_Simple.as
git commit -m "test(CkGoap): Phase U6 — smoke test rewritten against unified API

Ck_AutoTest_Goap_ActionSet_RootOnly passes end-to-end:
· AddActionSet + SetRootAction with InitialWS
· Helper action drops SetActionTag (class-derived identity now)
· OnPlanComplete fires → FinishSuccess

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"

cd D:/Repos/CkPlugins
git add Plugins/CkTests
git commit -m "chore(submodule): bump CkTests (U6 smoke test rewrite)

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

**End of Phase U6.**

---

## Phase U7 — Remaining 13 tests

Write each of the 13 tests listed in the unification spec §9, table rows 2–14. Each test gets its own `.as` file plus any supporting action subclass files.

For each test:

1. Create the test file `Plugins/CkTests/Script/CkGoap/CkAutoTest_Goap_ActionSet_<Name>.as` using `CkAutoTest_Goap_ActionSet_RootOnly.as` as the template.
2. Create any supporting action subclass files in the same directory using `CkAutoTestAction_Goap_ActionSet_Simple.as` as the template.
3. Run via `--test-pattern Goap_ActionSet_<Name>`.
4. Commit per test (one commit per test, or batches of 2–3 closely related tests).
5. Bump CkTests pointer in CkPlugins root.

The 13 tests in spec order — write them in approximately easiest-first order:

| # | Test name | Supporting action classes needed |
|---|---|---|
| 2 | `CkAutoTest_Goap_ActionSet_ChainGrowth.as` | Parent composite + child composite; child has its own atomic action |
| 3 | `CkAutoTest_Goap_ActionSet_ChainTruncation.as` | Setup so the WS dirties mid-life and parent's Plan[0] flips |
| 4 | `CkAutoTest_Goap_ActionSet_AtomicLeaf.as` | Parent composite + atomic child only (no grandchildren) |
| 5 | `CkAutoTest_Goap_ActionSet_GoalIsEffects.as` | Validate child._Goal == child.declared _Effects after activation |
| 6 | `CkAutoTest_Goap_ActionSet_WSInheritance.as` | Child with no override inherits parent's resolved WS |
| 7 | `CkAutoTest_Goap_ActionSet_WSOverride.as` | Child with Request_SetWorldStateSource has different resolved WS |
| 8 | `CkAutoTest_Goap_ActionSet_InvalidGoal.as` | Action's effects reference unknown WS key → `_InvalidGoal` populated |
| 9 | `CkAutoTest_Goap_ActionSet_DirtyPropagation.as` | Set WS value; subscribed Actions replan; non-subscribed don't |
| 10 | `CkAutoTest_Goap_ActionSet_MultiActionSet.as` | Two ActionSets on one entity tick independently |
| 11 | `CkAutoTest_Goap_ActionSet_Toggle.as` | Request_SetEnableToggle Disable → no planning, Enable → resumes |
| 12 | `CkAutoTest_Goap_ActionSet_OwnerCascadeDestroy.as` | Destroy owner; verify no leaks/crashes; child Actions cleaned up |
| 13 | `CkAutoTest_Goap_ActionSet_DeferOneFrame.as` | Newly-appended Action plans on frame+1, NOT activation frame |
| 14 | `CkAutoTest_Goap_ActionSet_ResetChain.as` | Request_ResetActiveChain collapses chain; OnActionDeactivated fires per removed Action |

Each test's pattern: build the structure, bind a signal, kick off the scenario, assert in the handler, FinishSuccess or FinishFailure.

After all 13 are green, run the full pattern in one go:

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --test --test-pattern Goap_ActionSet `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

Expected: 14 tests run (the smoke test from U6 + the 13 from U7), all pass.

**End of Phase U7.**

---

## Phase U8 — Polish + docs

### Task U8.1 — Update CkGoap CLAUDE.md

**File:**
- Modify: `Plugins/CkFoundation/Source/CkGoap/CLAUDE.md`

- [ ] **Step 1: Rewrite the overview**

The current file documents the pre-Bundle/Tier model (with `Add` and `Create` paradigms). Replace with an overview reflecting ActionSet/Action:

- Purpose: GOAP planner built on time-sliced A* + a tree-structured Action catalog
- Concepts: GoapRoot → ActionSet → Action tree (parent/children)
- Authoring: subclass `UCk_GoapAction_EntityScript`, override `DefineAction`, declare preconditions/effects/cost
- Registration: `AddActionSet` + `SetRootAction` + `AddAction_ToActionSet` / `AddAction_ToAction`
- Chain extension: composite Action's Plan[0] becomes child in chain; atomic Action terminates chain
- Replan policy + per-Action signals + diagnostics
- Anti-patterns (carry forward from current file): numeric WS via float/int tags, calling utility verbs from wrong thread, etc.

Keep the same section structure (Add vs Create, Architecture, Public API, Replan policy, Fragment table, Anti-patterns, See also) but updated content.

### Task U8.2 — Update root CLAUDE.md if it mentions CkGoap

- [ ] **Step 1: Check root reference**

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
grep -n "CkGoap" CLAUDE.md
```

Update any quick-reference line ("Goal-oriented action planning — see CkGoap/") to reflect the new ActionSet/Action model if it goes beyond a one-line description.

### Task U8.3 — Mark old Bundle/Tier spec as superseded

**File:**
- Modify: `D:/Repos/CkPlugins/docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md`

- [ ] **Step 1: Add a banner at the top**

Insert just after the line `# CkGoap Bundle/Tier Refactor — Design Spec`:

```markdown
> **⚠ SUPERSEDED** by [2026-05-19-CkGoap-ActionSetUnification-design.md](2026-05-19-CkGoap-ActionSetUnification-design.md).
> This Bundle/Tier model was implemented (Phases 0–6) then unified into a single ActionSet/Action model. The implementation here is preserved for history; the canonical design is the ActionSetUnification spec.
```

### Task U8.4 — Final build-test sweep

- [ ] **Step 1: Full build + all tests**

```powershell
./CkAuto/UnrealToolbox.exe `
    --build --config=DebugGame --target=Editor `
    --test --test-pattern Goap_ActionSet `
    --output=Saved/Logs/BuildTest.log `
    --project="D:\Repos\CkPlugins"
```

Expected: all 14 ActionSet tests pass.

### Task U8.5 — Commit Phase U8

```bash
cd D:/Repos/CkPlugins/Plugins/CkFoundation
git add Source/CkGoap/CLAUDE.md
git add -A   # in case any other doc tweaks
git commit -m "docs(CkGoap): Phase U8 — overview for ActionSet/Action model

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"

cd D:/Repos/CkPlugins
git add docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md
git add Plugins/CkFoundation
git commit -m "$(cat <<'EOF'
docs(CkGoap): mark Bundle/Tier spec superseded; bump CkFoundation (U8 docs)

· Banner added to old Bundle/Tier spec pointing at ActionSetUnification-design.md
· CkFoundation/Source/CkGoap/CLAUDE.md rewritten for unified model

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

**End of Phase U8 — unification complete.**

---

## Resuming debugger work after U8

After U8 lands, the debugger brainstorm picks back up where it was paused (mid-§4 of the design walkthrough). All decisions already made (Layout B refined, history per ActionSet, gateway Inspector, etc.) remain valid; only terminology shifts:

- `Bundle` → `ActionSet`
- `Tier` → `Action` (chain links)
- "active tier chain" → "active chain" (still an ordered list of Action handles)
- Mockup F v2 needs a refresh pass: rename UI labels, update sidebar tree (now Actions instead of Tiers), drop the `_ActionTag` UI block (no more matching layer), keep the class-derived identity tag for debug display

The debugger spec proper (`docs/superpowers/specs/YYYY-MM-DD-CkGoapDebugger_ActionSet-design.md`) is written after these decisions are re-confirmed.

---

## Risk register

| Risk | Mitigation |
|---|---|
| Build-test takes 5–10 min per iteration; small mistakes are expensive | Use `--build --target=Editor` (no `--test`) for in-phase iteration; only run `--test` at phase end |
| AS hot-reload failure on field-shape changes mid-phase | Toolbox detects + aborts in ~12s (per memory) — investigate the AS error in BuildTest.log tail |
| Editor open in another Claude session | Poll `Saved/Logs/CkPlugins.log` exclusive lock; wait until free |
| `Find_ActionByClass` linear scan on large catalogs | Acceptable for v1; revisit if profile shows hot-path |
| `Get_OwnerEntity().Cast<FCk_Handle_Goap_ActionSet>` in `AddAction_ToAction` assumes the parent's owner IS the ActionSet | True by construction (DoCreateOrFindActionEntity sets the parent as record-owner); CK_ENSURE catches violations |
| Tarjan SCC implementation correctness | Test #4 (`AtomicLeaf`) and an explicit cycle-detection test (could be added as test #15) cover this |
| AS bindings break across renames | Regenerate via editor launch after every phase; verify post-compile clean before toolbox build |
