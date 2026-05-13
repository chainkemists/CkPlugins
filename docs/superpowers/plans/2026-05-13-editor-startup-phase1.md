# Editor Startup — Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce wall-clock time of the `./CkAuto/UnrealToolbox.exe --build --test` cycle by shipping native measurement support in the toolbox, plus a project-side cleanup bundle and a small AS-side investigation/fix pass.

**Architecture:** Add `--measure` flag to `UnrealToolbox` (C++23 / CLI11 / nlohmann::json) that parses the run's own log output and emits a JSON timing snapshot. Add `--measure-compare` mode to diff two snapshots. Project-side cleanup is `.ini` / `.uproject` edits. AS work is a discovery task that may or may not produce a small engine-fork patch — the plan has explicit gates.

**Tech Stack:**
- **Toolbox (where measurement lives):** C++23, CLI11 (CLI parser), nlohmann::json (already vendored, used in `Settings.cpp` and `TestClassification.cpp`), Catch2 (test framework, files at `D:\Repos\FtxUiFramework\tests\UnrealToolbox_*.cpp`).
- **Toolbox build:** `cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8`. Auto-deploys to `D:\ConsoleApps\UnrealToolbox.exe` via `ftx_deploy_app()` post-build hook. `CkAuto/UnrealToolbox.exe` may be a stale standalone copy — see Task 1 Step 0 for the verification step.
- **Project side:** UE 5.5, `Config/DefaultEngine.ini`, `CkPlugins.uproject`.
- **Engine fork (if Tasks 8-9 land code):** Hazelight AngelScript plugin at `D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\`.

**Spec:** [docs/superpowers/specs/2026-05-13-editor-startup-phase1-design.md](../specs/2026-05-13-editor-startup-phase1-design.md)

---

## File Structure

**Created (in `D:\Repos\FtxUiFramework`):**
- `apps/UnrealToolbox/include/utb/StartupMeasurement.hpp` — public API (parse + compare functions, snapshot struct).
- `apps/UnrealToolbox/src/StartupMeasurement.cpp` — implementation (regex extraction, JSON emit).
- `tests/UnrealToolbox_StartupMeasurement.cpp` — Catch2 tests pinning regex behavior against a fixture.
- `tests/fixtures/UnrealToolbox_StartupMeasurement_baseline.log` — frozen UE log excerpt for the test.

**Created (in `D:\Repos\CkPlugins`):**
- `docs/superpowers/plans/2026-05-13-as-double-reload-findings.md` — written during Task 9; outcome of AS investigation.

**Modified (in `D:\Repos\FtxUiFramework`):**
- `apps/UnrealToolbox/include/utb/CliArgs.hpp` — add `Measure` bool + `MeasureCompareBefore`/`MeasureCompareAfter` paths.
- `apps/UnrealToolbox/src/CliArgs.cpp` — register `--measure` (sub-flag of `--test`) and `--measure-compare` (top-level mode).
- `apps/UnrealToolbox/src/UnrealToolbox_Main.cpp` — call `EmitStartupSnapshot` at end of `Run_TestMode` when `--measure`; add `Run_MeasureCompareMode` entry path.
- `apps/UnrealToolbox/CMakeLists.txt` — add `StartupMeasurement.cpp/.hpp` to `UTB_SOURCES`/`UTB_HEADERS`; add the test file to `_UTB_TEST_FILES`.

**Modified (in `D:\Repos\CkPlugins`):**
- `Config/DefaultEngine.ini` — platform `.ini` cull + misc settings sweep (Task 8).
- `CkPlugins.uproject` — engine plugin disables (audit-driven, list confirmed before flipping) (Task 8).

**(Conditional) Modified (in `D:\Repos\UnrealEngineAngelscript`):**
- `Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/<file>` — small AS reload-flow fix and/or debug-DB deferral. Only if budget gates pass (≤50 lines for Task 9, ≤30 lines for Task 10).

---

## Task 1: Add `--measure` and `--measure-compare` CLI flags (no implementation yet)

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\include\utb\CliArgs.hpp`
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\CliArgs.cpp`

- [ ] **Step 0: Verify toolbox deploy targets**

Establish whether `D:\Repos\CkPlugins\CkAuto\UnrealToolbox.exe` is the same binary as the build deploys to `D:\ConsoleApps\UnrealToolbox.exe`, or a separate static copy. Run:

```bash
sha256sum /d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe /c/ConsoleApps/UnrealToolbox.exe 2>&1
```

If hashes match, the build pipeline updates both. If they differ, note that you'll need to manually copy after rebuild: `cp /c/ConsoleApps/UnrealToolbox.exe /d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe` after every rebuild during this work.

Document the result in the commit message of Step 4 of this task.

- [ ] **Step 1: Add the flag fields to `FCliArgs`**

In `D:\Repos\FtxUiFramework\apps\UnrealToolbox\include\utb\CliArgs.hpp`, inside the `FCliArgs` struct, add:

```cpp
// --test sub-flags
bool                                  IncludeEngine = false;
std::optional<std::string>            TestPattern;

// --measure: sub-flag of --test. After tests run, parse the log output
// and write a JSON timing snapshot next to it.
//   Output: <output>-startup.json (when --output is set) or stdout.
bool                                  Measure = false;

// --measure-compare <before.json> <after.json>: top-level mode.
// Mutex with --build / --test / --run-scheduled. Reads both snapshots,
// prints a per-phase delta table, exits 0.
std::optional<std::filesystem::path>  MeasureCompareBefore;
std::optional<std::filesystem::path>  MeasureCompareAfter;
```

- [ ] **Step 2: Register the flags with CLI11**

In `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\CliArgs.cpp`, inside `Parse_Args`:

After the existing `--include-engine` block (around line 76), add the `--measure` sub-flag:

```cpp
auto* MeasureOpt = App.add_flag(
    "--measure", Out.Measure,
    "With --test: after tests run, parse the log and emit a JSON "
    "startup-timing snapshot next to the --output file (or to stdout "
    "if --output is not set).");
MeasureOpt->needs(TestOpt);
```

After the existing `--project` block (around line 88), add the `--measure-compare` top-level mode. CLI11 doesn't directly support "option with two file arguments" without help — use a vector option of size 2:

```cpp
auto MeasureCompareRaw = std::vector<std::string>{};
auto* MeasureCompareOpt = App.add_option(
    "--measure-compare", MeasureCompareRaw,
    "Diff two startup snapshot JSONs (produced by --measure). Pass "
    "two paths: <before.json> <after.json>. Exits 0 on success, 2 if "
    "the snapshots' --config differ.")
    ->expected(2);

// Exclusive with everything that does work.
MeasureCompareOpt->excludes(BuildOpt);
MeasureCompareOpt->excludes(TestOpt);
MeasureCompareOpt->excludes(ScheduledOpt);
```

Then after the existing `if (NOT OutputRaw.empty()) …` block (around line 99), wire the parsed paths:

```cpp
if (MeasureCompareRaw.size() == 2)
{
    Out.MeasureCompareBefore = std::filesystem::path{MeasureCompareRaw[0]};
    Out.MeasureCompareAfter  = std::filesystem::path{MeasureCompareRaw[1]};
}
```

- [ ] **Step 3: Build to verify the new flags parse**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8
```

Expected: clean build. If `CkAuto/UnrealToolbox.exe` was a separate copy per Step 0, also do: `cp /c/ConsoleApps/UnrealToolbox.exe /d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe`.

Then:
```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --help 2>&1 | grep -E "measure|compare"
```

Expected output includes:
- `--measure                  With --test: …`
- `--measure-compare X Y      Diff two startup snapshot JSONs …`

Mutex check:
```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare a.json b.json --test 2>&1 | head -5
```
Expected: CLI11 error mentioning `--measure-compare excludes --test`.

- [ ] **Step 4: Commit (in FtxUiFramework)**

