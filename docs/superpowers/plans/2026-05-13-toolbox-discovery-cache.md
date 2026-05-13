# Toolbox Test-Discovery Cache (Phase 2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate the ~17 second test-discovery editor cold-start when the toolbox `--test` mode iterates on a known build. Cache the discovered test list in the existing `Settings.Tests.DiscoveredTests` field; re-discover only when `--build` ran in the same invocation, `--discover-fresh` was passed, or the cache is empty (first run).

**Architecture:** Pure additive change inside `D:\Repos\FtxUiFramework\apps\UnrealToolbox\`. One new CLI flag (`--discover-fresh`), one new pure decision function (`Decide_TestDiscoveryMode`) with Catch2 tests, and a four-way branch replacing the always-discover block in `RunHeadlessTest`. No engine fork, no submodule changes, no new third-party deps.

**Tech Stack:** C++23, CLI11 (CLI parser), nlohmann::json (settings serialisation — pre-existing), Catch2 (test framework). Build via `cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8`. Auto-deploys to `D:\ConsoleApps\UnrealToolbox.exe` via `ftx_deploy_app()`. CkAuto-deployed copy must be refreshed manually after rebuild (verified during Phase 1: `CkAuto/UnrealToolbox.exe` is a separate static copy in the `CkAuto` submodule, not a symlink).

**Spec:** [docs/superpowers/specs/2026-05-13-toolbox-discovery-cache-design.md](../specs/2026-05-13-toolbox-discovery-cache-design.md)

**Prior phase:** Phase 1 shipped `--measure` / `--measure-compare` (used here to validate savings). Phase 1 commits on `feature/cliseq` ended at `9a5955da` (FtxUiFramework). The toolbox in CkAuto was pushed at CkAuto commit `b6caa3b`, and CkPlugins's submodule pointer was bumped at CkPlugins commit `7660c3c`.

---

## File Structure

**Modified (in `D:\Repos\FtxUiFramework`):**
- `apps/UnrealToolbox/include/utb/CliArgs.hpp` — add `DiscoverFresh` field.
- `apps/UnrealToolbox/src/CliArgs.cpp` — register `--discover-fresh` flag with `needs(TestOpt)`.
- `apps/UnrealToolbox/include/utb/TestRunner.hpp` — add `ETestDiscoveryMode` enum, `FTestDiscoveryInputs` struct, `Decide_TestDiscoveryMode` declaration.
- `apps/UnrealToolbox/src/TestRunner.cpp` — implement `Decide_TestDiscoveryMode`.
- `apps/UnrealToolbox/src/UnrealToolbox_Main.cpp` — replace the always-discover block in `RunHeadlessTest` with a decision-driven branch.
- `apps/UnrealToolbox/CMakeLists.txt` — register the new Catch2 test file in `_UTB_TEST_FILES`.

**Created (in `D:\Repos\FtxUiFramework`):**
- `tests/UnrealToolbox_TestDiscoveryDecision.cpp` — five Catch2 cases pinning the decision tree.

**No CkPlugins changes** during Tasks 1-5 (toolbox-only work). Task 6 records measurements + bumps the CkAuto submodule pointer in CkPlugins.

---

## Working-directory note for agentic workers

Two repos are touched:
- **Toolbox source + tests:** `D:\Repos\FtxUiFramework` (branch `feature/cliseq`).
- **Measurement / submodule bump:** `D:\Repos\CkPlugins` (branch `feature/generational-handle-migration`).

Most steps run in `FtxUiFramework`. Use `git -C /d/Repos/FtxUiFramework` for those. Don't `cd` mid-step (it's brittle in PowerShell + bash interop); always use absolute paths or `git -C`.

Editor lock check (before any toolbox `--test` cycle) — the standard CkPlugins helper:

```bash
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
```

If the result is `locked`, another editor session is running; wait it out before invoking the toolbox.

---

## Task 1: Add `--discover-fresh` CLI flag

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\include\utb\CliArgs.hpp`
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\CliArgs.cpp`

- [ ] **Step 1: Add the `DiscoverFresh` field**

Open `CliArgs.hpp`. Find the `--test sub-flags` region (above the `--measure` / `--measure-compare` block added in Phase 1). After the existing `bool IncludeEngine = false;` line, add:

```cpp
// --discover-fresh: sub-flag of --test. Forces test discovery to re-run
// even if a cached test list exists. Use after manually adding/removing
// tests without a --build, or to reset a stale cache.
bool DiscoverFresh = false;
```

- [ ] **Step 2: Register the flag with CLI11**

In `CliArgs.cpp`, locate the existing `--measure` block (registered with `needs(TestOpt)` in Phase 1's commit `9494df9`). Immediately after it, add the parallel `--discover-fresh` registration:

```cpp
auto* DiscoverFreshOpt = App.add_flag(
    "--discover-fresh", Out.DiscoverFresh,
    "With --test: force a fresh discovery editor launch even if a cached "
    "test list exists. Use after adding or removing tests without a "
    "--build, or to reset a stale cache.");
