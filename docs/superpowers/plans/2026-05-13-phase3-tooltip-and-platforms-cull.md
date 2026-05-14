# Phase 3 — SDeferredToolTip Backport + Platforms .ini Cull Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land two engine-fork patches on `D:\Repos\UnrealEngineAngelscript` (branch `main-ck`) to shave ~2-6 sec off editor cold-start per process: (1) port UE 5.8's `SDeferredToolTip` widget to defer tooltip construction, (2) add a `[Core.System] ConfigPlatformsToInclude=Windows` allow-list opt-in so the editor stops loading IOS/Android/Mac/Linux/etc. `.ini` files. Plus a one-line project-side opt-in in `Config/DefaultEngine.ini`.

**Architecture:** Two independent engine commits on `main-ck` so individual revert / bisect is clean, but bundled into one rebuild + measurement cycle for speed. Per-deliverable abort thresholds (tooltip ≥ 1.0 sec, cull ≥ 0.5 sec) with a documented attribution procedure if the combined saving under-delivers (< 1.5 sec). No new tests in this plan — verification is `--measure-compare` against the Phase 2 baseline, manual tooltip smoke, log grep for platforms, and the existing IskmRenderer 19-test set.

**Tech Stack:** UE 5.5 (chainkemists fork at `D:\Repos\UnrealEngineAngelscript`, branch `main-ck`), C++ (Slate + Core modules + PropertyEditor module), runreal build system (`runreal build editor`), `UnrealToolbox.exe --measure` / `--measure-compare` from Phase 1 / Phase 2.

**Spec:** [docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md](../specs/2026-05-13-sdeferredtooltip-backport-design.md)

**Prior phases:**
- [Phase 1](../specs/2026-05-13-editor-startup-phase1-design.md) — measurement infrastructure shipped (`--measure` / `--measure-compare`); 16 unused engine plugins disabled; `engine_init_seconds` saved -1.339s.
- [Phase 2](../specs/2026-05-13-toolbox-discovery-cache-design.md) — toolbox test-discovery cache; iteration cycle saved -32.944s on `total_to_pie_ready_seconds`.

---

## File Structure

**Modified in `D:\Repos\UnrealEngineAngelscript` (branch `main-ck`):**
- `Engine/Source/Runtime/Slate/Public/Widgets/SDeferredToolTip.h` — NEW. Copied verbatim from UE 5.8.
- `Engine/Source/Runtime/Slate/Private/Widgets/SDeferredToolTip.cpp` — NEW. Copied verbatim from UE 5.8.
- `Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp` — MODIFY. Two-line swap in `FSlateApplication::MakeToolTip` (both overloads) + one `#include`.
- `Engine/Source/Editor/PropertyEditor/Private/PropertyEditorHelpers.cpp` — MODIFY. Adapt `SPropertyNameWidget::Construct` to wrap the documentation tooltip in `SDeferredToolTip` + one `#include`. Adaptation diff confirmed during Task 2 Step 4.
- `Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp` — MODIFY. Add allow-list opt-in to `FConfigCacheIni::AsyncInitializeConfigForPlatforms` (~10-15 lines).

**Modified in `D:\Repos\CkPlugins` (branch `feature/generational-handle-migration`):**
- `Config/DefaultEngine.ini` — append `+ConfigPlatformsToInclude=Windows` to the `[Core.System]` section.
- `docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md` — fill the Phase 3 Results section at sign-off.

**Engine commits land on `main-ck` directly** (no feature branch on the fork). CkPlugins changes land on the existing `feature/generational-handle-migration` branch.

---

## Working-directory note for agentic workers

Two repos plus one engine fork are touched:
- **Engine fork:** `D:\Repos\UnrealEngineAngelscript` (branch `main-ck`). Use `git -C /d/Repos/UnrealEngineAngelscript ...`.
- **CkPlugins project:** `D:\Repos\CkPlugins` (branch `feature/generational-handle-migration`). Use `git -C /d/Repos/CkPlugins ...`.
- **UE 5.8 reference (read-only):** `E:\UE_5.8`. Used for verbatim file copies in Task 2 and to diff-check Task 5's structural change.

Don't `cd` mid-step (brittle in PowerShell + bash interop); always absolute paths or `git -C`.

**Critical: `git diff --cached --name-only` before EVERY commit.** A prior session in this repo accidentally bundled 7 unrelated binary files into a documentation commit because something in the editor environment auto-staged them between `git add` and `git commit`. Run the cached-name check after `git add` and confirm only the intended files are listed before invoking `git commit`. If anything unexpected appears, `git reset HEAD <path>` to unstage.

Editor lock check (before any toolbox `--test` cycle in Tasks 6, 8) — the standard helper:

```bash
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
```

If `locked`, wait ~60 sec and retry; don't kill the editor process.

**Engine rebuild commands** (used in Task 5):

```bash
# From the CkPlugins project directory — runreal reads engine path from runreal.config.json
runreal build editor
```

This rebuilds the engine modules that changed. For Phase 3, that's Core (platforms cull), Slate (tooltip new files + SlateApplication.cpp swap), and PropertyEditor (helpers diff). Expected wall-clock on iterative rebuild: ~15-30 min if those module's binaries are already linked; up to ~60 min if Core triggers a downstream rebuild. Capture the start/end timestamp so the plan results note the build cost.

---

## Task 1: Capture pre-patch baseline measurement

**Files:** None modified.

This task pins down the measurement noise floor and confirms the Phase 2 baseline numbers are still representative against the current binary. Without this, a "small saving" result later can't be distinguished from variance.

- [ ] **Step 1: Editor lock check**

```bash
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
```

Expected: `free`. If `locked`, wait and retry.

- [ ] **Step 2: Capture postbuild baseline snapshot**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase3-pre-postbuild.log --project='D:\Repos\CkPlugins'"
```

Expected: build completes, editor runs, 19/19 IskmRenderer pass. Produces `Saved/Logs/Phase3-pre-postbuild.log` and `Saved/Logs/Phase3-pre-postbuild.startup.json`.

Note: `--build` here will rebuild the toolbox project against the current engine, NOT rebuild the engine itself. Engine rebuilds happen via `runreal build editor` (Task 5).

- [ ] **Step 3: Editor lock check, then capture cached baseline snapshot**

```bash
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
```

When `free`:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase3-pre-cached.log --project='D:\Repos\CkPlugins'"
```

