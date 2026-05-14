# Editor Startup — Phase 3: SDeferredToolTip Backport + Platforms .ini Cull

**Date:** 2026-05-13
**Author:** Sulfur-CK (with assistance)
**Status:** Approved for implementation planning

**Prior phases:**
- [Phase 1](2026-05-13-editor-startup-phase1-design.md) — measurement infrastructure + project-side plugin cull. Saved -1.339s on `engine_init_seconds`.
- [Phase 2](2026-05-13-toolbox-discovery-cache-design.md) — toolbox test-discovery cache. Saved -32.944s on the iteration cycle (`--test` alone, cached).

---

## Goal

Reduce per-process editor cold-start by ~2-6 sec via two independent engine-fork patches on `D:\Repos\UnrealEngineCk` (branch `main-ck`):

1. **Tooltip backport (1-5 sec)** — port Epic Games' UE 5.8 `SDeferredToolTip` / `SDeferredToolTipText` widgets into our UE 5.5 fork. The shipped UE 5.8 change defers tooltip widget construction from "every widget at construction time" (~38000 eager `SNew(SToolTip)` invocations during editor init in DebugGame) to "first tooltip access". Per Epic's measurements and the source article (https://larstofus.com/2025/09/02/speeding-up-the-unreal-editor-launch-by-not-spawning-38000-tooltips/), this saves 2-5 sec in Debug, ~1 sec in Development, and ~40 MB RAM.

2. **Platforms .ini cull (~1.2 sec)** — add a `[Core.System] ConfigPlatformsToInclude=Windows` allow-list opt-in to `FConfigCacheIni::AsyncInitializeConfigForPlatforms`. UE 5.5's stock behaviour walks every platform discovered by `FDataDrivenPlatformInfoRegistry` (~10 platforms × ~0.10 sec each) unconditionally in editor builds. The opt-in lets the project narrow to Windows-only; default empty = unchanged behaviour for any other consumer of the fork.

**Baseline metric:** `total_to_pie_ready_seconds` and `engine_init_seconds` from the `--measure` infrastructure, compared against Phase 2's final snapshots:
- `D:\Repos\CkPlugins\Saved\Logs\Phase2-final-postbuild.startup.json` (post-build cycle baseline).
- `D:\Repos\CkPlugins\Saved\Logs\Phase2-final-cached.startup.json` (iteration cycle baseline).

The toolbox layer doesn't change in this phase — the same `UnrealToolbox.exe` from Phase 2 runs against a rebuilt engine and the saving shows up in both cycles (since both spawn an editor that has to do the eager work today). The two patches land as two separate engine commits on `main-ck` so a future bisect or revert can identify which patch carries its weight; both are validated in a single rebuild + measurement cycle.

## Current State

### Tooltip path

UE 5.5's `FSlateApplication::MakeToolTip` (both overloads) constructs an `SToolTip` widget eagerly every time any code asks for a tooltip:

```cpp
// D:\Repos\UnrealEngineCk\Engine\Source\Runtime\Slate\Private\Framework\Application\SlateApplication.cpp:4647-4658
TSharedRef<IToolTip> FSlateApplication::MakeToolTip(const TAttribute<FText>& ToolTipText)
{
    return SNew(SToolTip)
        .Text(ToolTipText);
}

TSharedRef<IToolTip> FSlateApplication::MakeToolTip(const FText& ToolTipText)
{
    return SNew(SToolTip)
        .Text(ToolTipText);
}
```

Every widget that assigns a tooltip via `.ToolTipText(...)` ends up calling this factory at construction time. Editor toolbars, menus, detail panels, content browser, asset registry status — all of them pay this cost up front. Most tooltips will never be hovered before the user closes the editor.

`PropertyEditorHelpers.cpp`'s `SPropertyNameWidget::Construct` also constructs a documentation tooltip eagerly via `IDocumentation::Get()->CreateToolTip(...)` for every property row in detail panels.

### Platforms .ini path

`FConfigCacheIni::AsyncInitializeConfigForPlatforms` (in `Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp:6390`) enumerates every platform from `FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos()` and kicks off async config loads for each:

```cpp
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
        // ... reads <platform>.ini files, ~0.10 sec/platform ...
        UE_LOG(LogConfig, Display, TEXT("Loading %s ini files took %.2f seconds"), *PlatformName.ToString(), ...);
    });
}
```

On a Windows-only dev host we pay ~1.2 sec for IOS/Android/Mac/Linux/PS5/etc. config loads we'll never reference. The function is called from inside `#if WITH_EDITOR`, so the cost is editor-only — but every editor invocation pays it, and the cost compounds with the per-process spawn pattern in toolbox `--test` cycles.

## In Scope (Phase 3)

Two deliverables, both engine-fork patches on `D:\Repos\UnrealEngineCk` branch `main-ck`. Both land in the same rebuild + measurement cycle, but as two distinct commits so individual revert / bisect is clean.

### Deliverable 1 — Backport SDeferredToolTip + convert two call sites

**NEW** `Engine/Source/Runtime/Slate/Public/Widgets/SDeferredToolTip.h` (~93 lines, copy verbatim from UE 5.8).

Declares two classes implementing `IToolTip`:

- **`SDeferredToolTip`** — wraps a `FOnGetDeferredToolTip` delegate. First call to any `IToolTip` method invokes the delegate to construct the underlying tooltip, then forwards all subsequent calls to it. Used when the tooltip construction is non-trivial (e.g. needs to capture state, query a documentation system, etc.).
- **`SDeferredToolTipText`** — specialized for plain-text tooltips. Takes a `TAttribute<FText>`, lazily constructs an `SToolTip` with that text on first access. Used for the common case.

The UE 5.5 `IToolTip` interface at `D:\Repos\UnrealEngineCk\Engine\Source\Runtime\SlateCore\Public\Widgets\IToolTip.h` is byte-identical to UE 5.8's (same nine virtual methods, same defaults). Therefore the new header is a clean copy — no adaptation needed.

**NEW** `Engine/Source/Runtime/Slate/Private/Widgets/SDeferredToolTip.cpp` (~215 lines, copy verbatim from UE 5.8).

Method implementations all follow the same shape: call `TryCacheToolTip()` to lazily construct the underlying widget, then forward to it. The text-variant uses `SAssignNew(CachedToolTip, SToolTip).Text(ToolTipText)` in `TryCacheToolTip` to build the SToolTip on demand. The `OnOpening` / `OnClosed` callbacks on the text variant intentionally no-op (Epic notes "SToolTip does No-op. We choose to not forward no-op to no-op.").

**MODIFY** `Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp`:

```cpp
// Add at top of #include block (sorted under existing "Widgets/SToolTip.h"):
#include "Widgets/SDeferredToolTip.h"

// Replace lines 4647-4658:
TSharedRef<IToolTip> FSlateApplication::MakeToolTip(const TAttribute<FText>& ToolTipText)
{
    return MakeShared<SDeferredToolTipText>(ToolTipText);
}

TSharedRef<IToolTip> FSlateApplication::MakeToolTip(const FText& ToolTipText)
{
    return MakeShared<SDeferredToolTipText>(ToolTipText);
}
```

Two-line swap. This is the central tooltip factory; every `.ToolTipText(...)` Slate widget builder eventually routes through here. This change alone is expected to capture the bulk of the 1-5 sec saving.

**MODIFY** `Engine/Source/Editor/PropertyEditor/Private/PropertyEditorHelpers.cpp`:

Wrap the documentation-tooltip construction in `SPropertyNameWidget::Construct` so the eager `IDocumentation::Get()->CreateToolTip(...)` call is deferred until the tooltip is actually accessed. The UE 5.8 form:

```cpp
auto CreateDocumentationToolTipDeferred = [WeakPropertyEditor, ToolTipText = PropertyEditor->GetToolTipText()]()
{
    if (TSharedPtr<FPropertyEditor> PropertyEditorPinned = WeakPropertyEditor.Pin())
    {
        return IDocumentation::Get()->CreateToolTip(ToolTipText, NULL, PropertyEditorPinned->GetDocumentationLink(), PropertyEditorPinned->GetDocumentationExcerptName());
    };

    return SNew(SToolTip)
    .Text(LOCTEXT("EditorDestroyedToolTip", "Invalid ToolTip, source editor closed."));
};
TSharedRef<SDeferredToolTip> DeferredDocumentationToolTip = MakeShared<SDeferredToolTip>(FOnGetDeferredToolTip::CreateLambda(CreateDocumentationToolTipDeferred));
```

Our UE 5.5 fork's `SPropertyNameWidget::Construct` body does the eager `IDocumentation::Get()->CreateToolTip(...)` call inline. The port adapts the 5.5 shape to use the lambda + `SDeferredToolTip` wrap above, then plumbs `DeferredDocumentationToolTip` into `SPropertyEditorTitle`'s `.ToolTip(...)` slot. Implementation reads our fork's current `SPropertyNameWidget::Construct` body during the plan phase to confirm the exact diff shape; if the surrounding code differs structurally from 5.8 in a way that prevents a clean wrap, the port falls back to "skip this call site" rather than restructuring our fork to match 5.8.

### Deliverable 2 — Platforms .ini cull via opt-in allow-list

**MODIFY** `Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp`:

Add an allow-list opt-in to `FConfigCacheIni::AsyncInitializeConfigForPlatforms` (around line 6390). Defaults to empty = include all platforms (zero behaviour change for anyone else consuming the fork). When non-empty, the function walks only the listed platforms.

Diff shape (~10-15 lines added):

```cpp
void FConfigCacheIni::AsyncInitializeConfigForPlatforms()
{
    FPaths::ProjectDir();
    FPlatformMisc::GeneratedConfigDir();
    FConfigContext::EnsureRequiredGlobalPathsHaveBeenInitialized();
    FPlatformProcess::ApplicationSettingsDir();

    // Optional allow-list: when [Core.System] ConfigPlatformsToInclude=... is non-empty,
    // skip every platform not on the list. Default empty = include all (no behaviour change).
    TArray<FString> PlatformsToInclude;
    GConfig->GetArray(TEXT("Core.System"), TEXT("ConfigPlatformsToInclude"), PlatformsToInclude, GEngineIni);
    const auto ShouldInclude = [&PlatformsToInclude](FName PlatformName)
    {
        return PlatformsToInclude.Num() == 0 || PlatformsToInclude.Contains(PlatformName.ToString());
    };

    const TMap<FName, FDataDrivenPlatformInfo>& AllPlatformInfos = FDataDrivenPlatformInfoRegistry::GetAllPlatformInfos();
    for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
    {
        if (!ShouldInclude(Pair.Key)) continue;
        GetPlatformConfigFutures().Emplace(Pair.Key);
        ConfigForPlatform.Add(Pair.Key, new FConfigCacheIni(EConfigCacheType::Temporary, Pair.Key, true));
    }

    for (const TPair<FName, FDataDrivenPlatformInfo>& Pair : AllPlatformInfos)
    {
        if (!ShouldInclude(Pair.Key)) continue;
        FName PlatformName = Pair.Key;
        GetPlatformConfigFutures()[PlatformName] = Async(EAsyncExecution::ThreadPool, [PlatformName]
        {
            // ... existing body unchanged ...
        });
    }
}
```

`GConfig` is already populated by the time this function runs (the calling site at `ConfigCacheIni.cpp:5709-5715` is gated on `bIsReadyForUse` being set; `GConfig->GetArray` is a safe read at that point).

**MODIFY** `Config/DefaultEngine.ini` in this CkPlugins project (NOT in the engine fork):

```ini
[Core.System]
+ConfigPlatformsToInclude=Windows
```

The `+` prefix is the Unreal INI accumulate-into-array syntax. If a downstream consumer of the engine fork ever wants to include additional platforms (e.g. a Linux CI build), they add their own line to their project's `DefaultEngine.ini`.

This is the only project-side change in Phase 3. All other modifications are inside the engine fork.

## Out of Scope (Phase 3 deferred / out of campaign)

- Toolbox single-process flow (combining `Automation List` + `Automation RunTests` into one `-ExecCmds` chain). Not engine work; separate Phase 3 candidate per Phase 1 spec's Phase 2 Targets table.
- AS timing marker re-enable. Observability only, separate cycle.
- Wider tooltip port — hunting eager `SNew(SToolTip)` call sites beyond what UE 5.8 itself touched. Speculative on additional savings; grows maintenance cost.
- Hard-coding Windows-only directly in `AsyncInitializeConfigForPlatforms` (without the .ini opt-in). Rejected because it would force a backout if a future game project on this fork ever shipped to console/mobile.

## Components / Deliverables

Two deliverables, both engine-fork patches on `main-ck`. Land as two distinct commits but in the same rebuild + measurement cycle.

## Data Flow

### Tooltip path

No runtime data flow change. The lazy-construction pattern is transparent to consumers of `IToolTip`:

```
Before (UE 5.5):
  Widget Construct ─► .ToolTipText("...") ─► FSlateApplication::MakeToolTip
                                              └─► SNew(SToolTip).Text(...) ── eager allocation
                                                                              SCompoundWidget construct
                                                                              text run layout
                                                                              ~37999 more times

After (Phase 3):
  Widget Construct ─► .ToolTipText("...") ─► FSlateApplication::MakeToolTip
                                              └─► MakeShared<SDeferredToolTipText>(...) ── store TAttribute only
                                                                                          no widget allocation

  [User hovers a widget for the first time]
    SDeferredToolTipText::AsWidget() called
    └─► TryCacheToolTip() ─► SAssignNew(CachedToolTip, SToolTip).Text(ToolTipText) ── on-demand allocation, once
    └─► returns CachedToolTip->AsWidget()
```

### Platforms .ini path

```
Before (UE 5.5):
  AsyncInitializeConfigForPlatforms()
    for each of ~10 platforms in AllPlatformInfos:
      Async() ─► load <platform>.ini ── ~0.10 sec/each
    total: ~1.0-1.2 sec on the Engine init critical path

After (Phase 3):
  AsyncInitializeConfigForPlatforms()
    read [Core.System] ConfigPlatformsToInclude from GEngineIni ── one-time read
    for each platform in AllPlatformInfos:
      if not on allow-list: continue
      Async() ─► load Windows.ini ── ~0.10 sec
    total: ~0.10 sec (one platform load)
```

## Error Handling

### Rebuild fails

Most likely cause: missing include in one of the modified files, or a Slate / Core module API mismatch between 5.5 and 5.8 we missed in scoping. Recovery: read the build error, fix the include or adapt the API call, rebuild. If the failure is structural (e.g. `IToolTip` interface diverged in ways we didn't see, or `GConfig->GetArray` API surface changed), revert and abort.

### Measured saving below threshold

Pre-committed abort criteria, **per deliverable** (since the two commits are independent and individually revertable):

- **Tooltip backport** (Deliverable 1) rolls back if its incremental saving is < 1.0 sec on `engine_init_seconds` or `total_to_pie_ready_seconds`.
- **Platforms cull** (Deliverable 2) rolls back if its incremental saving is < 0.5 sec on the same metrics.

To attribute savings per deliverable: revert each commit individually and re-measure, or land each as a separate engine commit with a measurement snapshot between. The plan picks the cheapest attribution path (likely: land both, measure combined; if combined saving < 1.5 sec, revert one at a time to isolate which carries weight).

Document the negative result (and the per-deliverable attribution) in the spec's Phase 3 Results section before reverting.

### Tooltip visual regression

Risk: deferred construction changes the timing of tooltip widget construction. Edge cases that might surface — tooltips that rely on construction-time side effects, tooltips that read state captured at construction time and expect it to be frozen there rather than re-read at first access. UE 5.8 ships this pattern so Epic has presumably exercised these cases, but our fork may have widget code that doesn't appear in stock UE.

Mitigation: the IskmRenderer test set (19 tests) covers our gameplay/runtime path but not editor-tooltip rendering. Manual smoke after the patch: open the editor, hover over a few common tooltips (property row, toolbar button, content browser asset), confirm they appear. If any tooltip appears blank or shows stale text, that's a regression. Roll back.

### Windows-specific config in skipped platforms

Risk: an asset or system in the project transitively references a non-Windows platform's config (e.g. an `IniPlatformName` redirector that points at "Linux" or "Mac"). With the allow-list narrowed to Windows-only, `ConfigForPlatform.Find("Linux")` would miss and `FConfigCacheIni::ForPlatform("Linux")` would fall back to `GConfig` per its existing `else return GConfig;` branch (line 6446). The fallback is the existing safety valve — the engine doesn't crash on a missing platform cache; it returns the active config instead.

Mitigation: the IskmRenderer test set must still pass 19/19. If a system was relying on a non-Windows platform cache, it'll fail (or log warnings about missing settings) at test time. If a system silently falls back to `GConfig` and that's the wrong cache for it, the regression is harder to spot — but this is the same risk Epic ships with on shipping-only Windows builds today.

### `--build` itself fails after engine change

The engine rebuild step (`runreal build editor`) is separate from `UnrealToolbox.exe --build`. If `runreal build editor` produces a build but the toolbox `--build --test` cycle fails to launch the editor, the issue is project-side, not engine-side. Recovery: read `Saved/Logs/CkPlugins.log` for the launch failure; revert if root cause is the tooltip change.

## Testing

### Unit / functional

No new unit tests. The decision function in Phase 2 was small enough to pin with Catch2; this phase's change is engine-side widget plumbing that exercises through the editor.

### Smoke (manual, post-patch)

After engine rebuild and toolbox cycle:

**Tooltip smoke:**
1. Editor launches without crashing.
2. Open a Blueprint asset. Hover a graph node. Confirm tooltip appears with text.
3. Open Project Settings. Hover a property name in the right column. Confirm documentation tooltip appears with text and (if a documentation excerpt is wired) the larger documentation pane.
4. Hover a toolbar button. Confirm tooltip appears.

A blank tooltip, a stale tooltip, or a crash on hover means roll back the tooltip commit.

**Platforms cull smoke:**
5. Grep `Saved/Logs/CkPlugins.log` for `Loading .* ini files took` — should see only `Loading Windows ini files took` (or whichever platforms were on the allow-list). Absence of `IOS`, `Android`, `Mac`, `Linux`, `PS5`, etc. confirms the cull is active.
6. IskmRenderer 19/19 still passes — confirms no system was relying on a non-Windows platform config.

### IskmRenderer regression suite

19/19 must still pass after the engine change. Same fixture as Phase 1 and Phase 2. The IskmRenderer tests don't exercise editor tooltips, but they confirm the engine still boots cleanly and the gameplay/render path still works.

### Measurement

Capture before/after via `--measure`:
1. Fresh `Phase3-pre-baseline.startup.json` against the current engine (sanity baseline — should match Phase 2 final).
2. After both patches + rebuild: `Phase3-combined-postbuild.startup.json` (`--build --test` cycle).
3. After both patches: `Phase3-combined-cached.startup.json` (`--test` alone, cached path).
4. `--measure-compare` diffs:
   - Phase 2 postbuild vs Phase 3 combined postbuild — engine-init delta on first run.
   - Phase 2 cached vs Phase 3 combined cached — engine-init delta on iteration loop.
   - Confirms Phase 2's cached-path saving is preserved (iteration cycle should still drop ~32 sec from postbuild).

Expected: `engine_init_seconds` drops ~2-6 sec combined. `total_to_pie_ready_seconds` drops by roughly the same amount (both savings are inside engine init). The iteration-cycle saving from Phase 2 (-32.6 sec) remains intact, so the cached cycle wall-clock drops to roughly 25-29 sec total.

**Per-deliverable attribution (only if combined saving < 1.5 sec):** revert each engine commit individually and re-measure to identify which patch carried the weight. The plan section covers this fallback procedure; we don't run it speculatively.

## Success Criteria

Phase 3 ships when:

- Two engine-fork commits on `main-ck` carry the changes — one for the tooltip backport, one for the platforms cull — each with a descriptive message naming the patch.
- One CkPlugins commit (on `feature/generational-handle-migration`) adds the `+ConfigPlatformsToInclude=Windows` line to `Config/DefaultEngine.ini`.
- Engine rebuild cleanly produces a working editor (no Slate / Core module symbol errors, no link errors).
- IskmRenderer test set 19/19 passes from both post-build and cached toolbox cycles.
- Manual tooltip smoke (4 scenes) and platforms cull smoke (log grep + IskmRenderer) both pass.
- `--measure-compare` shows ≥ 1.5 sec combined improvement on `engine_init_seconds` or `total_to_pie_ready_seconds` against the Phase 2 baseline. Per-deliverable abort thresholds: tooltip ≥ 1.0 sec, platforms cull ≥ 0.5 sec. Any deliverable below its threshold is reverted on `main-ck`.
- Results recorded in this spec under a "Phase 3 Results" section before sign-off.

## Engine fork branching

Land the two changes as **two commits on `main-ck` directly** (no feature branch). `main-ck` is the working branch for the chainkemists fork. Phase 1 and Phase 2 didn't touch the engine fork, so these are the first engine patches on `main-ck` since the campaign began.

Commit titles:
- `feat(slate): backport SDeferredToolTip from UE 5.8`
- `feat(core): add ConfigPlatformsToInclude opt-in for editor platform .ini loading`

Each commit body names what it ports / modifies, links this Phase 3 spec, and (after sign-off) quotes the measured saving. Future rebases against newer UE versions can locate and drop these commits by searching for `SDeferredToolTip` / `ConfigPlatformsToInclude` in the commit log. The tooltip commit becomes a no-op rebase against UE 5.8+; the platforms cull is a permanent carry until Epic adopts a similar opt-in upstream.

## Sequencing

1. Capture fresh pre-patch `Phase3-pre-baseline.startup.json` (sanity baseline; should match Phase 2 final, but the noise floor matters for the per-deliverable attribution path).
2. Read our fork's current `SPropertyNameWidget::Construct` body to confirm the adaptation diff for `PropertyEditorHelpers.cpp` (Deliverable 1).
3. Apply Deliverable 1 (tooltip) as one commit on `main-ck`: `feat(slate): backport SDeferredToolTip from UE 5.8`.
4. Apply Deliverable 2 (platforms cull) as one commit on `main-ck`: `feat(core): add ConfigPlatformsToInclude opt-in for editor platform .ini loading`.
5. Apply the project-side `Config/DefaultEngine.ini` opt-in line on `feature/generational-handle-migration` in CkPlugins.
6. `runreal build editor` to rebuild.
7. Capture post-patch `--measure` snapshots (post-build cycle + cached cycle).
8. Manual smoke: tooltip scenes (4), platforms log-grep (1), IskmRenderer (auto-runs).
9. `--measure-compare` against Phase 2 baselines.
10. Decision gate:
    - Combined saving ≥ 1.5 sec → record results, push engine fork + CkPlugins, sign off.
    - Combined saving 1.0-1.5 sec → run per-deliverable attribution: revert tooltip commit, rebuild, measure; if platforms cull saving < 0.5 sec, revert that too. Push whichever survives.
    - Combined saving < 1.0 sec → revert both commits on `main-ck`, document negative result, abandon Phase 3.

## Phase 3 Results (filled in on completion)

(Placeholder section. Implementer fills with real numbers from the measurement step or removes if the patch was rolled back.)