DiscoverFreshOpt->needs(TestOpt);
```

(`TestOpt` is a local already in scope from earlier in `Parse_Args`. Don't re-declare it.)

- [ ] **Step 3: Build to confirm the flag parses**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8
```

Expected: clean build. Then copy the rebuilt binary into CkAuto (CkAuto/UnrealToolbox.exe is a separate static copy — Phase 1 confirmed this):

```bash
cp "D:/Repos/FtxUiFramework_Build/apps/UnrealToolbox/Debug/UnrealToolbox.exe" "D:/Repos/CkPlugins/CkAuto/UnrealToolbox.exe"
```

- [ ] **Step 4: Verify the flag appears in `--help` and respects the `--test` requirement**

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --help 2>&1 | grep -E "discover-fresh|measure"
```

Expected: a line containing `--discover-fresh` with `Needs: --test` annotation, plus the existing `--measure` lines.

Verify the `needs(TestOpt)` constraint is enforced:

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --discover-fresh 2>&1 | head -5
```

Expected: CLI11 error containing "--discover-fresh requires --test" (or equivalent CLI11 wording).

- [ ] **Step 5: Commit**

```bash
git -C /d/Repos/FtxUiFramework add apps/UnrealToolbox/include/utb/CliArgs.hpp apps/UnrealToolbox/src/CliArgs.cpp
git -C /d/Repos/FtxUiFramework commit -m "feat(UnrealToolbox): add --discover-fresh CLI flag

Sub-flag of --test that forces a fresh discovery editor launch even
when Settings.Tests.DiscoveredTests is non-empty. Stores into
FCliArgs.DiscoverFresh; consumed by RunHeadlessTest in a later task.
CLI surface only — no behaviour change in this commit."
```

---

## Task 2: Add the decision-function API (header)

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\include\utb\TestRunner.hpp`

- [ ] **Step 1: Add `ETestDiscoveryMode`, `FTestDiscoveryInputs`, and `Decide_TestDiscoveryMode` declaration**

In `TestRunner.hpp`, after the existing `FTestRunResult` struct (around line 40) and before the `ResolveEditorExe` declaration, add:

```cpp
// --------------------------------------------------------------------------------------------------------------------
// Discovery cache decision
//
// The toolbox --test flow can either run a fresh discovery editor (paying
// the ~17 sec cold-start) or reuse the cached list at Settings.Tests.
// DiscoveredTests. The decision rule is pure — it depends only on three
// booleans pulled from FCliArgs and FSettings, with no side effects.
//
// Extracted out of RunHeadlessTest so the four-way branch is verifiable
// in unit tests without spinning up an editor.

enum class ETestDiscoveryMode { UseCache, DiscoverFresh };

struct FTestDiscoveryInputs
{
    bool DiscoverFreshRequested = false;  // FCliArgs.DiscoverFresh
    bool BuildJustRan           = false;  // FCliArgs.Build (build runs before test in main())
    bool CacheEmpty             = false;  // Settings.Tests.DiscoveredTests.empty()
};