Expected: 19/19 pass, Phase 2 cached path active (log line `Using cached test list (5391 tests)`). Produces `Saved/Logs/Phase3-pre-cached.startup.json`.

- [ ] **Step 4: Sanity-check the pre-baseline vs Phase 2 final**

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-final-postbuild.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/Phase3-pre-postbuild.startup.json
```

Expected: `engine_init_seconds` and `total_to_pie_ready_seconds` deltas within ~±2 sec (run-to-run noise). If a delta is larger than that, the environment has drifted since Phase 2 sign-off and the implementer should investigate before proceeding (e.g. plugin state changed, hardware load differs).

Record the noise floor in a short note for the Phase 3 Results section — the per-deliverable thresholds (tooltip ≥ 1.0 sec, cull ≥ 0.5 sec) are only meaningful relative to this noise.

- [ ] **Step 5: No commit needed — measurement artifacts are gitignored (`Saved/Logs/`).**

---

## Task 2: Apply Deliverable 1 (tooltip backport) and commit

**Files:**
- Create: `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\Slate\Public\Widgets\SDeferredToolTip.h`
- Create: `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\Slate\Private\Widgets\SDeferredToolTip.cpp`
- Modify: `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\Slate\Private\Framework\Application\SlateApplication.cpp`
- Modify: `D:\Repos\UnrealEngineAngelscript\Engine\Source\Editor\PropertyEditor\Private\PropertyEditorHelpers.cpp`

- [ ] **Step 1: Copy `SDeferredToolTip.h` from UE 5.8 verbatim**

Source: `E:\UE_5.8\Engine\Source\Runtime\Slate\Public\Widgets\SDeferredToolTip.h` (93 lines).
Destination: `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\Slate\Public\Widgets\SDeferredToolTip.h`.

Copy the file verbatim. No edits needed — the `IToolTip` interface at `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\SlateCore\Public\Widgets\IToolTip.h` is byte-identical to UE 5.8's, so all nine virtual methods overridden by `SDeferredToolTip` / `SDeferredToolTipText` line up cleanly.

After copy, verify the file landed:

```bash
ls D:/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Slate/Public/Widgets/SDeferredToolTip.h
head -5 D:/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Slate/Public/Widgets/SDeferredToolTip.h
```

Expected: file exists, first line is `// Copyright Epic Games, Inc. All Rights Reserved.`.

- [ ] **Step 2: Copy `SDeferredToolTip.cpp` from UE 5.8 verbatim**

Source: `E:\UE_5.8\Engine\Source\Runtime\Slate\Private\Widgets\SDeferredToolTip.cpp` (215 lines).
Destination: `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\Slate\Private\Widgets\SDeferredToolTip.cpp`.

Copy the file verbatim.

After copy:

```bash
ls D:/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Slate/Private/Widgets/SDeferredToolTip.cpp
head -5 D:/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Slate/Private/Widgets/SDeferredToolTip.cpp
```

Expected: file exists, first line is `// Copyright Epic Games, Inc. All Rights Reserved.`.

- [ ] **Step 3: Modify `SlateApplication.cpp` — swap both `MakeToolTip` overloads**

Open `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\Slate\Private\Framework\Application\SlateApplication.cpp`.

First, add the include. Find the existing `#include "Widgets/SToolTip.h"` line and insert immediately after:

```cpp
#include "Widgets/SDeferredToolTip.h"
```

Then locate the existing `FSlateApplication::MakeToolTip` definitions around line 4647 (use grep `MakeToolTip(const TAttribute<FText>` to find them). The current bodies are:

```cpp
TSharedRef<IToolTip> FSlateApplication::MakeToolTip(const TAttribute<FText>& ToolTipText)
{
    return SNew(SToolTip)
        .Text(ToolTipText);
}

TSharedRef<IToolTip> FSlateApplication::MakeToolTip( const FText& ToolTipText )
{
    return SNew(SToolTip)
        .Text(ToolTipText);
}
```

Replace both bodies (keep the function signatures unchanged):

```cpp
TSharedRef<IToolTip> FSlateApplication::MakeToolTip(const TAttribute<FText>& ToolTipText)
{
    return MakeShared<SDeferredToolTipText>(ToolTipText);
}

TSharedRef<IToolTip> FSlateApplication::MakeToolTip( const FText& ToolTipText )
{
    return MakeShared<SDeferredToolTipText>(ToolTipText);
}
```

After the edit, sanity-check the file:

```bash
grep -n "SDeferredToolTipText\|MakeToolTip" /d/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp | head -10
```

Expected: at least two `MakeShared<SDeferredToolTipText>` matches (one per overload) plus the `#include` line.

- [ ] **Step 4: Inspect our fork's `SPropertyNameWidget::Construct` to confirm the PropertyEditorHelpers adaptation diff**

Before editing `PropertyEditorHelpers.cpp`, read the current `SPropertyNameWidget::Construct` body to understand the shape we're adapting from. The UE 5.8 form wraps the documentation-tooltip construction in a lambda passed to `SDeferredToolTip` — but our 5.5 fork may have the eager `IDocumentation::Get()->CreateToolTip(...)` call inline at a different surrounding shape.

```bash
grep -n "SPropertyNameWidget::Construct" /d/Repos/UnrealEngineAngelscript/Engine/Source/Editor/PropertyEditor/Private/PropertyEditorHelpers.cpp
```

Read ~40 lines starting at the `::Construct` line. Confirm:
1. The function body still constructs a documentation tooltip via `IDocumentation::Get()->CreateToolTip(...)` somewhere — or an equivalent factory call.
2. The result of that call is passed into a `.ToolTip(...)` slot on a child widget (likely `SPropertyEditorTitle`).
3. The captures available (`PropertyEditor` or similar weak/shared pointer to the property editor, the tooltip text source) are usable inside a lambda.

If all three hold, proceed to Step 5 with the standard adaptation.

