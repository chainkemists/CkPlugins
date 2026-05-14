# Editor Startup — Phase 3: SDeferredToolTip Backport

**Date:** 2026-05-13
**Author:** Sulfur-CK (with assistance)
**Status:** Approved for implementation planning

**Prior phases:**
- [Phase 1](2026-05-13-editor-startup-phase1-design.md) — measurement infrastructure + project-side plugin cull. Saved -1.339s on `engine_init_seconds`.
- [Phase 2](2026-05-13-toolbox-discovery-cache-design.md) — toolbox test-discovery cache. Saved -32.944s on the iteration cycle (`--test` alone, cached).

---

## Goal

Reduce per-process editor cold-start by 1-5 sec by porting Epic Games' UE 5.8 `SDeferredToolTip` / `SDeferredToolTipText` widgets into our UE 5.5 fork at `D:\Repos\UnrealEngineCk` (branch `main-ck`). The shipped UE 5.8 change defers tooltip widget construction from "every widget at construction time" (~38000 eager `SNew(SToolTip)` invocations during editor init in DebugGame) to "first tooltip access". Per Epic's measurements and the source article (https://larstofus.com/2025/09/02/speeding-up-the-unreal-editor-launch-by-not-spawning-38000-tooltips/), this saves 2-5 sec in Debug, ~1 sec in Development, and ~40 MB RAM.

**Baseline metric:** `total_to_pie_ready_seconds` and `engine_init_seconds` from the `--measure` infrastructure, compared against Phase 2's final snapshots:
- `D:\Repos\CkPlugins\Saved\Logs\Phase2-final-postbuild.startup.json` (post-build cycle baseline).
- `D:\Repos\CkPlugins\Saved\Logs\Phase2-final-cached.startup.json` (iteration cycle baseline).

The toolbox layer doesn't change in this phase — the same `UnrealToolbox.exe` from Phase 2 runs against a rebuilt engine and the saving shows up in both cycles (since both spawn an editor that has to construct toolbar/menu widgets eagerly today).

## Current State

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

## In Scope (Phase 3)

One deliverable, four files in `D:\Repos\UnrealEngineCk` (branch `main-ck`).

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

## Out of Scope (Phase 3 deferred / out of campaign)

- Platforms `.ini` cull (UE 5.5's `FConfigCacheIni::AsyncInitializeConfigForPlatforms` walking every platform unconditionally). Explicitly dropped per user decision — engine-fork carry not worth the ~1.2 sec saving for this campaign cycle.
- Toolbox single-process flow (combining `Automation List` + `Automation RunTests` into one `-ExecCmds` chain). Not engine work; separate Phase 3 candidate per Phase 1 spec's Phase 2 Targets table.
- AS timing marker re-enable. Observability only, separate cycle.
- Wider port — hunting eager `SNew(SToolTip)` call sites beyond what UE 5.8 itself touched. Speculative on additional savings; grows maintenance cost.

## Components / Deliverables

Single deliverable, atomic. The four-file change is one engine-fork commit on `main-ck`. Rebuild → measure → sign off in one cycle.

## Data Flow

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

## Error Handling

### Rebuild fails

Most likely cause: missing include in one of the modified files, or a Slate module API mismatch between 5.5 and 5.8 we missed in scoping. Recovery: read the build error, fix the include or adapt the API call, rebuild. If the failure is structural (e.g. `IToolTip` interface diverged in ways we didn't see), revert and abort.

### Measured saving < 1 sec

Pre-committed abort criterion. Roll back the engine change (single commit on `main-ck`). The engine-fork carry cost of a 0.5 sec saving doesn't justify maintenance across UE version bumps and rebases. Document the negative result in the spec's Phase 3 Results section before reverting.

### Tooltip visual regression

Risk: deferred construction changes the timing of tooltip widget construction. Edge cases that might surface — tooltips that rely on construction-time side effects, tooltips that read state captured at construction time and expect it to be frozen there rather than re-read at first access. UE 5.8 ships this pattern so Epic has presumably exercised these cases, but our fork may have widget code that doesn't appear in stock UE.

Mitigation: the IskmRenderer test set (19 tests) covers our gameplay/runtime path but not editor-tooltip rendering. Manual smoke after the patch: open the editor, hover over a few common tooltips (property row, toolbar button, content browser asset), confirm they appear. If any tooltip appears blank or shows stale text, that's a regression. Roll back.

### `--build` itself fails after engine change

The engine rebuild step (`runreal build editor`) is separate from `UnrealToolbox.exe --build`. If `runreal build editor` produces a build but the toolbox `--build --test` cycle fails to launch the editor, the issue is project-side, not engine-side. Recovery: read `Saved/Logs/CkPlugins.log` for the launch failure; revert if root cause is the tooltip change.

## Testing

### Unit / functional

No new unit tests. The decision function in Phase 2 was small enough to pin with Catch2; this phase's change is engine-side widget plumbing that exercises through the editor.

### Smoke (manual, post-patch)

After engine rebuild and toolbox cycle:
1. Editor launches without crashing.
2. Open a Blueprint asset. Hover a graph node. Confirm tooltip appears with text.
3. Open Project Settings. Hover a property name in the right column. Confirm documentation tooltip appears with text and (if a documentation excerpt is wired) the larger documentation pane.
4. Hover a toolbar button. Confirm tooltip appears.

A blank tooltip, a stale tooltip, or a crash on hover means roll back.

### IskmRenderer regression suite

19/19 must still pass after the engine change. Same fixture as Phase 1 and Phase 2. The IskmRenderer tests don't exercise editor tooltips, but they confirm the engine still boots cleanly and the gameplay/render path still works.

### Measurement

Capture before/after via `--measure`:
1. Fresh `Phase3-pre-tooltip-baseline.startup.json` against the current engine (sanity baseline — should match Phase 2 final).
2. After patch + rebuild: `Phase3-tooltip-postbuild.startup.json` (`--build --test` cycle).
3. After patch: `Phase3-tooltip-cached.startup.json` (`--test` alone, cached path).
4. Three `--measure-compare` diffs:
   - Phase 2 postbuild vs Phase 3 postbuild — engine-init delta on first run.
   - Phase 2 cached vs Phase 3 cached — engine-init delta on iteration loop.
   - Phase 2 cached vs Phase 3 cached — confirms Phase 2's cached-path saving is preserved.

Expected: `engine_init_seconds` drops 1-5 sec. `total_to_pie_ready_seconds` drops by roughly the same amount (since the tooltip cost is inside engine init). The iteration-cycle saving from Phase 2 (-32.6 sec) remains intact, so the cached cycle wall-clock drops to roughly 27-30 sec total.

## Success Criteria

Phase 3 ships when:

- Engine fork commit on `main-ck` carries the four-file change with a descriptive message naming the patch.
- Engine rebuild cleanly produces a working editor (no Slate module symbol errors, no link errors).
- IskmRenderer test set 19/19 passes from both post-build and cached toolbox cycles.
- Manual tooltip smoke (4 scenes above) shows tooltips work as before — no blanks, no stales, no crashes.
- `--measure-compare` shows ≥ 1.0 sec improvement on `engine_init_seconds` or `total_to_pie_ready_seconds` against the Phase 2 baseline. If the measured saving is below this threshold, the patch is rolled back (pre-committed abort criterion).
- Results recorded in this spec under a "Phase 3 Results" section before sign-off.

## Engine fork branching

Land the four-file change as **one commit on `main-ck` directly** (no feature branch). `main-ck` is the working branch for the chainkemists fork. Phase 1 and Phase 2 didn't touch the engine fork, so this is the first engine patch on `main-ck` since the campaign began.

Commit title: `feat(slate): backport SDeferredToolTip from UE 5.8`. The commit body names what it ports, what it modifies, links the Phase 3 spec, and quotes the measured saving. Future rebases against newer UE versions can locate and (when 5.8+) drop this commit by searching for "SDeferredToolTip" in the commit log.

## Sequencing

1. Read our fork's current `SPropertyNameWidget::Construct` body to confirm the adaptation diff for `PropertyEditorHelpers.cpp`.
2. Apply the four-file change in one engine commit on `main-ck`.
3. `runreal build editor` to rebuild.
4. Capture post-patch `--measure` snapshots (post-build cycle + cached cycle).
5. Manual tooltip smoke (4 scenes).
6. `--measure-compare` against Phase 2 baselines.
7. Decision gate: ≥ 1 sec saving → record results, push engine fork, sign off. < 1 sec → roll back commit on `main-ck`, document negative result, abandon.

## Phase 3 Results (filled in on completion)

(Placeholder section. Implementer fills with real numbers from the measurement step or removes if the patch was rolled back.)