// Returns DiscoverFresh when ANY input is true; UseCache only when all
// three are false.
auto Decide_TestDiscoveryMode(const FTestDiscoveryInputs& In) -> ETestDiscoveryMode;
```

- [ ] **Step 2: Build to verify the header compiles**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8
```

Expected: clean build. The header is consumed by `TestRunner.cpp` and (after the next task) `UnrealToolbox_Main.cpp`, but at this commit only the declarations exist; nothing references them yet.

- [ ] **Step 3: Commit**

```bash
git -C /d/Repos/FtxUiFramework add apps/UnrealToolbox/include/utb/TestRunner.hpp
git -C /d/Repos/FtxUiFramework commit -m "feat(UnrealToolbox): declare Decide_TestDiscoveryMode + inputs

Pure-function API: takes three independent boolean inputs (DiscoverFresh
flag, BuildJustRan, CacheEmpty) and returns ETestDiscoveryMode. Will be
implemented in the next task; tested in the task after that; consumed by
RunHeadlessTest in the task after that."
```

---

## Task 3: Pin the decision contract with Catch2 tests (failing first)

**Files:**
- Create: `D:\Repos\FtxUiFramework\tests\UnrealToolbox_TestDiscoveryDecision.cpp`
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\CMakeLists.txt`

This task is TDD red — write the failing test and confirm it fails because `Decide_TestDiscoveryMode` isn't implemented yet (it's only declared from Task 2). Task 4 implements the function and the tests turn green.

- [ ] **Step 1: Create the Catch2 test file**

Create `D:\Repos\FtxUiFramework\tests\UnrealToolbox_TestDiscoveryDecision.cpp`:

```cpp
// Tests for utb::Decide_TestDiscoveryMode — the pure-function decision
// that gates whether RunHeadlessTest spawns the discovery editor or
// reuses Settings.Tests.DiscoveredTests.

#include "utb/TestRunner.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Decide_TestDiscoveryMode returns UseCache when all three inputs are false",
          "[utb][TestDiscoveryDecision]")
{
    const auto In = utb::FTestDiscoveryInputs{
        .DiscoverFreshRequested = false,
        .BuildJustRan           = false,
        .CacheEmpty             = false,
    };
    REQUIRE(utb::Decide_TestDiscoveryMode(In) == utb::ETestDiscoveryMode::UseCache);
}

TEST_CASE("Decide_TestDiscoveryMode returns DiscoverFresh when --discover-fresh is set",
          "[utb][TestDiscoveryDecision]")
{
    const auto In = utb::FTestDiscoveryInputs{
        .DiscoverFreshRequested = true,
        .BuildJustRan           = false,
        .CacheEmpty             = false,
    };
    REQUIRE(utb::Decide_TestDiscoveryMode(In) == utb::ETestDiscoveryMode::DiscoverFresh);
}

TEST_CASE("Decide_TestDiscoveryMode returns DiscoverFresh when --build ran",
          "[utb][TestDiscoveryDecision]")
{
    const auto In = utb::FTestDiscoveryInputs{
        .DiscoverFreshRequested = false,
        .BuildJustRan           = true,
        .CacheEmpty             = false,
    };
    REQUIRE(utb::Decide_TestDiscoveryMode(In) == utb::ETestDiscoveryMode::DiscoverFresh);
}

TEST_CASE("Decide_TestDiscoveryMode returns DiscoverFresh when cache is empty",
          "[utb][TestDiscoveryDecision]")
{
    const auto In = utb::FTestDiscoveryInputs{
        .DiscoverFreshRequested = false,
        .BuildJustRan           = false,
        .CacheEmpty             = true,
    };
    REQUIRE(utb::Decide_TestDiscoveryMode(In) == utb::ETestDiscoveryMode::DiscoverFresh);
}