**If our fork's structure differs in a way that prevents a clean lambda wrap** (e.g. the tooltip variable is consumed by multiple sibling widgets, or the surrounding code constructs it via a non-trivial helper that can't be captured): SKIP this step, document the skip in the commit message (and in the Phase 3 Results section), and move on. The skip costs us a fraction of a second of saving from this specific call site; the SlateApplication.cpp change in Step 3 captures the bulk of the tooltip win.

- [ ] **Step 5: Modify `PropertyEditorHelpers.cpp` — wrap documentation tooltip in `SDeferredToolTip`**

Open `D:\Repos\UnrealEngineAngelscript\Engine\Source\Editor\PropertyEditor\Private\PropertyEditorHelpers.cpp`.

Add the include near the other `Widgets/` includes at the top of the file:

```cpp
#include "Widgets/SDeferredToolTip.h"
```

In `SPropertyNameWidget::Construct`, locate the eager documentation-tooltip construction (the call to `IDocumentation::Get()->CreateToolTip(...)`). Replace it with the UE 5.8 form:

```cpp
TWeakPtr<FPropertyEditor> WeakPropertyEditor = PropertyEditor;
// Note: ToolTipText has to be captured immediately because FPropertyHandleBase::CreatePropertyNameWidget has a ToolTipOverride
//  that cannot be deferred since it's restored to FText::Empty right after SPropertyNameWidget::Construct.
auto CreateDocumentationToolTipDeferred = [WeakPropertyEditor, ToolTipText = PropertyEditor->GetToolTipText()]() -> TSharedPtr<IToolTip>
{
    if (TSharedPtr<FPropertyEditor> PropertyEditorPinned = WeakPropertyEditor.Pin())
    {
        return IDocumentation::Get()->CreateToolTip(ToolTipText, NULL, PropertyEditorPinned->GetDocumentationLink(), PropertyEditorPinned->GetDocumentationExcerptName());
    }

    return SNew(SToolTip)
        .Text(LOCTEXT("EditorDestroyedToolTip", "Invalid ToolTip, source editor closed."));
};
TSharedRef<SDeferredToolTip> DeferredDocumentationToolTip = MakeShared<SDeferredToolTip>(FOnGetDeferredToolTip::CreateLambda(CreateDocumentationToolTipDeferred));
```

Then plumb `DeferredDocumentationToolTip` into the `.ToolTip(...)` slot of whichever child widget previously consumed the eager tooltip. The UE 5.8 form passes it to `SPropertyEditorTitle`:

```cpp
SNew( SPropertyEditorTitle, PropertyEditor.ToSharedRef() )
    .OnDoubleClicked( InArgs._OnDoubleClicked )
    .ToolTip(DeferredDocumentationToolTip)
```

Adapt to whatever widget our fork passes the tooltip into (read in Step 4).

After the edit:

```bash
grep -n "SDeferredToolTip\|CreateDocumentationToolTipDeferred" /d/Repos/UnrealEngineAngelscript/Engine/Source/Editor/PropertyEditor/Private/PropertyEditorHelpers.cpp
```

Expected: at least 3 matches (include + lambda + MakeShared call).

- [ ] **Step 6: Verify staging contains exactly four files**

```bash
git -C /d/Repos/UnrealEngineAngelscript add Engine/Source/Runtime/Slate/Public/Widgets/SDeferredToolTip.h
git -C /d/Repos/UnrealEngineAngelscript add Engine/Source/Runtime/Slate/Private/Widgets/SDeferredToolTip.cpp
git -C /d/Repos/UnrealEngineAngelscript add Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp
git -C /d/Repos/UnrealEngineAngelscript add Engine/Source/Editor/PropertyEditor/Private/PropertyEditorHelpers.cpp
echo "=== staged files (should be exactly 4) ==="
git -C /d/Repos/UnrealEngineAngelscript diff --cached --name-only
```

Expected: exactly four files listed, all paths matching the four above. If Step 4 elected to skip the PropertyEditorHelpers change, expect three files instead.

If any other path appears, `git -C /d/Repos/UnrealEngineAngelscript reset HEAD <path>` to unstage and investigate.

- [ ] **Step 7: Commit Deliverable 1**

```bash
git -C /d/Repos/UnrealEngineAngelscript commit -m "$(cat <<'EOF'
feat(slate): backport SDeferredToolTip from UE 5.8

Defers SToolTip widget construction from "every widget at construction
time" (~38000 eager SNew(SToolTip) in DebugGame editor init) to "first
tooltip access". Two new classes (SDeferredToolTip + SDeferredToolTipText)
in Slate/Public+Private/Widgets/, plus two call-site swaps:

  - Slate/Private/Framework/Application/SlateApplication.cpp:
    FSlateApplication::MakeToolTip (both overloads) now returns
    MakeShared<SDeferredToolTipText> instead of SNew(SToolTip).
  - Editor/PropertyEditor/Private/PropertyEditorHelpers.cpp:
    SPropertyNameWidget::Construct wraps the documentation tooltip in
    SDeferredToolTip with a lambda capture (matches UE 5.8 shape).

Phase 3 spec:
  docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md
  (in chainkemists/CkPlugins).

Source article documenting the optimization:
  https://larstofus.com/2025/09/02/speeding-up-the-unreal-editor-launch-by-not-spawning-38000-tooltips/

Epic's measurements: 2-5 sec on Debug builds, ~1 sec on Development,
~40 MB RAM saved. Our measured saving recorded in Phase 3 Results.
EOF
)"
```

If Step 4 skipped PropertyEditorHelpers, replace the "two call-site swaps" wording with "one call-site swap" and drop the `PropertyEditorHelpers.cpp` bullet.

- [ ] **Step 8: Verify the commit**

```bash
git -C /d/Repos/UnrealEngineAngelscript show HEAD --stat
```

Expected: 3 or 4 files changed, two `create mode` entries (the new `SDeferredToolTip.h/.cpp`), title `feat(slate): backport SDeferredToolTip from UE 5.8`.

---

## Task 3: Apply Deliverable 2 (platforms cull) and commit

**Files:**
- Modify: `D:\Repos\UnrealEngineAngelscript\Engine\Source\Runtime\Core\Private\Misc\ConfigCacheIni.cpp`

- [ ] **Step 1: Locate the existing `AsyncInitializeConfigForPlatforms` function**

