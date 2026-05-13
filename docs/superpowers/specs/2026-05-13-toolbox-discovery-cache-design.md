# Toolbox Test-Discovery Cache — Phase 2 Design

**Date:** 2026-05-13
**Author:** Sulfur-CK (with assistance)
**Status:** Approved for implementation planning

**Prior phase:** [Phase 1](2026-05-13-editor-startup-phase1-design.md). Phase 1 shipped measurement infrastructure (`--measure` / `--measure-compare`) and disabled 16 unused engine plugins for a -1.339s engine-init saving. Phase 1's Task 10 discovery revealed the real opportunity: the toolbox `--test` flow spawns **two** editor cold-starts per cycle (one for `Automation List`, one for `Automation RunTests`). This spec addresses that.

---

## Goal

Eliminate the ~17 second test-discovery editor cold-start when iterating `--test` on a known build. The toolbox currently spawns two editor processes per `--test` invocation:

1. **Discovery phase** — spawns editor with `-ExecCmds="Automation List; Quit"` to enumerate every test in the project. Used for client-side pattern matching.
2. **Run phase** — spawns editor with `-ExecCmds="Automation RunTests <list>; Quit"` to actually execute.

Each spawn pays ~17 sec of engine init. The discovery phase is unnecessary when `Settings.Tests.DiscoveredTests` already contains a fresh list — the typical iteration loop after a build.

**Baseline metric:** `total_to_pie_ready_seconds` from the Phase 1 measurement infrastructure. Expected delta on the second-and-later iteration cycles: **~-17 sec**. First-run and post-build cycles see no change (discovery still runs).

## Current State

Per `D:\Repos\FtxUiFramework\apps\UnrealToolbox\src\UnrealToolbox_Main.cpp:448-458` (the `RunHeadlessTest` flow):

```cpp
// 3. Discover (always re-discover for headless — keeps the cache fresh)
Sink("Discovering tests…");
auto Disc = utb::DiscoverTests(Engine, Project, Settings.Tests, Settings.Build.Config, Sink);
if (NOT Disc.Success) { /* error */ return 1; }
Settings.Tests.DiscoveredTests   = Disc.DottedPaths;
Settings.Tests.LastDiscoveredUnix = static_cast<int64_t>(...);
```

The comment acknowledges the choice ("always re-discover for headless — keeps the cache fresh"). Phase 1's Task 10 finding makes the cost concrete: ~17 sec/cycle, on every `--test` invocation, regardless of whether the test list could possibly have changed since the last cycle.

`Settings.Tests.DiscoveredTests` is already persisted via `FSettingsManager`. The infrastructure for caching exists; only the consumption side ignores it.

## In Scope (Phase 2)

1. **CLI flag:** `--discover-fresh` (sub-flag of `--test`) — explicit user escape hatch when they want to force discovery (e.g. they added/removed tests in code without `--build`).
2. **`RunHeadlessTest` decision logic:** four-way branch that decides whether to discover or use the cached list. Cache invalidation rule: re-discover only when `--build` was in this invocation, when `--discover-fresh` is set, or when the cache is empty (first run). Otherwise trust the cache.
3. **Decision function extraction + Catch2 tests:** the decision tree is small but high-value to verify. Extracting it into a pure function (`Decide_TestDiscoveryMode`) keeps the four-way branch testable without spinning up an editor.

## Out of Scope (Phase 3 candidates)