```bash
cd /d/Repos/FtxUiFramework
git add apps/UnrealToolbox/include/utb/CliArgs.hpp apps/UnrealToolbox/src/CliArgs.cpp
git commit -m "feat(UnrealToolbox): add --measure and --measure-compare CLI flags

--measure is a sub-flag of --test that emits a JSON startup snapshot
after the test run completes.
--measure-compare <before.json> <after.json> is a top-level mode that
diffs two snapshots, exclusive with --build/--test/--run-scheduled.

This commit wires the CLI surface only — implementation lands in the
next task. (Toolbox deploy targets: <CkAuto matches/differs from
D:\\ConsoleApps — fill from Step 0 result>.)"
```

---

## Task 2: `FStartupMeasurement` skeleton + JSON struct + integration hook

**Files:**
- Create: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\include\utb\StartupMeasurement.hpp`
- Create: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\StartupMeasurement.cpp`
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\CMakeLists.txt`
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\UnrealToolbox_Main.cpp`

- [ ] **Step 1: Define the snapshot struct + public API**

Create `D:\Repos\FtxUiFramework\apps\UnrealToolbox\include\utb\StartupMeasurement.hpp`:

```cpp
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace utb {

// A single startup-timing snapshot. Numbers are wall-clock seconds.
// std::optional means "marker not found in log" — distinct from 0.0
// (which would mean "marker found, marker reported zero").
struct FStartupSnapshot
{
    std::string                                                       RunId;
    std::string                                                       Config;     // "DebugGame" | "Development" | ...
    std::filesystem::path                                             LogPath;
    std::unordered_map<std::string, std::optional<double>>            Phases;
};

// Parse a UE log file produced by a UnrealToolbox --build/--test run
// and return the timing snapshot.
//
// Pure log-parsing — no editor instrumentation. If a future UE version
// changes log line wording, the corresponding regex stops matching and
// that phase reports std::nullopt in the output (rather than silently
// producing wrong numbers).
auto Parse_StartupMeasurement(
    const std::filesystem::path& InLogPath,
    const std::string&           InConfig)
    -> FStartupSnapshot;

// Serialise a snapshot to JSON and write to InOutputPath (or to stdout
// when InOutputPath is std::nullopt).
auto Emit_StartupSnapshot(
    const FStartupSnapshot&                       InSnapshot,
    const std::optional<std::filesystem::path>&   InOutputPath)
    -> void;

// Compare two snapshots. Prints a per-phase delta table to stdout.
// Returns 0 on success, 2 if configs differ (refuses to diff).
auto Run_MeasureCompare(
    const std::filesystem::path& InBefore,
    const std::filesystem::path& InAfter)
    -> int;

} // namespace utb
```

- [ ] **Step 2: Stub the implementation**

Create `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\StartupMeasurement.cpp`:

```cpp
#include "utb/StartupMeasurement.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace utb {

auto
    Parse_StartupMeasurement(
        const std::filesystem::path& InLogPath,
        const std::string&           InConfig)
    -> FStartupSnapshot
{
    // STUB — regex extraction lands in Tasks 4 & 5.
    auto Snapshot   = FStartupSnapshot{};
    Snapshot.Config = InConfig;
    Snapshot.LogPath = InLogPath;
    {
        auto Now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto Tm  = std::tm{};
#ifdef _WIN32
        localtime_s(&Tm, &Now);
#else
        localtime_r(&Now, &Tm);
#endif
        auto Os = std::ostringstream{};
        Os << InLogPath.stem().string() << "_"
           << std::put_time(&Tm, "%Y%m%d-%H%M%S");
        Snapshot.RunId = Os.str();
    }
    return Snapshot;
}

auto
    Emit_StartupSnapshot(
        const FStartupSnapshot&                       InSnapshot,
        const std::optional<std::filesystem::path>&   InOutputPath)
    -> void
{
    auto J = nlohmann::ordered_json{};
    J["run_id"]   = InSnapshot.RunId;
    J["config"]   = InSnapshot.Config;
    J["log_path"] = InSnapshot.LogPath.string();

    auto Phases = nlohmann::ordered_json::object();
    for (const auto& [Name, Value] : InSnapshot.Phases)
    {
        Phases[Name] = Value.has_value()
            ? nlohmann::ordered_json(*Value)
            : nlohmann::ordered_json(nullptr);
    }
    J["phases"] = Phases;

    auto Serialised = J.dump(2);
    if (InOutputPath)
    {
        if (InOutputPath->has_parent_path())
        {
            std::filesystem::create_directories(InOutputPath->parent_path());
        }
        auto Out = std::ofstream{*InOutputPath};
        if (NOT Out)
        {
            std::cerr << "[utb --measure] Cannot open output: " << *InOutputPath << "\n";
            return;
        }
        Out << Serialised << "\n";
        std::cout << "Snapshot written: " << InOutputPath->string() << "\n";
    }
    else
    {
        std::cout << Serialised << "\n";
    }
}

auto
    Run_MeasureCompare(
        const std::filesystem::path& InBefore,
        const std::filesystem::path& InAfter)
    -> int
{
    // STUB — implementation lands in Task 7.
    (void)InBefore; (void)InAfter;
    std::cerr << "[utb --measure-compare] Not yet implemented.\n";
    return 1;
}

} // namespace utb
```

Note: this file uses `NOT` as a macro — confirm it's available in the UnrealToolbox's PCH (the existing CliArgs.cpp uses `NOT`, so the macro is in scope). If it isn't, use `!` instead.

- [ ] **Step 3: Register the new files in CMake**

Edit `D:\Repos\FtxUiFramework\apps\UnrealToolbox\CMakeLists.txt`:

In the `UTB_SOURCES` list, add `src/StartupMeasurement.cpp`.
In the `UTB_HEADERS` list, add `include/utb/StartupMeasurement.hpp`.

- [ ] **Step 4: Wire `--measure` into `Run_TestMode`**

In `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\UnrealToolbox_Main.cpp`:

Add include near the top of the file with the other `utb/*` includes:

```cpp
#include "utb/StartupMeasurement.hpp"
```

In `Run_TestMode` (look for the `return Run.Success ? 0 : 1;` near line 491), before that return statement insert:

```cpp
// --measure: after tests run, parse the log and emit a snapshot.
if (InArgs.Measure)
{
    if (NOT InArgs.Output)
    {
        std::cerr << "[utb --measure] --measure currently requires --output "
                     "(so we know which log to parse). Snapshot skipped.\n";
    }
    else
    {
        auto Snap = utb::Parse_StartupMeasurement(*InArgs.Output, Settings.Build.Config);
        auto SnapPath = *InArgs.Output;
        SnapPath.replace_extension(".startup.json");
        utb::Emit_StartupSnapshot(Snap, SnapPath);
    }
}

return Run.Success ? 0 : 1;
```

- [ ] **Step 5: Wire `--measure-compare` into `main`**

In the same file, find `auto main(int argc, char* argv[]) -> int` (around line 496). After argument parsing returns and you have a valid `FCliArgs`, before the existing `Run_BuildMode` / `Run_TestMode` dispatch, add:

```cpp
if (Args.MeasureCompareBefore.has_value() && Args.MeasureCompareAfter.has_value())
{
    return utb::Run_MeasureCompare(*Args.MeasureCompareBefore, *Args.MeasureCompareAfter);
}
```

If you can't immediately find the dispatch site, search for `Args.Build` or `Args.Test` in `UnrealToolbox_Main.cpp` and put the new branch just before them.

- [ ] **Step 6: Build and smoke-test**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8
# If CkAuto copy was separate per Task 1 Step 0:
cp /c/ConsoleApps/UnrealToolbox.exe /d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe
```

Expected: clean build.

Test the `--measure-compare` stub:
```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare /tmp/a.json /tmp/b.json
```
Expected: prints `Not yet implemented.`, exits 1.

Test the `--measure` stub (this won't actually run editor tests — it'll fail at the editor step because we're not in a project dir — that's OK for this smoke test as long as the flag is recognised):
```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --test --measure --help 2>&1 | grep measure
```
Expected: lists `--measure` in the help text.

- [ ] **Step 7: Commit**

```bash
cd /d/Repos/FtxUiFramework
git add apps/UnrealToolbox/include/utb/StartupMeasurement.hpp \
        apps/UnrealToolbox/src/StartupMeasurement.cpp \
        apps/UnrealToolbox/CMakeLists.txt \
        apps/UnrealToolbox/src/UnrealToolbox_Main.cpp