```bash
grep -n "void FConfigCacheIni::AsyncInitializeConfigForPlatforms" /d/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp
```

Expected: one match around line 6390. The function is gated by `#if WITH_EDITOR` (the `#if` is above the function, the matching `#endif` is just below).

- [ ] **Step 2: Modify `AsyncInitializeConfigForPlatforms` — add allow-list opt-in**

The current function body (lines 6390-6420 approximately):

```cpp
void FConfigCacheIni::AsyncInitializeConfigForPlatforms()
{
    // make sure any (non-const static) paths the worker threads will use are already initialized
    FPaths::ProjectDir();
    FPlatformMisc::GeneratedConfigDir(); // also inits FPaths::ProjectSavedDir
    FConfigContext::EnsureRequiredGlobalPathsHaveBeenInitialized();
    FPlatformProcess::ApplicationSettingsDir();

    // pre-create all platforms so that the loop below doesn't reallocate anything in the map
    const TMap<FName, FDataDrivenPlatformInfo>& AllPlatformInfos = FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos();
    for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
    {
        GetPlatformConfigFutures().Emplace(Pair.Key);
        ConfigForPlatform.Add(Pair.Key, new FConfigCacheIni(EConfigCacheType::Temporary, Pair.Key, true /* bInGloballyRegistered */));
    }

    for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
    {
        FName PlatformName = Pair.Key;
        GetPlatformConfigFutures()[PlatformName] = Async(EAsyncExecution::ThreadPool, [PlatformName]
        {
            double Start = FPlatformTime::Seconds();

            FConfigCacheIni* NewConfig = ConfigForPlatform.FindChecked(PlatformName);
            FConfigContext Context = FConfigContext::ReadIntoConfigSystem(NewConfig, PlatformName.ToString());
            InitializeKnownConfigFiles(Context);

            UE_LOG(LogConfig, Display, TEXT("Loading %s ini files took %.2f seconds"), *PlatformName.ToString(), FPlatformTime::Seconds() - Start);
        });
    }
}
```

Replace the body with the allow-list-aware version. Add the two new lines below the four existing `FPaths/FPlatform...` calls, then add the `if (!ShouldInclude(...)) continue;` guard at the top of each `for` loop:

```cpp
void FConfigCacheIni::AsyncInitializeConfigForPlatforms()
{
    // make sure any (non-const static) paths the worker threads will use are already initialized
    FPaths::ProjectDir();
    FPlatformMisc::GeneratedConfigDir(); // also inits FPaths::ProjectSavedDir
    FConfigContext::EnsureRequiredGlobalPathsHaveBeenInitialized();
    FPlatformProcess::ApplicationSettingsDir();

    // [Phase 3 / chainkemists fork] Optional allow-list: when
    // [Core.System] ConfigPlatformsToInclude is non-empty, skip every
    // platform not on the list. Default empty = include all (no behaviour
    // change for stock fork consumers).
    TArray<FString> PlatformsToInclude;
    GConfig->GetArray(TEXT("Core.System"), TEXT("ConfigPlatformsToInclude"), PlatformsToInclude, GEngineIni);
    const auto ShouldInclude = [&PlatformsToInclude](FName PlatformName)
    {
        return PlatformsToInclude.Num() == 0 || PlatformsToInclude.Contains(PlatformName.ToString());
    };

    // pre-create all platforms so that the loop below doesn't reallocate anything in the map
    const TMap<FName, FDataDrivenPlatformInfo>& AllPlatformInfos = FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos();
    for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
    {
        if (!ShouldInclude(Pair.Key)) continue;
        GetPlatformConfigFutures().Emplace(Pair.Key);
        ConfigForPlatform.Add(Pair.Key, new FConfigCacheIni(EConfigCacheType::Temporary, Pair.Key, true /* bInGloballyRegistered */));
    }

    for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
    {
        if (!ShouldInclude(Pair.Key)) continue;
        FName PlatformName = Pair.Key;
        GetPlatformConfigFutures()[PlatformName] = Async(EAsyncExecution::ThreadPool, [PlatformName]
        {
            double Start = FPlatformTime::Seconds();

            FConfigCacheIni* NewConfig = ConfigForPlatform.FindChecked(PlatformName);
            FConfigContext Context = FConfigContext::ReadIntoConfigSystem(NewConfig, PlatformName.ToString());
            InitializeKnownConfigFiles(Context);

            UE_LOG(LogConfig, Display, TEXT("Loading %s ini files took %.2f seconds"), *PlatformName.ToString(), FPlatformTime::Seconds() - Start);
        });
    }
}
```

The two changes are: (1) read `ConfigPlatformsToInclude` from `GConfig` near the top, (2) `if (!ShouldInclude(Pair.Key)) continue;` at the start of both `for` loop bodies.

After the edit:

```bash
grep -n "ConfigPlatformsToInclude\|ShouldInclude" /d/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp
```

Expected: 4-5 matches (one `GConfig->GetArray`, one lambda definition, two `if (!ShouldInclude...)` guards, possibly one in a comment).

- [ ] **Step 3: Verify staging contains exactly one file**

```bash
git -C /d/Repos/UnrealEngineAngelscript add Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp
echo "=== staged files (should be exactly 1) ==="
git -C /d/Repos/UnrealEngineAngelscript diff --cached --name-only
```

Expected: exactly `Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp`.

- [ ] **Step 4: Commit Deliverable 2**

```bash
git -C /d/Repos/UnrealEngineAngelscript commit -m "$(cat <<'EOF'
feat(core): add ConfigPlatformsToInclude opt-in for editor platform .ini loading

FConfigCacheIni::AsyncInitializeConfigForPlatforms walks every platform
returned by FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos in
editor builds (~10 platforms x ~0.10 sec/each = ~1.2 sec on critical
path). Most are unused on Windows-only dev hosts.

Add an opt-in allow-list at [Core.System] ConfigPlatformsToInclude. When
the array is non-empty, only listed platforms get their .ini files loaded.
Default empty = include all (no behaviour change for stock fork consumers).

The chainkemists/CkPlugins project opts in via its Config/DefaultEngine.ini
(separate commit on the CkPlugins side).

Phase 3 spec:
  docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md
  (in chainkemists/CkPlugins).

Measured saving recorded in Phase 3 Results.
EOF
)"
```