- Combining `Automation List` + `Automation RunTests` into a single `-ExecCmds` chain (the more aggressive alternative; collapses two spawns into one for the post-build cycle too).
- Auto-fall-back to discovery when the pattern filter yields zero matches (hides stale-cache problems; the `--discover-fresh` flag is the explicit contract instead).
- Cache-validation against deleted tests pre-run (`Automation RunTests` already surfaces `TestResult=Failed` for missing names; the toolbox doesn't need to be smarter).
- Platform `.ini` cull engine patch (~1.2s — Phase 3).
- UE 5.8 tooltip backport (1-5s — Phase 3).
- Re-enabling AS timing markers (observability only — Phase 3).

## Components / Deliverables

Three atomic deliverables. All toolbox-side. No engine fork, no submodule changes.

### Deliverable 1 — `--discover-fresh` CLI flag

Add to `FCliArgs`:

```cpp
// --discover-fresh: sub-flag of --test. Forces test discovery to re-run
// even if a cached test list exists. Use after manually adding/removing
// tests without a --build, or to reset a stale cache.
bool DiscoverFresh = false;
```

Register in `Parse_Args` via CLI11's `add_flag(...)->needs(TestOpt)`. Trivial — same shape as the existing `--measure` flag registration.

### Deliverable 2 — Decision function + tests

Extract the cache-vs-discover decision into a pure function so it's testable without spinning up the editor:

```cpp
// In include/utb/TestRunner.hpp (or a new TestDiscoveryDecision.hpp if
// TestRunner.hpp is already too crowded — decide during implementation).

enum class ETestDiscoveryMode { UseCache, DiscoverFresh };

struct FTestDiscoveryInputs
{
    bool DiscoverFreshRequested;  // --discover-fresh flag set
    bool BuildJustRan;            // --build in this invocation
    bool CacheEmpty;              // Settings.Tests.DiscoveredTests is empty
};

// Returns DiscoverFresh if ANY of:
//   - DiscoverFreshRequested
//   - BuildJustRan
//   - CacheEmpty
// Returns UseCache only when all three are false.
auto Decide_TestDiscoveryMode(const FTestDiscoveryInputs& In) -> ETestDiscoveryMode;
```

Tests (Catch2, in `tests/UnrealToolbox_TestDiscoveryDecision.cpp`):
- All three inputs false → `UseCache`
- `DiscoverFreshRequested=true`, others false → `DiscoverFresh`
- `BuildJustRan=true`, others false → `DiscoverFresh`
- `CacheEmpty=true`, others false → `DiscoverFresh`
- All three true → `DiscoverFresh` (sanity)

Five cases. Pure function, no dependencies. Catches regressions in the decision tree without requiring an end-to-end smoke run.

### Deliverable 3 — `RunHeadlessTest` integration

Replace `UnrealToolbox_Main.cpp:448-458` with the decision-driven flow:

```cpp
// 3. Decide: discover, or use cache?
const auto DecisionInputs = utb::FTestDiscoveryInputs{
    .DiscoverFreshRequested = InArgs.DiscoverFresh,
    .BuildJustRan           = InArgs.Build,
    .CacheEmpty             = Settings.Tests.DiscoveredTests.empty(),
};
const auto Decision = utb::Decide_TestDiscoveryMode(DecisionInputs);

auto Tests = std::vector<std::string>{};
if (Decision == utb::ETestDiscoveryMode::DiscoverFresh)
{
    // Build a human-readable reason for the log line (helps post-hoc
    // debugging of "why did this run discover?").
    auto Reason = std::string{};
    if      (InArgs.DiscoverFresh)                       Reason = "--discover-fresh requested";
    else if (InArgs.Build)                               Reason = "--build in this invocation; test list may have changed";
    else if (Settings.Tests.DiscoveredTests.empty())     Reason = "no cached test list";

    Sink("Discovering tests… (" + Reason + ")");
    auto Disc = utb::DiscoverTests(Engine, Project, Settings.Tests, Settings.Build.Config, Sink);
    if (NOT Disc.Success)
    {
        Sink("Discovery failed: " + Disc.ErrorMessage);
        return 1;
    }
    Tests = Disc.DottedPaths;
    Settings.Tests.DiscoveredTests   = Tests;
    Settings.Tests.LastDiscoveredUnix = static_cast<int64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}
else
{
    Tests = Settings.Tests.DiscoveredTests;
    Sink("Using cached test list (" + std::to_string(Tests.size()) + " tests)");
}

// 4. Resolve test set — unchanged. ResolveTestsToRun reads Settings.Tests
//    so the cached list is what gets filtered.
const auto Prefixes = NOT Settings.Tests.ProjectPrefixes.empty()
    ? Settings.Tests.ProjectPrefixes
    : utb::AutoDeriveProjectPrefixes(Project.ProjectPath);

const auto ToRun = ResolveTestsToRun(InArgs, Settings.Tests, Prefixes);
// ... existing 'if (ToRun.empty()) ...' branch unchanged ...
```

The `Sink` lines are load-bearing for debuggability: post-hoc when a user wonders why a particular run did or didn't discover, the log line names the trigger.

`Settings.Tests.LastDiscoveredUnix` is updated only when discovery actually runs. The "last discovered" age is a future polish (e.g. printing "Using cached test list (203 tests; last discovered 5m ago)") — leave as raw timestamp for Phase 2; a relative-time formatter can land later if useful.

## Data Flow

**First run (or post `--build`):**

```
toolbox --build --test --test-pattern IskmRenderer
  ├── (build)
  ├── Decide_TestDiscoveryMode → DiscoverFresh (BuildJustRan=true)
  ├── Sink: "Discovering tests… (--build in this invocation; test list may have changed)"
  ├── DiscoverTests   ── spawn editor #1 → Automation List ──┐
  │                    cache populated                       │
  ├── filter cached list against "IskmRenderer" ─────────────┤
  └── RunTests       ── spawn editor #2 → run 19 tests ─────┘
```

Same wall-clock as today (~64 sec).

**Iteration on the same build:**

```
toolbox --test --test-pattern IskmRenderer
  ├── Decide_TestDiscoveryMode → UseCache (all three false)
  ├── Sink: "Using cached test list (203 tests)"
  ├── filter cached list against "IskmRenderer"
  └── RunTests       ── spawn editor → run 19 tests
```

One editor spawn instead of two. ~-17 sec.

**Explicit re-discovery:**

```
toolbox --test --discover-fresh --test-pattern IskmRenderer
  ├── Decide_TestDiscoveryMode → DiscoverFresh (DiscoverFreshRequested=true)
  ├── Sink: "Discovering tests… (--discover-fresh requested)"
  └── (same as first run)
```

## Error Handling

### Discovery fails (editor crash during `Automation List`)

Existing behavior: `Sink("Discovery failed: …")`, return 1. The cache is NOT updated. A subsequent `--test` either still has empty cache (cache-empty branch fires → tries discovery again) or has a stale-but-non-empty cache (uses it). Either is acceptable.

### Cache corrupted (settings.json malformed)

`FSettingsManager` already handles malformed settings by returning defaults. `Settings.Tests.DiscoveredTests` ends up empty → cache-empty branch fires → fresh discovery. No new code needed.

### Pattern matches 0 tests in cache (stale cache or typo)

`ResolveTestsToRun` returns empty → existing `if (ToRun.empty())` branch prints "No tests matched — nothing to run." User chooses between `--discover-fresh` (likely fix if a test was added) or correcting the pattern. No silent fallback.

### `Automation RunTests` rejects a stale test name

Editor exits with `TestResult=Failed` for that name. Surfaces in the test summary. The toolbox does not need to validate test names against the engine pre-run.

## Testing

### Decision-function unit tests (Catch2)

Five cases (see Deliverable 2). Run as part of `FtxUnitTests`. No editor required.

### End-to-end smoke (manual; documented in plan)

1. From a clean state (`Settings.Tests.DiscoveredTests` empty), run `--test --test-pattern IskmRenderer`. Expect: discovers, runs 19 tests. Snapshot via `--measure`.
2. Re-run the same command. Expect: prints "Using cached test list", skips discovery, runs 19 tests. Snapshot.
3. Run `--build --test --test-pattern IskmRenderer`. Expect: prints "Discovering tests… (--build in this invocation)", full cycle.
4. Run `--test --discover-fresh --test-pattern IskmRenderer`. Expect: prints "Discovering tests… (--discover-fresh requested)", full cycle.

### Measurement

Compare snapshots from step 1 (baseline iteration) and step 2 (cached iteration) using `--measure-compare`. Expected: `total_to_pie_ready_seconds` drops by ~17 sec; `engine_init_seconds` shows little or no change (that's the per-process editor init, only paid once now).

## Success Criteria

Phase 2 ships when:

- All three deliverables merged and verified.
- Catch2 decision-function tests pass.
- Smoke step #2 shows the "Using cached test list" log line.
- Snapshot comparison shows `total_to_pie_ready_seconds` reduction of ~15-20 sec on the iteration cycle.
- The full IskmRenderer test set (19 tests) still passes from both fresh-discovery and cached paths.

## Sequencing

1. **Deliverable 1** (CLI flag) — small, no logic. Ships first to unblock Deliverable 3.
2. **Deliverable 2** (decision function + tests) — TDD red phase then green. Pure function so tests are fast.
3. **Deliverable 3** (RunHeadlessTest integration) — touches the user-facing flow. Smoke-tested end to end with measurements.
4. **Final snapshot + sign-off** — capture before/after snapshots, append results to this spec.

## Phase 2 Results (filled in on completion)

**First-run cycle (--build --test, cache invalidated):**

| Phase | Phase 1 final | Phase 2 final | Delta |
|---|---|---|---|
| engine_init_seconds | 15.919 | 17.788 | +1.869 |
| total_to_pie_ready_seconds | 64.206 | 64.565 | +0.359 |

(Expected: roughly unchanged. First-run still pays both spawns. Observed: well within run-to-run noise — confirms Phase 2 does not regress the post-build cycle.)

**Iteration cycle (--test alone, cached):**

| Phase | --build cycle | cached cycle | Delta |
|---|---|---|---|
| engine_init_seconds | 17.788 | 18.479 | +0.691 |
| total_to_pie_ready_seconds | 64.565 | 31.621 | -32.944 |

(Expected: total_to_pie_ready_seconds drops by ~15-20 sec. Observed: -32.944s / -51.0% — well past target. One full editor cold-start eliminated, plus the discovery-phase Automation List/exit overhead removed.)

**Combined Phase 1 + Phase 2 saving on iteration cycle:**

BuildTest-bundle.startup.json (Phase 1 final post-build cycle) total: 64.206s
Phase2-final-cached.startup.json total: 31.621s
Saving: 32.585 sec (-50.8%)

Note: Phase 1's `BuildTest-baseline.startup.json` (the earliest pre-Phase-1 baseline) sat at 64.669s. Comparing that to Phase 2 cached (31.621s) yields 33.048s saving across both phases — Phase 1 itself was a wash on `total_to_pie_ready_seconds` (the -1.339s engine_init saving did not materialise at the wall-clock level), so essentially the entire iteration-cycle improvement is Phase 2.

Tests: 19/19 IskmRenderer pass on every smoke-test in Tasks 5 and 6 (cache-empty, cached, build-trigger, discover-fresh, final-postbuild, final-cached).

Phase 3 decision: **deferred — revisit if iteration-cycle ergonomics demand it**. Reasoning: the measured 32.9s saving on iteration cycles already exceeds the Phase 2 target by ~2x and crosses the perceptual "feels fast" threshold for tight inner-loop work; the remaining Phase 3 candidates (single-process flow ~30s further, engine-fork .ini cull ~1.2s, UE 5.8 tooltip backport 1-5s, AS timing-marker restoration) each add complexity (engine-fork patches need maintenance across UE upgrades; single-process flow rewires the toolbox `--test` pipeline) and the next sensible decision point is after we've lived with the Phase 2 win for a few real iteration loops to see whether the remaining ~31s/cycle is actually painful.

---

## Phase 3 (deferred)

After Phase 2 ships, remaining candidates from Phase 1's sign-off:

- Toolbox single-process flow via combined `-ExecCmds` (saves the post-`--build` spawn too).
- Platform `.ini` cull engine patch (~1.2s, engine fork).
- UE 5.8 tooltip backport (1-5s, engine fork).
- Re-enable AS timing markers in the engine fork (observability — recovers the AS phases that disappeared between May 5 and May 13 in our `--measure` output).

Phase 3 spec only happens if Phase 2 leaves meaningful headroom worth pursuing.