TEST_CASE("Decide_TestDiscoveryMode returns DiscoverFresh when all three inputs are true",
          "[utb][TestDiscoveryDecision]")
{
    // Sanity / belt-and-suspenders: simultaneous triggers must not produce
    // a different result than any single trigger alone.
    const auto In = utb::FTestDiscoveryInputs{
        .DiscoverFreshRequested = true,
        .BuildJustRan           = true,
        .CacheEmpty             = true,
    };
    REQUIRE(utb::Decide_TestDiscoveryMode(In) == utb::ETestDiscoveryMode::DiscoverFresh);
}
```

- [ ] **Step 2: Register the test file in CMake**

Open `D:\Repos\FtxUiFramework\apps\UnrealToolbox\CMakeLists.txt`. Find the existing `_UTB_TEST_FILES` block (added in Phase 1 Task 3). Add the new file:

```cmake
set(_UTB_TEST_FILES
    "${CMAKE_SOURCE_DIR}/tests/UnrealToolbox_CliArgs.cpp"
    "${CMAKE_SOURCE_DIR}/tests/UnrealToolbox_TestClassification.cpp"
    "${CMAKE_SOURCE_DIR}/tests/UnrealToolbox_StartupMeasurement.cpp"
    "${CMAKE_SOURCE_DIR}/tests/UnrealToolbox_TestDiscoveryDecision.cpp"
)
```

No new compile-definition needed — the new test file has no fixture data; everything is inline boolean inputs.

- [ ] **Step 3: Build and run the test — confirm it FAILS at link time**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target FtxUnitTests --parallel 8
```

Expected: **link error** mentioning unresolved external symbol `utb::Decide_TestDiscoveryMode`. The function is declared (Task 2) but not implemented. This is the TDD red phase.