- [ ] **Step 5: Verify the commit**

```bash
git -C /d/Repos/UnrealEngineAngelscript show HEAD --stat
```

Expected: 1 file changed, title `feat(core): add ConfigPlatformsToInclude opt-in for editor platform .ini loading`.

- [ ] **Step 6: Verify both commits are on `main-ck`**

```bash
git -C /d/Repos/UnrealEngineAngelscript log --oneline -5
```

Expected: tip shows `feat(core): ...` followed by `feat(slate): ...` followed by whatever was on `main-ck` previously. Both are on `main-ck` (no feature branch).

---

## Task 4: Apply CkPlugins project-side opt-in and commit

**Files:**
- Modify: `D:\Repos\CkPlugins\Config\DefaultEngine.ini`

- [ ] **Step 1: Locate `[Core.System]` section in `Config/DefaultEngine.ini`**

```bash
grep -n "\[Core.System\]" /d/Repos/CkPlugins/Config/DefaultEngine.ini
```

Expected: zero or one match.

- [ ] **Step 2: Append the opt-in line**

If the section exists, insert `+ConfigPlatformsToInclude=Windows` on the line immediately after `[Core.System]`. If it doesn't exist, append at the end of the file:

```ini

[Core.System]
+ConfigPlatformsToInclude=Windows
```

After edit:

```bash
grep -nA 2 "\[Core.System\]" /d/Repos/CkPlugins/Config/DefaultEngine.ini
```

Expected: the `[Core.System]` line followed by `+ConfigPlatformsToInclude=Windows`.

- [ ] **Step 3: Verify staging contains exactly the .ini**

```bash
git -C /d/Repos/CkPlugins add Config/DefaultEngine.ini
echo "=== staged files (should be exactly 1) ==="
git -C /d/Repos/CkPlugins diff --cached --name-only
```

Expected: exactly `Config/DefaultEngine.ini`.

If anything else shows up — particularly the assorted untracked `Content/`, `Build/`, or `docs/superpowers/` files that have been auto-staging in this repo lately — unstage them with `git -C /d/Repos/CkPlugins reset HEAD <path>` and re-run the check.

- [ ] **Step 4: Commit the project-side opt-in**

```bash
git -C /d/Repos/CkPlugins commit -m "$(cat <<'EOF'
chore(config): opt into ConfigPlatformsToInclude=Windows allow-list

Phase 3 Deliverable 2 project-side activation. With the matching engine
fork patch on main-ck (UnrealEngineAngelscript: feat(core): add
ConfigPlatformsToInclude opt-in), editor startup now skips ~9 non-Windows
platform .ini loads (IOS, Android, Mac, Linux, etc.), saving ~1.2 sec
per editor process.

If a future build needs additional platforms (e.g. Linux for headless
CI), add another +ConfigPlatformsToInclude=<Platform> line.

Phase 3 spec:
  docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md
EOF
)"
```

- [ ] **Step 5: Verify the commit**

```bash
git -C /d/Repos/CkPlugins show HEAD --stat
```

Expected: 1 file changed (`Config/DefaultEngine.ini`), title `chore(config): opt into ConfigPlatformsToInclude=Windows allow-list`.

---

## Task 5: Rebuild engine

**Files:** None modified — this task triggers an engine rebuild from the changes landed in Tasks 2-3.

- [ ] **Step 1: Editor lock check**

```bash
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
```

