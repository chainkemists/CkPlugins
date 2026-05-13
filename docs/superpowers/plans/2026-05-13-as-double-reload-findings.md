# AS Double-Reload Investigation Findings

**Date:** 2026-05-13
**Author:** subagent (Task 10)
**Outcome:** (d) — not an AS plugin issue; the toolbox `--test` mode launches the editor twice (discovery + run), and AS legitimately initializes once per process.

## Evidence from log

Log file: `D:\Repos\CkPlugins\Saved\Logs\BuildTest-bundle.log`

Pass A: log lines 230–232 at `2026.05.13-19.55.27:984` — surrounding context: first editor process; toolbox phase `=== UnrealToolbox --test === Discovering tests…` running `-ExecCmds="Automation List; Quit"` (line 75 commandline). `Engine is initialized` for this process at line 2319 (`19:55:36`), and `**** TEST COMPLETE. EXIT CODE: 0 ****` at line 9550 (`19:55:51`).

Pass B: log lines 9722–9724 at `2026.05.13-19.56.00:026` — surrounding context: a SECOND editor process; `Running 19 tests…` banner at line 9551 immediately after the previous process exited. Fresh `LogInit: Display: Running engine for game: CkPlugins` at line 9552 with a new commandline `-ExecCmds="Automation RunTests Project.Functional Tests....Ck_AutoTest_IskmRenderer_*; Quit"` (line 9573). `Engine is initialized` for this process at line 11811 (`19:56:09`).

Time between passes: `19:56:00.026 - 19:55:27.984` = **32.04 seconds** (well over the cache miss window — but this is wall-time across two distinct processes, not a single process re-doing work).

Other corroborating counts in the same log:
- `Running engine for game: CkPlugins` appears **2** times (line 60, line 9552).
- `Engine is initialized. Leaving FEngineLoop::Init()` appears **2** times (line 2319, line 11811).
- `Angelscript root path:` appears **6** times = 3 roots × 2 processes (exactly one init per process).

Same pattern reproduces in `BuildTest-baseline.log` (6 root-path lines, 2 process starts). In a hand-run `BuildTest-IskmRenderer.log` that did not use the toolbox `--test` flow, there are 0 of each (different run mode).

## Pass A trigger
- Trigger site: `D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\AngelscriptManager.cpp:924` (the only `Angelscript root path:` call site in the entire AS plugin).
- Containing function: `FAngelscriptManager::InitialCompile()` at line 893, called once from `FAngelscriptManager::Startup()` at line 491.
- Delegate / call site: standard module startup path inside the first editor process. The toolbox `--test` step launches this process with `-ExecCmds="Automation List; Quit"` to enumerate available automation tests.
- Work performed: full AS initialization — `MakeAllScriptRoots()`, preprocessor file scan, class generator, JIT compile, binding scan. The full AS bring-up.

## Pass B trigger
- Trigger site: same — `AngelscriptManager.cpp:924`, same `InitialCompile()`.
- Delegate / call site: same `Startup()` path, but inside a **separate, freshly-spawned editor process**. The toolbox spawns this second process after the first exits with code 0, this time with `-ExecCmds="Automation RunTests <selected tests>; Quit"` to actually execute the discovered tests.
- Work performed: identical to Pass A.

## Are the two passes doing the same work?

**Yes — full duplicate.** Both passes do identical work, because they are the same code path running in two separate OS processes. The duplication is at the process-launch layer, not inside the AS plugin.

## Root cause

The CkPlugins `UnrealToolbox.exe --test` flow is a **two-phase harness**: (1) a discovery phase that launches the editor with `Automation List; Quit` to enumerate available tests, and (2) a run phase that launches the editor again with `Automation RunTests <list>; Quit` to execute them. The AS plugin initializes exactly once per editor process (`FAngelscriptManager::Startup` → `InitialCompile`), so two editor processes naturally produce two AS init passes. There is no redundant work happening inside a single process.

## Decision

**Defer to Phase 2.** This is not an AS plugin bug — `InitialCompile()` runs once per `Startup()`, and `Startup()` runs once per process. There is nothing to fix in `D:\Repos\UnrealEngineAngelscript`. The duplicated AS init that the optimization plan is targeting is actually two complete editor cold-starts driven by the toolbox.

The original Task 10 hypothesis (a single editor process re-running AS reload twice via a delegate) does not match the evidence. The May-5 baseline that reportedly showed timing markers like `class generator reload took 1500ms` (twice) was almost certainly also two processes — that earlier analysis appears to have missed the duplicate `Running engine for game` and `Engine is initialized` markers.

## Phase 2 shape (not landed here)

The real optimization opportunity, if any, is at the **toolbox harness** level, not the AS plugin:

1. **Cache the discovery output** — if the set of selected tests does not change between runs (e.g. the user is iterating on a known fixed test pattern like `IskmRenderer`), the toolbox can skip the discovery phase entirely. The current run already paid for two ~9-second AS cold-starts; one of them is purely to list tests.
2. **Single-process flow** — combine `Automation List` + `Automation RunTests` into one `-ExecCmds` chain so we pay AS init once. Requires that the test-selection logic be expressible without inspecting the list first (e.g. just pass the user-supplied pattern straight through to `Automation RunTests`).
3. **Trust the test-pattern flag** — when the user passed `--test-pattern IskmRenderer`, we already know the filter; we don't need to enumerate every test in every module to apply it. Discovery is only required if we need to *display* the list back to the user or validate a filter.

Each of these is a toolbox change (likely .ts / .rs / .cs depending on toolbox impl), not an engine change. Out of scope for Task 10's "≤50 lines of engine code" budget.

## Why no Phase 1 fix landed

- The AS plugin code is already correct — one `InitialCompile()` per process is the right behavior. Adding a process-global guard (e.g. `static bool bReloadAlreadyRanThisSession`) inside the AS plugin would do nothing, because each process has its own static storage; the second process starts fresh.
- The actual remedy lives in the toolbox harness (test runner) which is not part of the AS plugin and not part of the engine.
- No single ≤50-line engine edit eliminates the second editor process.

## Recommended action

1. Land this findings doc in CkPlugins. (Done by Task 10.)
2. Open a Phase 2 task scoped at the toolbox: "Eliminate the test-discovery editor launch when `--test-pattern` is supplied".
3. Stop describing the symptom as "AS double-reload" in future planning; the accurate name is "toolbox `--test` runs two editor cold-starts".