git commit -m "feat(UnrealToolbox): StartupMeasurement scaffolding + main integration

Add FStartupSnapshot struct, Parse_StartupMeasurement / Emit_StartupSnapshot
/ Run_MeasureCompare API. Wire --measure post-test hook in Run_TestMode and
--measure-compare branch in main. Implementations are stubs — JSON
emission is functional (empty phases map), parsing and compare arrive
in the next tasks."
```

---

## Task 3: Pin parser contract with Catch2 test fixture (failing test first)

**Files:**
- Create: `D:\Repos\FtxUiFramework\tests\UnrealToolbox_StartupMeasurement.cpp`
- Create: `D:\Repos\FtxUiFramework\tests\fixtures\UnrealToolbox_StartupMeasurement_baseline.log`
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\CMakeLists.txt`

- [ ] **Step 1: Capture the fixture log**

Extract a minimal log excerpt from a known-good toolbox run. Aim for ~150 lines covering: first timestamped line, `Engine is initialized`, all AS marker lines (bindings total, both class-gen reloads, both post-full-reloads, script reload total, debug db, both script compile totals), and the first `LogAutomationController: Display: Test Started` line.

Source: an existing `Saved/Logs/BuildTest*.log` in `D:\Repos\CkPlugins\Saved\Logs\`. Pick the most recent successful run.

```bash
mkdir -p /d/Repos/FtxUiFramework/tests/fixtures
# Identify the line ranges:
grep -nE "LogInit: Initializing FReadOnlyCVARCache|Engine is initialized|Angelscript: == bindings total|Angelscript: class generator reload|Angelscript: ==script reload total|Angelscript: post full reload|Angelscript: Sending debug database|Angelscript: script compilation total|LogAutomationController: Display: Test Started" /d/Repos/CkPlugins/Saved/Logs/BuildTest.log | head -25
# Pick start/end line numbers covering all the markers.
# Then extract:
sed -n '<start>,<end>p' /d/Repos/CkPlugins/Saved/Logs/BuildTest.log \
    > /d/Repos/FtxUiFramework/tests/fixtures/UnrealToolbox_StartupMeasurement_baseline.log
wc -l /d/Repos/FtxUiFramework/tests/fixtures/UnrealToolbox_StartupMeasurement_baseline.log
```

Verify the fixture contains every required marker:
```bash
grep -cE "Engine is initialized|bindings total|class generator reload|script reload total|post full reload|Sending debug database|Test Started" /d/Repos/FtxUiFramework/tests/fixtures/UnrealToolbox_StartupMeasurement_baseline.log
```
Expected: at least 8 matches (one Engine is initialized, one bindings total, two class-gen reloads, one script reload total, two post-full-reloads, one debug db, one Test Started).

- [ ] **Step 2: Write the failing test**

Create `D:\Repos\FtxUiFramework\tests\UnrealToolbox_StartupMeasurement.cpp`:

```cpp
// Tests for utb::Parse_StartupMeasurement (UE log parser).
//
// Backed by a frozen log fixture under tests/fixtures/. The fixture is
// real log excerpt — not synthetic — so the test catches drift in UE's
// log line wording when the engine bumps version.

#include "utb/StartupMeasurement.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <string>

namespace
{
    auto FixturePath() -> std::filesystem::path
    {
        // Tests run with cwd = the build's test working directory. The
        // fixtures dir is at <repo>/tests/fixtures/ relative to source —
        // we have to navigate from $<TARGET_RUNTIME_DIR> to it. For
        // simplicity the fixture path is wired via a compile-time define
        // (see CMakeLists changes) that resolves to the absolute fixture
        // path at build time.
        return std::filesystem::path{UTB_TEST_FIXTURE_DIR}
             / "UnrealToolbox_StartupMeasurement_baseline.log";
    }
}

TEST_CASE("Parse_StartupMeasurement on baseline fixture produces non-empty phases",
          "[utb][StartupMeasurement]")
{
    auto Snap = utb::Parse_StartupMeasurement(FixturePath(), "DebugGame");
    REQUIRE(Snap.Config == "DebugGame");
    REQUIRE(NOT Snap.Phases.empty());
}

TEST_CASE("Parse_StartupMeasurement extracts engine_init_seconds",
          "[utb][StartupMeasurement]")
{
    auto Snap = utb::Parse_StartupMeasurement(FixturePath(), "DebugGame");
    auto It   = Snap.Phases.find("engine_init_seconds");
    REQUIRE(It != Snap.Phases.end());
    REQUIRE(It->second.has_value());
    REQUIRE(*It->second > 0.0);
}

TEST_CASE("Parse_StartupMeasurement extracts both AS class-gen reload occurrences distinctly",
          "[utb][StartupMeasurement]")
{
    auto Snap = utb::Parse_StartupMeasurement(FixturePath(), "DebugGame");
    auto It1 = Snap.Phases.find("as_class_gen_reload_1_seconds");
    auto It2 = Snap.Phases.find("as_class_gen_reload_2_seconds");
    REQUIRE(It1 != Snap.Phases.end());
    REQUIRE(It2 != Snap.Phases.end());
    REQUIRE(It1->second.has_value());
    REQUIRE(It2->second.has_value());
    // If both values are identical, either the fixture is synthetic (bad)
    // or the parser is indexing the same marker twice (also bad).
    REQUIRE(*It1->second != *It2->second);
}

TEST_CASE("Parse_StartupMeasurement extracts as_debug_db_seconds (seconds-suffix marker)",
          "[utb][StartupMeasurement]")
{
    auto Snap = utb::Parse_StartupMeasurement(FixturePath(), "DebugGame");
    auto It   = Snap.Phases.find("as_debug_db_seconds");
    REQUIRE(It != Snap.Phases.end());
    REQUIRE(It->second.has_value());
    REQUIRE(*It->second > 0.0);
}

TEST_CASE("Parse_StartupMeasurement extracts as_script_reload_total_seconds",
          "[utb][StartupMeasurement]")
{
    auto Snap = utb::Parse_StartupMeasurement(FixturePath(), "DebugGame");
    auto It   = Snap.Phases.find("as_script_reload_total_seconds");
    REQUIRE(It != Snap.Phases.end());
    REQUIRE(It->second.has_value());
    REQUIRE(*It->second > 0.0);
}