Expected: `free`. Rebuild can run with the editor open (rebuilds engine binaries, not the editor's in-memory state), but it's still safer to close any open editor session that might be re-locking files mid-build.

- [ ] **Step 2: Capture rebuild start time**

```bash
date '+%H:%M:%S'
```

Record the timestamp. Used in the Phase 3 Results section to document the rebuild cost.

- [ ] **Step 3: Run `runreal build editor`**

```bash
cd /d/Repos/CkPlugins
runreal build editor 2>&1 | tee Saved/Logs/Phase3-engine-rebuild.log
```

Expected wall-clock: ~15-30 min iterative; up to ~60 min if Core triggers a downstream cascade. Watch for compilation errors — if any of the modified files fail to compile, the build fails fast.

Common failures and recovery:
- **Missing include for `SDeferredToolTip.h` in `SlateApplication.cpp`** → check that the include was added in Task 2 Step 3. If missing, add it and re-run.
- **`SLATE_API` macro missing on a new class method** → the verbatim copy from UE 5.8 should already have these. If a method is flagged as undefined, confirm the verbatim copy didn't drop any.
- **`GConfig->GetArray` signature mismatch** → unlikely (the API is stable across 5.5/5.8) but if it surfaces, check whether our fork has a non-stock `GConfig`. Adapt the call signature.
- **PropertyEditorHelpers adaptation breaks compile** → revisit Task 2 Step 4 — the adaptation may need a different surrounding shape than UE 5.8's. The fallback is to back out only the PropertyEditorHelpers change (`git -C /d/Repos/UnrealEngineAngelscript checkout HEAD~1 -- Engine/Source/Editor/PropertyEditor/Private/PropertyEditorHelpers.cpp` then `git -C /d/Repos/UnrealEngineAngelscript commit --amend` to keep just the Slate parts of Deliverable 1).

If the build succeeds, record end time:

```bash
date '+%H:%M:%S'
```

The wall-clock delta (end − start) goes in the Phase 3 Results section.

- [ ] **Step 4: Verify the built editor binary timestamp**

```bash
# Path varies by config; typical Debug location:
ls -la $(find D:/Repos/UnrealEngineAngelscript/Engine/Binaries/Win64 -name "UnrealEditor*.exe" 2>/dev/null | head -1) 2>&1 | head -3
```

Expected: editor binary modification time within the rebuild window (between start and end timestamps from Step 2/3).

- [ ] **Step 5: No commit needed.** Engine commits already landed in Tasks 2-3; this task is the rebuild that turns those commits into runnable binaries.

---

## Task 6: Run measurement + smoke + IskmRenderer

**Files:** None modified — this task runs the toolbox against the rebuilt engine, captures snapshots, and runs the smoke checklist.

- [ ] **Step 1: Editor lock check, then capture post-patch postbuild snapshot**

```bash
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
```

When `free`:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase3-combined-postbuild.log --project='D:\Repos\CkPlugins'"
```

Expected: 19/19 IskmRenderer pass; produces `Saved/Logs/Phase3-combined-postbuild.startup.json`. Note: `--build` here rebuilds the toolbox project against the (now-rebuilt) engine, NOT the engine itself.

- [ ] **Step 2: Editor lock check, then capture post-patch cached snapshot**

When `free`:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase3-combined-cached.log --project='D:\Repos\CkPlugins'"
```

Expected: 19/19 pass, Phase 2 cached path active (log line `Using cached test list (...)`); produces `Saved/Logs/Phase3-combined-cached.startup.json`.

- [ ] **Step 3: Verify IskmRenderer 19/19 on both runs**

```bash
grep "=== Test summary ===" -A 4 /d/Repos/CkPlugins/Saved/Logs/Phase3-combined-postbuild.log
echo "---"
grep "=== Test summary ===" -A 4 /d/Repos/CkPlugins/Saved/Logs/Phase3-combined-cached.log
```

Expected: both runs show `Total: 19`, `Passed: 19`, `Failed: 0`.

If either run shows fewer than 19 passed, STOP and investigate. A failed IskmRenderer test means a system depended on something the patches broke (most likely candidate: a system depending on a non-Windows platform config that's now skipped).

- [ ] **Step 4: Platforms cull smoke — grep the log for platform .ini loads**

```bash
grep "Loading .* ini files took" /d/Repos/CkPlugins/Saved/Logs/Phase3-combined-postbuild.log
```

Expected: only one or two lines, naming `Windows` (or whichever platforms are on the allow-list). Absence of `IOS`, `Android`, `Mac`, `Linux`, `PS5`, `Switch`, `TVOS`, etc. confirms the cull is active.

If the log shows all platforms still loading, the project-side `+ConfigPlatformsToInclude=Windows` line in Task 4 didn't take effect (typo in the section name, in the key name, or the `+` prefix). Check `Config/DefaultEngine.ini`.

- [ ] **Step 5: Tooltip smoke (manual, 4 scenes)**

This step requires the user to interact with the editor manually. The implementer should:

1. Launch the editor: `runreal run editor` (or open the project in any locally-installed UE 5.5 instance pointing at the chainkemists fork).
2. Open any Blueprint asset. Hover a graph node. **Confirm:** tooltip appears with text (Blueprint node description).
3. Open Project Settings. Hover a property name in the right column. **Confirm:** documentation tooltip appears with text and (if a documentation excerpt is wired) the larger documentation pane.
4. Hover any toolbar button (e.g. "Play"). **Confirm:** tooltip appears.

A blank tooltip, a stale tooltip, or a crash on hover is a regression. If any scene fails, STOP and decide between (a) backing out Deliverable 1 (Task 8 Branch C subset) or (b) backing out just the PropertyEditorHelpers part if the failure is localized to property tooltips.

If the implementer is a non-interactive agent that cannot exercise the editor UI, document this as "manual smoke deferred to user" and proceed; the user runs the four scenes before sign-off.

- [ ] **Step 6: No commit needed — measurement artifacts gitignored.**

---

## Task 7: `--measure-compare` and decide

**Files:** None modified — this task computes deltas and triages the result.

- [ ] **Step 1: Compare against Phase 2 baseline (postbuild cycle)**

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-final-postbuild.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/Phase3-combined-postbuild.startup.json \
    2>&1 | tee /d/Repos/CkPlugins/Saved/Logs/Phase3-compare-postbuild.txt
```

Expected: `engine_init_seconds` and/or `total_to_pie_ready_seconds` show negative delta (faster). Total improvement should be roughly the combined ~2-6 sec.

Record the per-phase deltas.

- [ ] **Step 2: Compare against Phase 2 baseline (cached cycle)**

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-final-cached.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/Phase3-combined-cached.startup.json \
    2>&1 | tee /d/Repos/CkPlugins/Saved/Logs/Phase3-compare-cached.txt
```

Expected: same per-phase savings on the cached path. Confirms the Phase 2 cached saving (-32.6 sec on `total_to_pie_ready_seconds` vs postbuild) is preserved.

- [ ] **Step 3: Apply the decision gate**

Pick a branch in Task 8 based on the combined `engine_init_seconds` improvement from Step 1:

| Combined `engine_init_seconds` delta | Branch | Action |
|---|---|---|
| ≥ 1.5 sec faster | **8A — Sign off** | Fill Phase 3 Results, push fork + CkPlugins, done. |
| 1.0-1.5 sec faster | **8B — Per-deliverable attribution** | Revert one commit, rebuild, remeasure; decide per-patch. |
| < 1.0 sec faster | **8C — Abandon Phase 3** | Revert both engine commits and the CkPlugins commit, document negative result. |

Record the branch decision and proceed to Task 8.

If the IskmRenderer regression caused a forced stop in Task 6 Step 3, skip ahead to Task 8 Branch C (abandon) instead.

If the manual tooltip smoke caused a forced stop in Task 6 Step 5, decide per scene which Deliverable is at fault and go to Branch B with that one targeted.

- [ ] **Step 4: No commit needed — comparison artifacts are gitignored.**

---

## Task 8: Sign-off / attribution / rollback

Pick the branch per Task 7 Step 3. Only one of A, B, C runs.

### Branch 8A — Sign off (combined ≥ 1.5 sec)

- [ ] **Step 1: Fill the Phase 3 Results section in the spec**

Edit `D:\Repos\CkPlugins\docs\superpowers\specs\2026-05-13-sdeferredtooltip-backport-design.md`. Replace the placeholder text under `## Phase 3 Results (filled in on completion)` with:

```markdown
## Phase 3 Results

**Combined deliverables (tooltip + platforms cull):**

| Metric | Phase 2 final | Phase 3 combined | Delta |
|---|---|---|---|
| engine_init_seconds (postbuild) | <fill> | <fill> | <fill> |
| total_to_pie_ready_seconds (postbuild) | <fill> | <fill> | <fill> |
| engine_init_seconds (cached) | <fill> | <fill> | <fill> |
| total_to_pie_ready_seconds (cached) | <fill> | <fill> | <fill> |

**Pre-patch noise floor (Phase 2 final vs Phase 3 pre-baseline):** ±<fill> sec on engine_init_seconds.

**Engine rebuild cost:** <start time> → <end time> = <duration> min.

**Smoke tests:**
- Tooltip scenes (4): all passed / failed at scene N (describe).
- Platforms cull log grep: only Windows .ini load present / additional platforms (list).
- IskmRenderer 19/19 on postbuild and cached.

**Phase 4 decision:** <pursue toolbox single-process flow / pursue AS marker re-enable / not pursue further / etc.>. Reasoning: <fill>.
```

Fill all `<fill>` placeholders with real numbers from Tasks 6-7.

- [ ] **Step 2: Stage the spec update + verify**

```bash
git -C /d/Repos/CkPlugins add docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md
echo "=== staged files (should be exactly 1) ==="
git -C /d/Repos/CkPlugins diff --cached --name-only
```

Expected: exactly `docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md`.

- [ ] **Step 3: Commit the Phase 3 sign-off on CkPlugins**

```bash
git -C /d/Repos/CkPlugins commit -m "$(cat <<'EOF'
docs(spec): record Phase 3 Results — tooltip + platforms cull shipped

Combined engine-fork patches saved <X.X> sec on engine_init_seconds.
Tooltip backport: <X.X> sec. Platforms cull: <X.X> sec. IskmRenderer
19/19 on both postbuild and cached paths. All four tooltip smoke scenes
passed. Platforms log grep confirms only Windows .ini loads remain.

Phase 4 candidates (deferred / pursue / abandoned): see spec section.
EOF
)"
```

Replace `<X.X>` with the actual numbers from Step 1.

- [ ] **Step 4: Push the engine fork**

```bash
git -C /d/Repos/UnrealEngineAngelscript push origin main-ck 2>&1 | tail -10
```

Expected: two commits land (`feat(slate): ...`, `feat(core): ...`). If push prompts for credentials non-interactively, the agent stops and the user pushes manually.

- [ ] **Step 5: Push CkPlugins**

```bash
git -C /d/Repos/CkPlugins push 2>&1 | tail -10
```

Expected: two commits land on `feature/generational-handle-migration` (`chore(config): ...`, `docs(spec): record Phase 3 Results ...`).

- [ ] **Step 6: Verify all three remotes match local**

```bash
git -C /d/Repos/UnrealEngineAngelscript log origin/main-ck..HEAD --oneline
git -C /d/Repos/CkPlugins log origin/feature/generational-handle-migration..HEAD --oneline
```

Both should print nothing (zero commits ahead).

Phase 3 is signed off.

### Branch 8B — Per-deliverable attribution (combined 1.0-1.5 sec)

The combined saving fell below the ≥ 1.5 sec sign-off threshold but above the abandon threshold. Time to figure out which deliverable carries weight.

- [ ] **Step 1: Revert Deliverable 2 (platforms cull) on `main-ck`**

```bash
git -C /d/Repos/UnrealEngineAngelscript revert HEAD --no-edit
```

This creates a new revert commit on `main-ck` that backs out the `ConfigCacheIni.cpp` change. Don't push yet — we may revert this revert if it turns out the cull is the load-bearing patch.

- [ ] **Step 2: Rebuild engine**

```bash
cd /d/Repos/CkPlugins
runreal build editor 2>&1 | tail -20
```

Should be a faster rebuild this time (only Core module relinks, no header changes).

- [ ] **Step 3: Run measurement (postbuild + cached)**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase3-tooltip-only-postbuild.log --project='D:\Repos\CkPlugins'"
# (lock check between)
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase3-tooltip-only-cached.log --project='D:\Repos\CkPlugins'"
```

- [ ] **Step 4: Compute tooltip-only saving**

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-final-postbuild.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/Phase3-tooltip-only-postbuild.startup.json
```

Subtract this from the combined saving (Task 7 Step 1) to derive the cull's individual contribution.

Per-deliverable thresholds:
- Tooltip alone ≥ 1.0 sec → keep tooltip commit.
- Cull alone ≥ 0.5 sec → keep cull commit.

- [ ] **Step 5: Resolve based on per-deliverable thresholds**

Four cases:

1. **Both above thresholds** (rare given combined is 1.0-1.5 sec, but possible if the two interact): revert the revert (`git -C /d/Repos/UnrealEngineAngelscript revert HEAD --no-edit`), rebuild, fall through to Branch 8A.
2. **Only tooltip above threshold**: keep the revert commit (cull is reverted; tooltip remains). Push fork + CkPlugins (don't push the `+ConfigPlatformsToInclude=Windows` line — it's harmless without the engine support, but cleaner not to ship it).
3. **Only cull above threshold**: revert the revert AND revert Deliverable 1 (tooltip). Push the resulting two-revert + one-keep state. The `Config/DefaultEngine.ini` line stays.
4. **Neither above threshold**: fall through to Branch 8C.

- [ ] **Step 6: Push and document**

Same shape as Branch 8A Steps 4-6 (push engine fork + CkPlugins), but the spec's Phase 3 Results section documents which deliverable was kept and which was reverted, with attribution numbers.

- [ ] **Step 7: Commit the spec sign-off** (same as Branch 8A Step 3, with the partial-result framing).

### Branch 8C — Abandon Phase 3 (combined < 1.0 sec OR regression detected)

- [ ] **Step 1: Revert both engine commits on `main-ck`**

```bash
git -C /d/Repos/UnrealEngineAngelscript log --oneline -5
# Confirm the two feat commits are at the tip:
#   feat(core): add ConfigPlatformsToInclude opt-in ...
#   feat(slate): backport SDeferredToolTip from UE 5.8
git -C /d/Repos/UnrealEngineAngelscript revert HEAD HEAD~1 --no-edit
```

Creates two revert commits on `main-ck`, undoing both deliverables.

- [ ] **Step 2: Revert the CkPlugins .ini change**

```bash
git -C /d/Repos/CkPlugins log --oneline -3
# Confirm chore(config): opt into ConfigPlatformsToInclude ... is at HEAD
git -C /d/Repos/CkPlugins revert HEAD --no-edit
```

- [ ] **Step 3: Document the negative result**

Edit `D:\Repos\CkPlugins\docs\superpowers\specs\2026-05-13-sdeferredtooltip-backport-design.md`. Under `## Phase 3 Results`:

```markdown
## Phase 3 Results — Abandoned

**Combined deliverables (tooltip + platforms cull): SAVING < 1.0 SEC**

| Metric | Phase 2 final | Phase 3 combined | Delta |
|---|---|---|---|
| engine_init_seconds (postbuild) | <fill> | <fill> | <fill> |
| total_to_pie_ready_seconds (postbuild) | <fill> | <fill> | <fill> |

Per-deliverable attribution (Branch 8B procedure run / not run):
- Tooltip alone: <fill> sec (threshold: ≥ 1.0 sec, achieved / missed)
- Platforms cull alone: <fill> sec (threshold: ≥ 0.5 sec, achieved / missed)

Hypotheses for why the saving fell short of Epic's reported 2-5 sec on Debug:
- <list>

All commits reverted on `main-ck` (UnrealEngineAngelscript) and on `feature/generational-handle-migration` (CkPlugins).

**Phase 4 decision:** <fill>.
```

- [ ] **Step 4: Commit the spec sign-off** (negative-result version)

```bash
git -C /d/Repos/CkPlugins add docs/superpowers/specs/2026-05-13-sdeferredtooltip-backport-design.md
git -C /d/Repos/CkPlugins diff --cached --name-only  # verify only the spec
git -C /d/Repos/CkPlugins commit -m "$(cat <<'EOF'
docs(spec): record Phase 3 negative result — patches reverted

Combined engine-fork patches (tooltip backport + platforms cull) saved
< 1.0 sec on engine_init_seconds against Phase 2 baseline. Below the
sign-off threshold (1.5 sec combined). Both engine commits and the
project-side .ini opt-in reverted.

See spec's Phase 3 Results section for measured numbers and hypotheses.
EOF
)"
```

- [ ] **Step 5: Push fork and CkPlugins**

```bash
git -C /d/Repos/UnrealEngineAngelscript push origin main-ck 2>&1 | tail -10
git -C /d/Repos/CkPlugins push 2>&1 | tail -10
```

Both should push four / three commits respectively (originals + reverts + spec doc).

Phase 3 is closed (abandoned). The next campaign cycle considers the remaining candidates (toolbox single-process flow, AS marker re-enable).

---

## Self-Review

**Spec coverage:**

- Spec Deliverable 1 (SDeferredToolTip backport) → Task 2 (all sub-steps cover the four-file change).
- Spec Deliverable 2 (platforms cull opt-in) → Task 3.
- Spec project-side opt-in → Task 4.
- Spec "Verification → Smoke" → Task 6 Steps 4-5.
- Spec "Verification → IskmRenderer regression suite" → Task 6 Step 3.
- Spec "Verification → Measurement" → Tasks 1, 6, 7.
- Spec "Error Handling" → Task 5 Step 3 (rebuild fails), Task 6 Step 3 (IskmRenderer regression), Task 6 Step 5 (tooltip regression), Task 8 (per-deliverable rollback).
- Spec "Success Criteria" → Task 8 Branch 8A.
- Spec "Sequencing" steps 1-10 → Tasks 1-8 (1:1 correspondence).

**Placeholder scan:**

- `<fill>` markers appear ONLY in Task 8 Branch 8A Step 1 (Phase 3 Results table to be filled with real numbers) and Branch 8C Step 3 (negative-result table). These are explicit substitution points, not unfilled placeholders.
- No `TBD`, `TODO`, `implement later`, or `add error handling` patterns.
- No "similar to Task N" — each task's code is repeated in full.

**Type consistency:**

- `SDeferredToolTipText` / `SDeferredToolTip` (classes from UE 5.8) used consistently across Task 2 Step 3 and Step 5.
- `ConfigPlatformsToInclude` (allow-list key name) consistent in Task 3 Step 2, Task 4 Step 2, and Task 6 Step 4 (grep verification).
- `MakeShared<SDeferredToolTipText>(ToolTipText)` shape consistent in Task 2 Step 3 (both overloads use the same call).
- `Phase2-final-postbuild.startup.json` / `Phase2-final-cached.startup.json` baseline paths consistent across Tasks 1, 7.
- Filename pattern `Phase3-combined-*` / `Phase3-tooltip-only-*` for measurement snapshots, consistent across Tasks 6, 7, 8.

**Sequencing:**

- Task 1 (baseline) is non-destructive and can run independently. Required input for Task 7's noise-floor sanity check.
- Tasks 2-4 (code edits + commits) are independent of each other in terms of code, but Tasks 2-3 must land on `main-ck` before Task 5 (rebuild). Task 4 lands on CkPlugins and could in principle land in parallel, but the plan sequences it after for cleanliness.
- Task 5 (rebuild) is the bottleneck. Single occurrence in Branch 8A. Branches 8B (attribution) and 8C (abandon) each require additional rebuilds — these are the cost of triage.
- Tasks 6-7 (measurement + compare) run only after Task 5 succeeds.
- Task 8 has three exclusive branches (8A/8B/8C). The decision happens in Task 7 Step 3.

**Risk acknowledgements:**

- Engine rebuild is the iteration cost (~15-30 min iterative, up to ~60 min if Core cascades). Plan minimizes rebuilds by bundling both deliverables into a single cycle.
- PropertyEditorHelpers adaptation has a documented fallback to "skip this call site" in Task 2 Step 4 — implementer should not block on adapting if the surrounding 5.5 shape resists.
- The `git diff --cached --name-only` discipline is repeated in every commit step because a prior session in this repo hit an auto-staging bug. The cost of the check is one extra command; the cost of an accidental commit is a soft-reset round trip.
- Branches 8B and 8C have full step-by-step recovery procedures; the abort criteria don't depend on judgment alone.
- The PAT-in-config security issue (engine fork remote URL with embedded `ghp_*` tokens) is out of scope for this plan but worth rotating at any convenient point.