If the build instead succeeds (because Catch2 doesn't link unused symbols, or some other linker quirk), run the tests:

```bash
/d/Repos/FtxUiFramework_Build/ftxc/FtxUnitTests "[TestDiscoveryDecision]"
```

Expected: test failure (the function returns garbage or crashes).

Either failure mode is acceptable — the point is the tests don't pass yet.

- [ ] **Step 4: Commit (red test only)**

```bash
git -C /d/Repos/FtxUiFramework add tests/UnrealToolbox_TestDiscoveryDecision.cpp apps/UnrealToolbox/CMakeLists.txt
git -C /d/Repos/FtxUiFramework commit -m "test(UnrealToolbox): pin Decide_TestDiscoveryMode contract (failing)

Five Catch2 cases enumerate the decision tree:
  - all-false  -> UseCache
  - DiscoverFreshRequested only -> DiscoverFresh
  - BuildJustRan only            -> DiscoverFresh
  - CacheEmpty only              -> DiscoverFresh
  - all-true (sanity)            -> DiscoverFresh

Tests fail at this commit because Decide_TestDiscoveryMode is declared
but not implemented. Task 4 implements it; tests turn green."
```

---

## Task 4: Implement `Decide_TestDiscoveryMode`

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\TestRunner.cpp`

- [ ] **Step 1: Implement the function**

In `TestRunner.cpp`, at the END of the existing `namespace utb {` block (after `RunTests`, before the closing `} // namespace utb`), add:

```cpp
auto
    Decide_TestDiscoveryMode(
        const FTestDiscoveryInputs& In)
    -> ETestDiscoveryMode
{
    // Any single trigger forces a fresh discovery. UseCache is the
    // "everything is normal" branch — and it's the only one that
    // captures the ~17 sec saving the cache exists for.
    if (In.DiscoverFreshRequested) return ETestDiscoveryMode::DiscoverFresh;
    if (In.BuildJustRan)           return ETestDiscoveryMode::DiscoverFresh;
    if (In.CacheEmpty)             return ETestDiscoveryMode::DiscoverFresh;
    return ETestDiscoveryMode::UseCache;
}
```

- [ ] **Step 2: Build and run all five tests — they must pass**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target FtxUnitTests --parallel 8
/d/Repos/FtxUiFramework_Build/ftxc/FtxUnitTests "[TestDiscoveryDecision]"
```

Expected: `5 passed in N test cases`. All five cases green.

- [ ] **Step 3: Commit**

```bash
git -C /d/Repos/FtxUiFramework add apps/UnrealToolbox/src/TestRunner.cpp
git -C /d/Repos/FtxUiFramework commit -m "feat(UnrealToolbox): implement Decide_TestDiscoveryMode

Pure-function dispatch: any of the three triggers (DiscoverFreshRequested,
BuildJustRan, CacheEmpty) forces DiscoverFresh; UseCache is the all-three-
false branch. 5/5 [TestDiscoveryDecision] Catch2 tests pass."
```

---

## Task 5: Wire the decision into `RunHeadlessTest`

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\UnrealToolbox_Main.cpp`

This is the user-facing behaviour change. The five Catch2 tests from Task 3-4 cover the pure decision logic; this task wires it into the real flow.

- [ ] **Step 1: Locate the always-discover block to replace**

In `UnrealToolbox_Main.cpp`, find the `RunHeadlessTest` function. Look for the section labelled `// 3. Discover (always re-discover for headless — keeps the cache fresh)`. It currently reads:

```cpp
    // 3. Discover (always re-discover for headless — keeps the cache fresh)
    Sink("Discovering tests\xe2\x80\xa6");
    auto Disc = utb::DiscoverTests(Engine, Project, Settings.Tests, Settings.Build.Config, Sink);
    if (NOT Disc.Success)
    {
        Sink("Discovery failed: " + Disc.ErrorMessage);
        return 1;
    }
    Settings.Tests.DiscoveredTests = Disc.DottedPaths;
    Settings.Tests.LastDiscoveredUnix = static_cast<int64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
```

That's the block to replace.

- [ ] **Step 2: Replace with decision-driven flow**

Replace the entire block above with:

```cpp
    // 3. Decide: discover fresh or reuse cached test list?
    //    Cache invalidation rule: re-discover only when --build ran in this
    //    invocation, --discover-fresh is set, or cache is empty (first run).
    const auto DecisionInputs = utb::FTestDiscoveryInputs{
        .DiscoverFreshRequested = InArgs.DiscoverFresh,
        .BuildJustRan           = InArgs.Build,
        .CacheEmpty             = Settings.Tests.DiscoveredTests.empty(),
    };
    const auto Decision = utb::Decide_TestDiscoveryMode(DecisionInputs);

    if (Decision == utb::ETestDiscoveryMode::DiscoverFresh)
    {
        // Build a human-readable reason for the log line — load-bearing
        // for post-hoc debugging of "why did this run discover?".
        auto Reason = std::string{};
        if      (InArgs.DiscoverFresh)                    Reason = "--discover-fresh requested";
        else if (InArgs.Build)                            Reason = "--build in this invocation; test list may have changed";
        else                                              Reason = "no cached test list";

        Sink("Discovering tests\xe2\x80\xa6 (" + Reason + ")");
        auto Disc = utb::DiscoverTests(Engine, Project, Settings.Tests, Settings.Build.Config, Sink);
        if (NOT Disc.Success)
        {
            Sink("Discovery failed: " + Disc.ErrorMessage);
            return 1;
        }
        Settings.Tests.DiscoveredTests = Disc.DottedPaths;
        Settings.Tests.LastDiscoveredUnix = static_cast<int64_t>(
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }
    else
    {
        Sink("Using cached test list (" +
             std::to_string(Settings.Tests.DiscoveredTests.size()) + " tests)");
    }
```

The block below this (`// 4. Resolve test set` and onward) is unchanged. `ResolveTestsToRun` reads `Settings.Tests.DiscoveredTests`, which in the UseCache branch is the previously-saved list and in the DiscoverFresh branch was just refreshed.

- [ ] **Step 3: Build and verify it compiles**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8
```

Expected: clean build. Then deploy:

```bash
cp "D:/Repos/FtxUiFramework_Build/apps/UnrealToolbox/Debug/UnrealToolbox.exe" "D:/Repos/CkPlugins/CkAuto/UnrealToolbox.exe"
```

- [ ] **Step 4: Smoke-test the cache-empty branch (fresh state)**

This sub-test confirms first-run behaviour. We need an empty cache, so first wipe the discovered tests field from `Settings.json`. Easiest: edit by hand or use `jq`.

Find the settings file:

```bash
ls D:/Repos/CkPlugins/Saved/UtbSettings*.json 2>/dev/null
# OR:
ls "$env:LOCALAPPDATA/UnrealToolbox/Settings.json" 2>/dev/null
```

(Path discovery during implementation — `FSettingsManager` decides the actual location. Look for "DiscoveredTests" or "Tests" in the file; the directory will be obvious.)

Open the settings file and either delete the `DiscoveredTests` array or replace it with `[]`. Save.

Editor lock check, then run:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase2-cache-empty.log --project='D:\Repos\CkPlugins'"
```

Inspect the log:

```bash
grep -nE "Discovering tests|Using cached test list" /d/Repos/CkPlugins/Saved/Logs/Phase2-cache-empty.log
```

Expected: a single line containing `Discovering tests… (no cached test list)`. The cache-empty trigger fired.

Confirm 19/19 IskmRenderer tests passed:

```bash
grep "=== Test summary ===" -A 4 /d/Repos/CkPlugins/Saved/Logs/Phase2-cache-empty.log
```

Expected: `Total: 19`, `Passed: 19`, `Failed: 0`.

- [ ] **Step 5: Smoke-test the UseCache branch (iteration)**

Re-run the same command (without `--build` and without wiping the cache this time):

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase2-cached.log --project='D:\Repos\CkPlugins'"
```

Inspect:

```bash
grep -nE "Discovering tests|Using cached test list" /d/Repos/CkPlugins/Saved/Logs/Phase2-cached.log
```

Expected: a single line containing `Using cached test list (<N> tests)` where `<N>` is around 200. NO "Discovering tests…" line.

Tests still pass:

```bash
grep "=== Test summary ===" -A 4 /d/Repos/CkPlugins/Saved/Logs/Phase2-cached.log
```

Expected: `Total: 19`, `Passed: 19`, `Failed: 0`.

Measure the saving:

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-cache-empty.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-cached.startup.json
```

Expected: `total_to_pie_ready_seconds` drops by roughly 15-20 seconds. The `engine_init_seconds` should be roughly the same as before (the per-process editor init cost hasn't changed — we just pay it once now instead of twice).

- [ ] **Step 6: Smoke-test the `--build` trigger**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase2-build-trigger.log --project='D:\Repos\CkPlugins'"
```

Inspect:

```bash
grep -nE "Discovering tests|Using cached test list" /d/Repos/CkPlugins/Saved/Logs/Phase2-build-trigger.log
```

Expected: `Discovering tests… (--build in this invocation; test list may have changed)`. The build-just-ran trigger fired.

- [ ] **Step 7: Smoke-test the `--discover-fresh` trigger**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --discover-fresh --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase2-discover-fresh.log --project='D:\Repos\CkPlugins'"
```

Inspect:

```bash
grep -nE "Discovering tests|Using cached test list" /d/Repos/CkPlugins/Saved/Logs/Phase2-discover-fresh.log
```

Expected: `Discovering tests… (--discover-fresh requested)`. The DiscoverFresh-requested trigger fired.

All four log-line variants exercised; the four-way branch is functionally verified.

- [ ] **Step 8: Commit**

```bash
git -C /d/Repos/FtxUiFramework add apps/UnrealToolbox/src/UnrealToolbox_Main.cpp
git -C /d/Repos/FtxUiFramework commit -m "feat(UnrealToolbox): cache-aware test discovery in RunHeadlessTest

Replace the always-discover block with a Decide_TestDiscoveryMode-driven
branch:

  - Cache empty                  -> Discover (with reason in log line)
  - --build in this invocation   -> Discover
  - --discover-fresh requested   -> Discover
  - Otherwise                    -> Reuse Settings.Tests.DiscoveredTests

Smoke-test on the four-way branch verified all four log-line variants
fire correctly. Tests still 19/19 green on both fresh-discovery and
cached paths.

Expected saving: ~15-20s on iteration cycles (--test without --build
on the same build). First-run and post-build cycles unchanged."
```

---

## Task 6: Final measurement + CkAuto submodule bump + Phase 2 sign-off

**Files:**
- Modify: `D:\Repos\CkPlugins\docs\superpowers\specs\2026-05-13-toolbox-discovery-cache-design.md` — append Phase 2 Results section.
- (Possibly) commit a fresh CkAuto binary if it hasn't already been pushed in CkAuto's repo.

- [ ] **Step 1: Push the FtxUiFramework commits**

```bash
git -C /d/Repos/FtxUiFramework push 2>&1 | tail -10
```

Expected: five commits land on `origin/feature/cliseq` (Tasks 1-5 commits). If push needs auth that the agent can't supply non-interactively, surface to the user.

- [ ] **Step 2: Commit and push the CkAuto binary**

The `CkAuto/UnrealToolbox.exe` binary was updated by every rebuild in Tasks 1, 3, 5. Stage and commit in the CkAuto submodule:

```bash
git -C /d/Repos/CkPlugins/CkAuto status
```

If the binary is dirty:

```bash
git -C /d/Repos/CkPlugins/CkAuto add UnrealToolbox.exe
git -C /d/Repos/CkPlugins/CkAuto commit -m "chore(bin): bump UnrealToolbox.exe with --discover-fresh + discovery cache

Phase 2 toolbox build. RunHeadlessTest now reuses
Settings.Tests.DiscoveredTests when cache is non-empty and neither
--build nor --discover-fresh was passed. Iteration cycles drop one
editor cold-start (~17s saving)."
git -C /d/Repos/CkPlugins/CkAuto push 2>&1 | tail -10
```

- [ ] **Step 3: Capture the Phase 2 final snapshot**

Editor lock check, then run:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase2-final-postbuild.log --project='D:\Repos\CkPlugins'"
```

This is the post-build cycle (does NOT benefit from caching — discovery fires because `--build` was passed). Captures the baseline for the "first run after rebuild" case.

Then run `--test` alone for the cached cycle:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Phase2-final-cached.log --project='D:\Repos\CkPlugins'"
```

Compare:

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-final-postbuild.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-final-cached.startup.json
```

Record `total_to_pie_ready_seconds` for both runs and the delta.

Also compare against Phase 1's baseline if available:

```bash
ls /d/Repos/CkPlugins/Saved/Logs/BuildTest-bundle.startup.json
# If present:
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-bundle.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/Phase2-final-cached.startup.json
```

This shows Phase 1 + Phase 2 combined savings.

- [ ] **Step 4: Append Phase 2 Results to the spec**

Open `D:\Repos\CkPlugins\docs\superpowers\specs\2026-05-13-toolbox-discovery-cache-design.md`. Append a new section before the existing `## Phase 3 (deferred)` section:

```markdown
## Phase 2 Results (filled in on completion)

**First-run cycle (--build --test, cache invalidated):**

| Phase | Phase 1 final | Phase 2 final | Delta |
|---|---|---|---|
| engine_init_seconds | <fill> | <fill> | <fill> |
| total_to_pie_ready_seconds | <fill> | <fill> | <fill> |

(Expected: roughly unchanged. First-run still pays both spawns.)

**Iteration cycle (--test alone, cached):**

| Phase | --build cycle | cached cycle | Delta |
|---|---|---|---|
| engine_init_seconds | <fill> | <fill> | <fill> |
| total_to_pie_ready_seconds | <fill> | <fill> | <fill> |

(Expected: total_to_pie_ready_seconds drops by ~15-20 sec.)

**Combined Phase 1 + Phase 2 saving on iteration cycle:**

baseline.startup.json (Phase 1 initial baseline) total: <fill>s
Phase2-final-cached.startup.json total: <fill>s
Saving: <fill> sec

Tests: 19/19 IskmRenderer pass on every smoke-test in Tasks 5 and 6.

Phase 3 decision: [pursue / not pursue]. Reasoning: <fill>.
```

Fill the `<fill>` placeholders with the numbers from Step 3.

- [ ] **Step 5: Commit the spec update + CkAuto submodule bump**

```bash
cd /d/Repos/CkPlugins
git add docs/superpowers/specs/2026-05-13-toolbox-discovery-cache-design.md CkAuto
git commit -m "chore(submodule): bump CkAuto for Phase 2 toolbox + record results

Brings in the Phase 2 toolbox build (--discover-fresh + cache-aware
RunHeadlessTest) and appends Phase 2 Results to the design spec.

Iteration cycle now skips the discovery editor spawn when the cache is
non-empty and neither --build nor --discover-fresh was passed. See spec
for measured savings."
git push 2>&1 | tail -10
```

---

## Self-Review

**Spec coverage:**

- Spec Deliverable 1 (`--discover-fresh` CLI flag) → Task 1.
- Spec Deliverable 2 (decision function + Catch2 tests) → Tasks 2 (declare), 3 (failing tests), 4 (implementation turns tests green).
- Spec Deliverable 3 (`RunHeadlessTest` integration) → Task 5.
- Spec "Testing" section (decision-function unit tests + end-to-end smoke) → Tasks 3-4 (unit) and Task 5 Steps 4-7 (smoke).
- Spec "Success Criteria" (all 5 bullets) → covered by Task 5 Steps 4-7 (each smoke variant) plus Task 6 (measurement + recorded results).

**Placeholder scan:**

- The `<fill>` markers in Task 6 Step 4 are inside a documented spec-results-table that the implementer fills with actual run output. They're explicit substitution points, not unfilled blanks.
- No `<file>` / `<list>` / generic-placeholder patterns.
- No "TBD" or "TODO" in actionable steps.

**Type consistency:**

- `ETestDiscoveryMode` (enum), `FTestDiscoveryInputs` (struct), and `Decide_TestDiscoveryMode` (function) are named identically across Tasks 2, 3, 4, 5.
- Field names (`DiscoverFreshRequested`, `BuildJustRan`, `CacheEmpty`) match across the declaration, the tests, and the call site.
- `Settings.Tests.DiscoveredTests` and `Settings.Tests.LastDiscoveredUnix` are the only `Settings` paths touched — consistent with the spec.

**Sequencing:**

- Task 1 (CLI flag) → required input for the decision function call site in Task 5. Independent of decision-function implementation; can ship in any order before Task 5.
- Task 2 (header decl) → required for Task 3 (test compiles against the header).
- Task 3 (failing test) → red phase.
- Task 4 (implementation) → green phase. Must come after Task 3.
- Task 5 (integration) → requires Tasks 1, 2, 4 done. Smoke tests confirm end-to-end behaviour.
- Task 6 (push, submodule bump, results) → terminal task.

**Risk acknowledgements:**

- Step 4 of Task 5 (cache-empty smoke test) requires hand-editing the settings JSON to clear the cache. Path discovery noted; if the implementer cannot find the file, they can also bypass by deleting any cache-shaped fields from any plausible path. The `--discover-fresh` flag is a softer alternative if hand-editing is brittle — but the cache-empty branch is part of Spec Deliverable 3 and deserves direct exercise.
- Smoke tests in Steps 5-7 of Task 5 generate log files into `Saved/Logs/`. These are gitignored (per the standard `Saved/` rule), so no cleanup is required.
- The submodule bump in Task 6 depends on the CkAuto submodule push succeeding non-interactively. If push auth fails, the user runs it manually and the agent waits.