TEST_CASE("Parse_StartupMeasurement reports nullopt for missing markers (empty log)",
          "[utb][StartupMeasurement]")
{
    // Write a tiny synthetic log with NO markers and confirm we don't crash
    // and report nullopt for every phase rather than fabricating zeros.
    auto TmpPath = std::filesystem::temp_directory_path() / "utb_empty_log.log";
    {
        auto Out = std::ofstream{TmpPath};
        Out << "[2026.05.13-17.25.58:097][  0]LogInit: nothing useful\n";
    }
    auto Snap = utb::Parse_StartupMeasurement(TmpPath, "DebugGame");
    for (const auto& [Name, Value] : Snap.Phases)
    {
        INFO("Phase: " << Name);
        REQUIRE_FALSE(Value.has_value());
    }
    std::filesystem::remove(TmpPath);
}
```

- [ ] **Step 3: Register the test file + fixture define in CMake**

In `D:\Repos\FtxUiFramework\apps\UnrealToolbox\CMakeLists.txt`, find the existing `_UTB_TEST_FILES` block (near the bottom). Add the new file:

```cmake
set(_UTB_TEST_FILES
    "${CMAKE_SOURCE_DIR}/tests/UnrealToolbox_CliArgs.cpp"
    "${CMAKE_SOURCE_DIR}/tests/UnrealToolbox_TestClassification.cpp"
    "${CMAKE_SOURCE_DIR}/tests/UnrealToolbox_StartupMeasurement.cpp"
)
```

After the existing `target_sources(FtxUnitTests PRIVATE ${_UTB_TEST_FILES_EXISTING})` line, add a compile definition for the fixture directory:

```cmake
target_compile_definitions(FtxUnitTests PRIVATE
    UTB_TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
```

- [ ] **Step 4: Build and run the test — confirm it FAILS as expected**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target FtxUnitTests --parallel 8
/d/Repos/FtxUiFramework_Build/ftxc/FtxUnitTests "[StartupMeasurement]"
```

Expected: tests fail. The current `Parse_StartupMeasurement` stub returns an empty `Phases` map, so:
- "non-empty phases" → fails (`Phases` is empty)
- "extracts engine_init_seconds" → fails (key not found)
- etc.

This is the TDD red phase — confirms the test is wired correctly before we implement.

- [ ] **Step 5: Commit (test only, no implementation yet)**

```bash
cd /d/Repos/FtxUiFramework
git add tests/UnrealToolbox_StartupMeasurement.cpp \
        tests/fixtures/UnrealToolbox_StartupMeasurement_baseline.log \
        apps/UnrealToolbox/CMakeLists.txt
git commit -m "test(UnrealToolbox): pin StartupMeasurement parser contract (failing)

Catch2 fixture-backed tests against a frozen UE log excerpt. Six cases:
non-empty phases, engine_init_seconds, both AS class-gen reloads distinct,
as_debug_db_seconds (seconds-suffix variant), as_script_reload_total_seconds,
and missing-marker handling.

Tests fail at this commit — the parser is still the stub from the
previous commit. Implementation lands next."
```

---

## Task 4: Implement engine_init + total_to_pie_ready extraction

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\StartupMeasurement.cpp`

- [ ] **Step 1: Add timestamp parsing helpers**

In `StartupMeasurement.cpp`, after the existing includes and before the `namespace utb {` block, add:

```cpp
#include <fstream>
#include <optional>
#include <regex>
#include <string>

namespace
{
    // UE log timestamp format: "[2026.05.13-17.25.58:097]"  (ms precision).
    const std::regex kTimestampRx{
        R"(^\[(\d{4})\.(\d{2})\.(\d{2})-(\d{2})\.(\d{2})\.(\d{2}):(\d{3})\])"};

    struct FParsedTimestamp
    {
        int Year{}, Month{}, Day{}, Hour{}, Minute{}, Second{}, Millis{};

        // Convert to "milliseconds since epoch midnight Jan 1, year 2000 (UTC-naive)".
        // We don't care about absolute time — just deltas — so any consistent base works.
        auto AsMillis() const -> int64_t
        {
            // Simple wall-clock-naive: encode as ms-from-arbitrary-start.
            // This works for deltas of up to 24 hours within the same day.
            return ((int64_t(Hour) * 60 + Minute) * 60 + Second) * 1000 + Millis;
        }
    };

    auto Parse_Timestamp(const std::string& InLine) -> std::optional<FParsedTimestamp>
    {
        auto M = std::smatch{};
        if (NOT std::regex_search(InLine, M, kTimestampRx))
        {
            return std::nullopt;
        }
        auto Ts = FParsedTimestamp{};
        Ts.Year   = std::stoi(M[1]);
        Ts.Month  = std::stoi(M[2]);
        Ts.Day    = std::stoi(M[3]);
        Ts.Hour   = std::stoi(M[4]);
        Ts.Minute = std::stoi(M[5]);
        Ts.Second = std::stoi(M[6]);
        Ts.Millis = std::stoi(M[7]);
        return Ts;
    }
}
```

- [ ] **Step 2: Replace the stub `Parse_StartupMeasurement` body with the real implementation**

Replace the stub body in `Parse_StartupMeasurement` with:

```cpp
auto
    Parse_StartupMeasurement(
        const std::filesystem::path& InLogPath,
        const std::string&           InConfig)
    -> FStartupSnapshot
{
    auto Snapshot   = FStartupSnapshot{};
    Snapshot.Config = InConfig;
    Snapshot.LogPath = InLogPath;
    {
        auto Now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto Tm  = std::tm{};
#ifdef _WIN32
        localtime_s(&Tm, &Now);
#else
        localtime_r(&Now, &Tm);
#endif
        auto Os = std::ostringstream{};
        Os << InLogPath.stem().string() << "_"
           << std::put_time(&Tm, "%Y%m%d-%H%M%S");
        Snapshot.RunId = Os.str();
    }

    // Initialise every known phase to nullopt — we'll fill in the ones we find.
    static const char* const kKnownPhases[] = {
        "engine_init_seconds",
        "total_to_pie_ready_seconds",
        "as_bindings_seconds",
        "as_class_gen_reload_1_seconds",
        "as_class_gen_reload_2_seconds",
        "as_script_compile_1_seconds",
        "as_script_compile_2_seconds",
        "as_post_full_reload_1_seconds",
        "as_post_full_reload_2_seconds",
        "as_script_reload_total_seconds",
        "as_debug_db_seconds",
    };
    for (const auto* Key : kKnownPhases)
    {
        Snapshot.Phases[Key] = std::nullopt;
    }

    auto LogFile = std::ifstream{InLogPath};
    if (NOT LogFile)
    {
        return Snapshot;
    }

    auto Line          = std::string{};
    auto StartTsOpt    = std::optional<FParsedTimestamp>{};
    auto EngineInitOpt = std::optional<FParsedTimestamp>{};
    auto TestStartOpt  = std::optional<FParsedTimestamp>{};

    while (std::getline(LogFile, Line))
    {
        if (NOT StartTsOpt)
        {
            StartTsOpt = Parse_Timestamp(Line);
        }
        if (NOT EngineInitOpt &&
            Line.find("LogInit:") != std::string::npos &&
            Line.find("Engine is initialized") != std::string::npos)
        {
            EngineInitOpt = Parse_Timestamp(Line);
        }
        if (NOT TestStartOpt &&
            Line.find("LogAutomationController:") != std::string::npos &&
            Line.find("Test Started") != std::string::npos)
        {
            TestStartOpt = Parse_Timestamp(Line);
        }
    }

    if (StartTsOpt && EngineInitOpt)
    {
        const auto Delta = (EngineInitOpt->AsMillis() - StartTsOpt->AsMillis()) / 1000.0;
        Snapshot.Phases["engine_init_seconds"] = Delta;
    }
    if (StartTsOpt && TestStartOpt)
    {
        const auto Delta = (TestStartOpt->AsMillis() - StartTsOpt->AsMillis()) / 1000.0;
        Snapshot.Phases["total_to_pie_ready_seconds"] = Delta;
    }

    return Snapshot;
}
```

- [ ] **Step 3: Build and re-run the test**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target FtxUnitTests --parallel 8
/d/Repos/FtxUiFramework_Build/ftxc/FtxUnitTests "[StartupMeasurement]"
```

Expected progress: "non-empty phases" passes, "extracts engine_init_seconds" passes, the rest still fail (the AS markers aren't extracted yet).

- [ ] **Step 4: Commit**

```bash
cd /d/Repos/FtxUiFramework
git add apps/UnrealToolbox/src/StartupMeasurement.cpp
git commit -m "feat(UnrealToolbox): StartupMeasurement extracts engine_init + total_to_pie_ready"
```

---

## Task 5: Implement the AS-marker extraction

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\StartupMeasurement.cpp`

- [ ] **Step 1: Add the AS-marker scanning loop**

In `StartupMeasurement.cpp`, inside `Parse_StartupMeasurement`, before the final `return Snapshot;`, add:

```cpp
// AS markers — multi-occurrence patterns are indexed by ordinal (first / second).
// Note: the AS engine prints some markers in ms and others in seconds.
struct FAsPattern
{
    std::regex  Rx;
    bool        IsMs;         // true → group "value" is milliseconds; false → seconds
    std::vector<std::string> PhaseKeysByOrdinal;
};

const auto AsPatterns = std::vector<FAsPattern>{
    { std::regex{R"(Angelscript:\s*==\s*bindings total\s*==\s*took\s+([\d.]+)\s*ms)"},
      true,  { "as_bindings_seconds" } },
    { std::regex{R"(Angelscript:\s*class generator reload took\s+([\d.]+)\s*ms)"},
      true,  { "as_class_gen_reload_1_seconds", "as_class_gen_reload_2_seconds" } },
    { std::regex{R"(Angelscript:\s*script compilation total took\s+([\d.]+)\s*ms)"},
      true,  { "as_script_compile_1_seconds", "as_script_compile_2_seconds" } },
    { std::regex{R"(Angelscript:\s*post full reload took\s+([\d.]+)\s*ms)"},
      true,  { "as_post_full_reload_1_seconds", "as_post_full_reload_2_seconds" } },
    { std::regex{R"(Angelscript:\s*==script reload total\s*==\s*took\s+([\d.]+)\s*ms)"},
      true,  { "as_script_reload_total_seconds" } },
    { std::regex{R"(Angelscript:\s*Sending debug database took\s+([\d.]+)\s+seconds)"},
      false, { "as_debug_db_seconds" } },
};

// Re-open the file (cheaper than buffering all lines) and run a single pass.
auto OccurrenceCounts = std::unordered_map<const FAsPattern*, size_t>{};
LogFile.clear();
LogFile.seekg(0);
while (std::getline(LogFile, Line))
{
    for (const auto& Pat : AsPatterns)
    {
        auto M = std::smatch{};
        if (std::regex_search(Line, M, Pat.Rx))
        {
            const auto Ordinal = OccurrenceCounts[&Pat]++;
            if (Ordinal < Pat.PhaseKeysByOrdinal.size())
            {
                const auto Raw      = std::stod(M[1]);
                const auto Seconds  = Pat.IsMs ? (Raw / 1000.0) : Raw;
                Snapshot.Phases[Pat.PhaseKeysByOrdinal[Ordinal]] = Seconds;
            }
        }
    }
}
```

- [ ] **Step 2: Build and re-run all StartupMeasurement tests — all must pass**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target FtxUnitTests --parallel 8
/d/Repos/FtxUiFramework_Build/ftxc/FtxUnitTests "[StartupMeasurement]"
```

Expected: all 6 tests pass.

- [ ] **Step 3: End-to-end smoke against a real CkPlugins log**

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare /tmp/dummy_a.json /tmp/dummy_b.json
```
Still prints `Not yet implemented` (compare is Task 7).

```bash
# Re-parse an existing CkPlugins log via the toolbox by hand-running the parser path.
# Since --measure only kicks in after --test, the easiest verification is the test suite.
# For real-log verification, run the full test cycle in Task 6.
```

(End-to-end real-run verification happens in Task 6 — this step is just the unit-test green.)

- [ ] **Step 4: Commit**

```bash
cd /d/Repos/FtxUiFramework
git add apps/UnrealToolbox/src/StartupMeasurement.cpp
git commit -m "feat(UnrealToolbox): StartupMeasurement extracts all AS-side timing markers"
```

---

## Task 6: Capture baseline snapshot (post-implementation, pre-changes)

**Files:**
- Create: `D:\Repos\CkPlugins\Saved\Logs\Startup-baseline.startup.json` (NOT committed — `Saved/` is gitignored).

- [ ] **Step 1: Run a fresh build+test+measure cycle**

```bash
# Editor lock check
pwsh -Command "try { [IO.File]::Open('D:\Repos\CkPlugins\Saved\Logs\CkPlugins.log','Open','Write','None').Close(); 'free' } catch { 'locked' }"
# If 'locked', wait it out.
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/BuildTest-baseline.log --project='D:\Repos\CkPlugins'"
```

Expected: build+test runs to completion with exit code 0. 19/19 IskmRenderer tests pass. A `Saved/Logs/BuildTest-baseline.startup.json` file is created alongside the log.

- [ ] **Step 2: Inspect the snapshot**

```bash
cat Saved/Logs/BuildTest-baseline.startup.json
```

Expected: JSON with `config: "DebugGame"`, all `engine_init_seconds` / `as_*_seconds` / `total_to_pie_ready_seconds` fields populated with non-null doubles. Record these numbers — they're the values to beat.

If any AS field is null but the marker exists in the log, the regex didn't match — fix the regex in `StartupMeasurement.cpp` (Task 5 Step 1) and rebuild before proceeding.

- [ ] **Step 3: No commit needed**

`Saved/` is gitignored. The baseline lives on your machine for comparison. Take a screenshot or paste the JSON into the conversation as a reference point.

---

## Task 7: Implement `--measure-compare`

**Files:**
- Modify: `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\StartupMeasurement.cpp`

- [ ] **Step 1: Add unit tests for the compare function (failing first)**

Append to `D:\Repos\FtxUiFramework\tests\UnrealToolbox_StartupMeasurement.cpp`:

```cpp
namespace
{
    auto WriteJson(const std::filesystem::path& InPath, const std::string& InContent) -> void
    {
        auto Out = std::ofstream{InPath};
        Out << InContent;
    }
}

TEST_CASE("Run_MeasureCompare refuses to diff snapshots with differing configs",
          "[utb][StartupMeasurement]")
{
    auto Tmp = std::filesystem::temp_directory_path();
    auto A   = Tmp / "utb_cmp_a.json";
    auto B   = Tmp / "utb_cmp_b.json";
    WriteJson(A, R"({"config":"DebugGame","phases":{"total_to_pie_ready_seconds":26.0}})");
    WriteJson(B, R"({"config":"Development","phases":{"total_to_pie_ready_seconds":12.0}})");
    REQUIRE(utb::Run_MeasureCompare(A, B) == 2);
    std::filesystem::remove(A);
    std::filesystem::remove(B);
}

TEST_CASE("Run_MeasureCompare succeeds when configs match",
          "[utb][StartupMeasurement]")
{
    auto Tmp = std::filesystem::temp_directory_path();
    auto A   = Tmp / "utb_cmp_a.json";
    auto B   = Tmp / "utb_cmp_b.json";
    WriteJson(A, R"({"config":"DebugGame","phases":{"total_to_pie_ready_seconds":26.0}})");
    WriteJson(B, R"({"config":"DebugGame","phases":{"total_to_pie_ready_seconds":20.0}})");
    REQUIRE(utb::Run_MeasureCompare(A, B) == 0);
    std::filesystem::remove(A);
    std::filesystem::remove(B);
}
```

Build + run; confirm both tests fail (the stub returns 1):

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target FtxUnitTests --parallel 8
/d/Repos/FtxUiFramework_Build/ftxc/FtxUnitTests "[StartupMeasurement]"
```

- [ ] **Step 2: Implement `Run_MeasureCompare`**

Replace the stub `Run_MeasureCompare` body in `StartupMeasurement.cpp`:

```cpp
auto
    Run_MeasureCompare(
        const std::filesystem::path& InBefore,
        const std::filesystem::path& InAfter)
    -> int
{
    auto Read = [](const std::filesystem::path& Path) -> std::optional<nlohmann::json>
    {
        auto In = std::ifstream{Path};
        if (NOT In)
        {
            std::cerr << "[utb --measure-compare] Cannot open " << Path << "\n";
            return std::nullopt;
        }
        try { return nlohmann::json::parse(In); }
        catch (const std::exception& E)
        {
            std::cerr << "[utb --measure-compare] Invalid JSON in " << Path << ": " << E.what() << "\n";
            return std::nullopt;
        }
    };

    auto BeforeJ = Read(InBefore);
    auto AfterJ  = Read(InAfter);
    if (NOT BeforeJ || NOT AfterJ) return 1;

    auto BeforeCfg = BeforeJ->value("config", std::string{});
    auto AfterCfg  = AfterJ->value("config",  std::string{});
    if (BeforeCfg != AfterCfg)
    {
        std::cerr << "[utb --measure-compare] Configs differ: before=" << BeforeCfg
                  << " vs after=" << AfterCfg
                  << ". Re-run both with the same --config.\n";
        return 2;
    }

    auto BeforePhases = (*BeforeJ)["phases"];
    auto AfterPhases  = (*AfterJ)["phases"];

    // Gather union of phase names, sorted for stable output.
    auto Phases = std::set<std::string>{};
    for (auto It = BeforePhases.begin(); It != BeforePhases.end(); ++It) Phases.insert(It.key());
    for (auto It = AfterPhases.begin();  It != AfterPhases.end();  ++It) Phases.insert(It.key());

    auto Width_Phase = std::string::size_type{ std::string{"Phase"}.size() };
    for (const auto& P : Phases) Width_Phase = std::max(Width_Phase, P.size());

    std::cout << std::left << std::setw(static_cast<int>(Width_Phase) + 2) << "Phase"
              << std::right << std::setw(10) << "Before"
              << std::setw(10) << "After"
              << std::setw(10) << "Delta"
              << std::setw(8)  << "Pct%"
              << "   Note\n";

    auto AsNum = [](const nlohmann::json& J) -> std::optional<double>
    {
        if (J.is_number()) return J.get<double>();
        return std::nullopt;
    };

    for (const auto& Phase : Phases)
    {
        auto B = BeforePhases.contains(Phase) ? AsNum(BeforePhases[Phase]) : std::nullopt;
        auto A = AfterPhases.contains(Phase)  ? AsNum(AfterPhases[Phase])  : std::nullopt;

        std::string Note;
        if (NOT B && A)     Note = "BEFORE-MISSING";
        else if (B && NOT A) Note = "AFTER-MISSING";
        else if (NOT B && NOT A) Note = "BOTH-MISSING";

        auto FmtVal = [](std::optional<double> V) -> std::string
        {
            if (NOT V) return "—";
            auto Os = std::ostringstream{};
            Os << std::fixed << std::setprecision(3) << *V;
            return Os.str();
        };

        std::cout << std::left << std::setw(static_cast<int>(Width_Phase) + 2) << Phase
                  << std::right << std::setw(10) << FmtVal(B)
                  << std::setw(10) << FmtVal(A);

        if (B && A)
        {
            auto Delta = *A - *B;
            std::cout << std::setw(10) << FmtVal(Delta);
            if (*B != 0.0)
            {
                auto Pct = ((*A - *B) / *B) * 100.0;
                auto Os  = std::ostringstream{};
                Os << std::fixed << std::setprecision(1) << Pct;
                std::cout << std::setw(8) << Os.str();
            }
            else
            {
                std::cout << std::setw(8) << "—";
            }
        }
        else
        {
            std::cout << std::setw(10) << "—" << std::setw(8) << "—";
        }
        std::cout << "   " << Note << "\n";
    }

    auto BeforeTotal = AsNum(BeforePhases.value("total_to_pie_ready_seconds", nlohmann::json{}));
    auto AfterTotal  = AsNum(AfterPhases.value("total_to_pie_ready_seconds",  nlohmann::json{}));
    if (BeforeTotal && AfterTotal)
    {
        auto TotalDelta = *AfterTotal - *BeforeTotal;
        auto Sign = TotalDelta < 0 ? "FASTER" : (TotalDelta > 0 ? "SLOWER" : "SAME");
        std::cout << "\nTotal wall-clock: " << *BeforeTotal << "s -> "
                  << *AfterTotal << "s  (" << TotalDelta << "s, " << Sign << ")\n";
    }

    return 0;
}
```

Add the necessary includes near the top of `StartupMeasurement.cpp`:

```cpp
#include <iomanip>
#include <set>
```

- [ ] **Step 3: Build, re-run all tests, verify**

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target FtxUnitTests --parallel 8
/d/Repos/FtxUiFramework_Build/ftxc/FtxUnitTests "[StartupMeasurement]"
```

Expected: all 8 tests pass.

Build the toolbox and smoke-test compare against a snapshot compared to itself:

```bash
cmake --build D:/Repos/FtxUiFramework_Build --target UnrealToolbox --parallel 8
# (cp to CkAuto if needed per Task 1 Step 0)
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-baseline.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-baseline.startup.json
```

Expected: every Delta is 0.000, every Pct% is 0.0, total wall-clock prints `SAME`.

- [ ] **Step 4: Commit**

```bash
cd /d/Repos/FtxUiFramework
git add apps/UnrealToolbox/src/StartupMeasurement.cpp tests/UnrealToolbox_StartupMeasurement.cpp
git commit -m "feat(UnrealToolbox): Run_MeasureCompare prints per-phase delta table

Refuses to diff snapshots with differing --config values (exit 2).
Total wall-clock summary line at the bottom. Missing phases on either
side flagged so 'regex drifted' doesn't get misread as an improvement."
```

---

## Task 8: Audit currently-loaded engine plugins

**Files:** Read-only audit. No writes yet. Output: a list to confirm with the user.

- [ ] **Step 1: List which plugins UE loaded during the baseline run**

```bash
grep -oE "Mounting plugin [A-Za-z0-9_]+|LogPluginManager: Mounting plugin [A-Za-z0-9_]+" /d/Repos/CkPlugins/Saved/Logs/BuildTest-baseline.log | sort -u
```

Expected: a sorted list of plugin names that loaded during startup. Save to a scratch file:

```bash
grep -oE "Mounting plugin [A-Za-z0-9_]+" /d/Repos/CkPlugins/Saved/Logs/BuildTest-baseline.log | sort -u > /tmp/loaded-plugins.txt
wc -l /tmp/loaded-plugins.txt
```

- [ ] **Step 2: Cross-reference against the project's allowlist**

Read `D:\Repos\CkPlugins\CkPlugins.uproject`. It already disables ~15 plugins explicitly. Identify plugins loading from the engine that the host project doesn't actively use.

Candidate categories (don't disable yet — this is the proposal):
- **XR / AR / VR:** `XRBase`, `OpenXR`, `OpenXREyeTracker`, `OpenXRHandTracking`, `OpenXRMsftHandInteraction`.
- **Mobile / cross-platform:** `MobileLauncherProfileWizard` (already off — confirm it's not loading transitively), `GoogleCloudMessaging` (already off — same), platform-specific device profiles.
- **Media / movies:** `MediaCompositing`, `MediaFrameworkUtilities`, `MovieRenderPipelineCore`, `MovieRenderPipelineEditor`.
- **Legacy:** `LegacyEditorWidgets`.
- **Online / multiplayer:** `OnlineSubsystemNull`, `OnlineSubsystemUtils` (if not used), `OnlineFramework`.
- **Geometry / Chaos:** `ChaosNiagara`, `GeometryCollectionEngine` if CkChaos doesn't actually depend on `UGeometryCollectionComponent`.
- **Build/iteration:** `LiveCoding` — confirm with the user whether they rely on it for non-toolbox iteration.

**Output a candidate list** in a markdown table in the conversation or a scratch file with columns: plugin name, observed in baseline (yes/no/transitive), reason candidate, risk-of-disabling. **Do not flip any toggles yet.**

- [ ] **Step 3: Present the candidate list to the user, get the confirmed-disable set**

Stop and ask. The audit produces a hypothesis; the user approves which entries actually move into Task 9's bundle.

---

## Task 9: Project-side cleanup bundle

**Files:**
- Modify: `D:\Repos\CkPlugins\Config\DefaultEngine.ini`
- Modify: `D:\Repos\CkPlugins\CkPlugins.uproject`

- [ ] **Step 1: Platform `.ini` cull — find the right key for UE 5.5**

The log lines we want to silence:
```
LogConfig: Display: Loading VulkanPC ini files took 0.10 seconds
LogConfig: Display: Loading Mac ini files took 0.10 seconds
... (8 more platforms)
```

Search the engine source for the actual config key that drives this:

```bash
grep -rn "PlatformsToLoad\|LoadAllPlatformConfigs\|ConfigSystemPlatforms\|LoadConfigForPlatform" /d/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp 2>/dev/null | head -20
grep -rn "Mac.*ini\|VulkanPC.*ini" /d/Repos/UnrealEngineAngelscript/Engine/Source/Runtime/Core/ 2>/dev/null | head -10
```

This is a discovery step. The key name varies by UE version. Likely candidates in 5.5: `[ConsoleVariables]`, `[Core.System] +ConfigPlatformsToLoad=Windows`, or `[Core.PlatformConfigCache]`. Record the actual key found.

- [ ] **Step 2: Add the platform cull setting and verify**

In `D:\Repos\CkPlugins\Config\DefaultEngine.ini`, after the existing `[SystemSettings]` section, add the discovered key. Example shape (replace with what Step 1 found):

```ini
[Core.System]
+ConfigPlatformsToLoad=Windows
```

Run a single editor cycle (no test):

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --output=Saved/Logs/Build-platform-cull.log --project='D:\Repos\CkPlugins'"
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --measure --output=Saved/Logs/Test-platform-cull.log --project='D:\Repos\CkPlugins'"
```

Then check:
```bash
grep -c "Loading .* ini files took" /d/Repos/CkPlugins/Saved/Logs/Test-platform-cull.log
```

Expected: dramatically fewer matches than baseline (ideally 1-2 for Windows only, vs ~10 for all platforms). If the count is unchanged, the key didn't work — try a different key from Step 1's candidates. If the build broke or tests fail, revert this single edit.

- [ ] **Step 3: Disable confirmed engine plugins**

For each plugin in the user-confirmed list from Task 8 Step 3, add an entry to the `Plugins` array of `D:\Repos\CkPlugins\CkPlugins.uproject`:

```json
{
    "Name": "<PluginName>",
    "Enabled": false
}
```

**Do one plugin at a time.** After each addition:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/BuildTest-plugin-<name>.log --project='D:\Repos\CkPlugins'"
```

Expected: build+test passes with 19/19 tests green and a `.startup.json` is produced. If the toolbox fails with `Plugin X failed to load` or similar, **revert that one entry** and move to the next candidate. Record both successes and reverts in a scratch list.

- [ ] **Step 4: Misc DefaultEngine.ini sweep (optional, conservative)**

Only land what the user explicitly approves from the Task 8 candidate list. Example:

```ini
[/Script/LiveCoding.LiveCodingSettings]
bEnabled=False
```

Verify after each addition with another build+test cycle.

- [ ] **Step 5: Compare bundle against baseline**

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-baseline.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-plugin-<last>.startup.json
```

Expected: `total_to_pie_ready_seconds` is lower than baseline. Record by how much in the commit message of Step 6.

- [ ] **Step 6: Commit**

```bash
cd /d/Repos/CkPlugins
git add Config/DefaultEngine.ini CkPlugins.uproject
git commit -m "perf(startup): cull platform .ini loads + disable unused engine plugins

Baseline build+test --test-pattern IskmRenderer showed ~X seconds
spent in:
  - 10x platform .ini loads (~1.2s)
  - module init for <N> plugins the host project never uses

Restrict platform .ini loading to Windows only; disable <list>. Tests
remain green (19/19 IskmRenderer). Total wall-clock saving: <Ys>."
```

(Replace `<X>`, `<N>`, `<list>`, `<Ys>` with the actual values from the compare output.)

---

## Task 10: AS double-reload investigation

**Files:**
- Create: `D:\Repos\CkPlugins\docs\superpowers\plans\2026-05-13-as-double-reload-findings.md`
- (Possibly) Modify: `D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\<file>` — only if fix is ≤50 lines.

- [ ] **Step 1: Identify the two reload sites in the engine source**

The log shows two AS reload passes — call them "Pass A" (around log time +6s, ends with `post full reload took 178 ms`) and "Pass B" (around log time +24s, ends with `==script reload total == took 2700 ms`).

Pass B is preceded by `==script reload total ==` which is the marker for `FAngelscriptCodeModule::ReloadScripts` (or equivalent — confirm in source).

Search for the trigger sites:

```bash
grep -rnE "class generator reload|script reload total|post full reload" /d/Repos/UnrealEngineAngelscript/Engine/Plugins/Angelscript/Source/ 2>/dev/null
```

Then find what calls each path. Likely entry points:
- `FAngelscriptCodeModule::StartupModule`
- `FAngelscriptCodeModule::ReloadScripts` / `ReloadAll`
- `FAngelscriptManager::Initialize` / `BindRegistered`
- Editor delegates: `FEditorDelegates::OnEditorBoot`, `OnPostEngineInit`, `OnAllSourceCodeMapsLoaded`

- [ ] **Step 2: Add log breadcrumbs to confirm the call chain**

Temporarily, in the Angelscript engine fork, add `UE_LOG(LogAngelscript, Display, TEXT("AS-RELOAD-TRIGGER: %s called from %s"), TEXT(<site>), TEXT(<context>));` at each candidate trigger site identified in Step 1. Rebuild the engine plugin only (not the whole editor target — should be a quick incremental build). Run:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --test --test-pattern IskmRenderer --output=Saved/Logs/Test-as-trace.log --project='D:\Repos\CkPlugins'"
grep "AS-RELOAD-TRIGGER" /d/Repos/CkPlugins/Saved/Logs/Test-as-trace.log
```

You should see two lines, one for Pass A and one for Pass B. Note the trigger context.

- [ ] **Step 3: Determine the root cause and write findings**

Possible outcomes (record the actual one in the findings doc):

- **(a) Editor fires the same delegate twice during init.** E.g. `OnPostEngineInit` and `OnPostInitProperties` both end up causing a reload. Fix: guard the second reload with a `bReloadAlreadyRan` flag. Likely <20 lines.
- **(b) The two reloads do different work intentionally.** Pass A does "bind native types and register UFUNCTION wrappers"; Pass B does "post-content-load script recompile after assets are loaded". Both legitimate. Fix is structural — defer to Phase 2.
- **(c) Hot reload (live coding) integration fires a redundant reload.** Fix: skip the reload if hot reload isn't actually loaded.
- **(d) Something else.** Investigate.

Create `D:\Repos\CkPlugins\docs\superpowers\plans\2026-05-13-as-double-reload-findings.md`:

```markdown
# AS Double-Reload Investigation Findings

**Date:** 2026-05-13
**Author:** <implementer>
**Outcome:** [(a) one-line fix landed / (b) structural, deferred to Phase 2 / (c) hot-reload guarded / (d) other]

## Pass A (~log timestamp +6s)
- Triggered by: <delegate / call site>
- Work performed: <summary>

## Pass B (~log timestamp +24s)
- Triggered by: <delegate / call site>
- Work performed: <summary>

## Are the two passes doing the same work?
[Yes / No / Partially — what overlaps]

## Decision
[Fix here in ≤50 lines / Defer to Phase 2]

## If fixed: what changed?
[engine file / lines / commit hash]

## If deferred: what's the Phase 2 shape?
[Brief — Phase 2 spec will expand]
```

- [ ] **Step 4: Remove the temporary log breadcrumbs from Step 2**

If breadcrumbs from Step 2 are still in the engine fork, remove them. They were diagnostic only. Verify with another grep that they're gone.

- [ ] **Step 5: Decide and execute**

- If outcome is (b) — structural — commit ONLY the findings doc. STOP this task. Skip Task 11 too (no AS fix lands in Phase 1).
- If outcome is (a)/(c)/(d) AND the fix is ≤50 lines — proceed to Step 6.

- [ ] **Step 6: Land the fix (only if Step 5 decided yes)**

Edit the appropriate file in `D:\Repos\UnrealEngineAngelscript\…`. The fix shape depends on the outcome but here's an example for outcome (a):

```cpp
// In FAngelscriptCodeModule::PostEngineInit or wherever Pass B is triggered:
static bool bReloadAlreadyRanThisSession = false;
if (bReloadAlreadyRanThisSession) {
    return;
}
bReloadAlreadyRanThisSession = true;
// ... existing reload logic ...
```

Rebuild, re-test:

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/BuildTest-as-fix.log --project='D:\Repos\CkPlugins'"
```

Expected: 19/19 tests pass. Compare:

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-plugin-<last>.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-as-fix.startup.json
```

Expected: `as_class_gen_reload_2_seconds`, `as_post_full_reload_2_seconds`, `as_script_reload_total_seconds` drop close to zero (or to `null`-with-explanation, depending on what the fix does). Total wall-clock drops by ~5 sec.

- [ ] **Step 7: Commit the fix in the engine fork**

```bash
cd /d/Repos/UnrealEngineAngelscript
git add Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/<file>
git commit -m "fix(angelscript): elide redundant <pass-name> reload during editor init

Editor init was firing the same reload path twice: <root-cause summary>.
Pass A did <work>, Pass B repeated <same work>, costing ~5 sec on a
typical DebugGame launch. Guard the second invocation so it only runs
on actual hot-reload / live-coding triggers, not on initial boot."
```

Push it. (User runs the push if non-interactive auth blocks the agent.)

- [ ] **Step 8: Commit the findings doc in CkPlugins**

```bash
cd /d/Repos/CkPlugins
git add docs/superpowers/plans/2026-05-13-as-double-reload-findings.md
git commit -m "docs(plans): AS double-reload investigation findings"
```

---

## Task 11: AS debug-database deferral (conditional)

**Skip this task entirely if Task 10 produced a regression in `as_script_reload_total_seconds` or `as_class_gen_reload_*_seconds`.** Conditional: only land if Task 10 didn't make things worse. Re-check the comparison output from Task 10 Step 6 before starting.

**Files:**
- Modify: `D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\<debugger-server-file>` — exact file TBD via Step 1.

- [ ] **Step 1: Find where `Sending debug database` is logged**

```bash
grep -rn "Sending debug database" /d/Repos/UnrealEngineAngelscript/Engine/Plugins/Angelscript/Source/ 2>/dev/null
```

Expected: one or two hits. Read the surrounding code — it's the AS-debugger server pushing the type/symbol database. Identify the function and what gates it (probably nothing — it sends unconditionally on startup).

- [ ] **Step 2: Check if Hazelight already has a "debugger attached" or "lazy-send" flag**

```bash
grep -rnE "IsDebuggerAttached|bDebuggerConnected|HasClient|GetNumClients" /d/Repos/UnrealEngineAngelscript/Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/Debug 2>/dev/null
```

If a flag like `bDebuggerConnected` already exists, gate the send on it: send only when a client connects. If no such flag exists, add one (small — a bool plus the existing OnClientConnected callback).

- [ ] **Step 3: Modify the send call site to defer**

Example (replace with what Step 1/2 actually find):

```cpp
// Before:
void FAngelscriptDebuggerServer::OnEngineInitComplete() {
    SendDebugDatabase();  // ~740ms blocking
}

// After:
void FAngelscriptDebuggerServer::OnEngineInitComplete() {
    // Debug database is sent lazily on first client connect.
    // See FAngelscriptDebuggerServer::OnClientConnected.
}

void FAngelscriptDebuggerServer::OnClientConnected(/* …client info… */) {
    if (NOT bDebugDatabaseSent) {
        SendDebugDatabase();
        bDebugDatabaseSent = true;
    }
    // …existing client-handshake code…
}
```

**Budget reminder:** total change ≤30 lines. If it grows past that, revert and document as a Phase 2 candidate in the findings doc from Task 10.

- [ ] **Step 4: Verify**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/BuildTest-as-debugdb.log --project='D:\Repos\CkPlugins'"
```

Expected: 19/19 tests pass. The `Sending debug database` log line should NOT appear during the no-debugger-attached run.

```bash
grep -c "Sending debug database" /d/Repos/CkPlugins/Saved/Logs/BuildTest-as-debugdb.log
```

Expected: 0.

Compare:

```bash
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-as-fix.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-as-debugdb.startup.json
```

Expected: `as_debug_db_seconds` reports `—` (or `AFTER-MISSING` note) since the marker no longer fires. Total wall-clock drops by ~0.7s.

- [ ] **Step 5: Confirm debugger connection still works**

Manual check: launch the AS debugger client (VSCode extension or whatever you use), connect to a running editor. The debug DB should send on connect — verify by looking for the same log line *after* the connection event.

If the debugger can't get types after this change, the deferral broke type resolution. Revert and document as Phase 2 in the findings doc.

- [ ] **Step 6: Commit the engine fork change**

```bash
cd /d/Repos/UnrealEngineAngelscript
git add Engine/Plugins/Angelscript/Source/AngelscriptCode/Private/<debugger-file>
git commit -m "perf(angelscript-debugger): defer debug database send until first client connect

SendDebugDatabase was running unconditionally during editor init,
costing ~740ms even when no debugger client was ever going to connect.
Move the call into the OnClientConnected path, guarded by a one-shot
flag. Same payload reaches the client on first connect; idle editors
save the cost entirely."
```

Push it (user runs the push if non-interactive auth blocks the agent).

---

## Task 12: Final snapshot + success-criteria sign-off

**Files:**
- Modify: `D:\Repos\CkPlugins\docs\superpowers\specs\2026-05-13-editor-startup-phase1-design.md` — append final results.

- [ ] **Step 1: One final clean baseline cycle**

```bash
pwsh -Command "Set-Location 'D:\Repos\CkPlugins'; ./CkAuto/UnrealToolbox.exe --build --config=DebugGame --target=Editor --test --test-pattern IskmRenderer --measure --output=Saved/Logs/BuildTest-phase1-final.log --project='D:\Repos\CkPlugins'"
/d/Repos/CkPlugins/CkAuto/UnrealToolbox.exe --measure-compare \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-baseline.startup.json \
    /d/Repos/CkPlugins/Saved/Logs/BuildTest-phase1-final.startup.json
```

Expected: total wall-clock savings reflect the union of Tasks 9-11. Capture the delta table.

- [ ] **Step 2: Decide on Phase 2**

If remaining startup time is acceptable → Phase 1 ends. Done.
If meaningful headroom remains (e.g. you still see 15s+ in engine init, or asset-registry scans look chunky) → write a Phase 2 spec. Likely Phase 2 candidates from the original design:

- Tooltip backport (`SDeferredToolTip` from UE 5.8) — 1–5 sec, engine fork patch.
- Async / background AssetRegistry — high ceiling but substantial engine work.
- Plugin culling of our own Ck* set — UX impact tradeoff.

- [ ] **Step 3: Update the spec with final numbers**

Append a "Phase 1 Results" section to `D:\Repos\CkPlugins\docs\superpowers\specs\2026-05-13-editor-startup-phase1-design.md`:

```markdown
## Phase 1 Results (filled in on completion)

**Baseline (2026-05-13):** total_to_pie_ready_seconds = X.Xs
**After Phase 1:**         total_to_pie_ready_seconds = Y.Ys
**Saving:** Z.Zs (P%)

Per-phase breakdown:
| Phase | Before | After | Delta |
|---|---|---|---|
| engine_init_seconds        | … | … | … |
| as_bindings_seconds        | … | … | … |
| as_class_gen_reload_1      | … | … | … |
| as_class_gen_reload_2      | … | … | … |
| as_script_reload_total     | … | … | … |
| as_debug_db_seconds        | … | … | … |
| **total_to_pie_ready**     | … | … | … |

Phase 2 decision: [pursue / not pursue]. Reasoning: <…>.
```

- [ ] **Step 4: Commit**

```bash
cd /d/Repos/CkPlugins
git add docs/superpowers/specs/2026-05-13-editor-startup-phase1-design.md
git commit -m "docs(spec): record Phase 1 startup-optimisation results"
```

Push.

---

## Self-Review Notes

**Spec coverage:**
- Deliverable 1 (measurement) → Tasks 1-7 (CLI flags, struct/API skeleton, failing test, engine-init extraction, AS-marker extraction, baseline capture, compare implementation).
- Deliverable 2 (project bundle) → Tasks 8-9 (audit, then bundle).
- Deliverable 3 (AS investigation) → Task 10.
- Deliverable 4 (AS debug-DB) → Task 11.
- Final sign-off → Task 12.

All four spec deliverables have at least one task.

**Conditional sequencing matches spec:** Task 11 explicitly skips if Task 10 produced a regression. Findings doc is required output regardless of fix-or-defer.

**Budget gates from spec are preserved:** ≤50 lines for Task 10 fix, ≤30 lines for Task 11 fix. Both have explicit "revert and document" branches.

**TDD pattern:** Task 3 is "write the failing test"; Task 4 implements until first two tests pass; Task 5 implements until all tests pass. Task 7 also follows TDD (test first, then implement).

**No placeholders in test code or scripts.** `<file>` / `<list>` / `<X>` markers only appear inside steps framed as "replace with what discovery finds" — they're explicit substitution points after the discovery step that surfaces the value, not unfilled blanks the engineer should ignore.

**Type consistency:** Phase names match across all tasks (`engine_init_seconds`, `as_class_gen_reload_1_seconds`, `total_to_pie_ready_seconds`, etc.). API signatures introduced in Task 2 (`Parse_StartupMeasurement` / `Emit_StartupSnapshot` / `Run_MeasureCompare`) are used unchanged in later tasks.
