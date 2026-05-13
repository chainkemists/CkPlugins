# Editor Startup — Phase 1 Design

**Date:** 2026-05-13
**Author:** Sulfur-CK (with assistance)
**Status:** Approved for implementation planning

---

## Goal

Reduce the wall-clock from `./CkAuto/UnrealToolbox.exe --build --test` invocation to "tests started running" — currently ~26 sec of post-build startup on a fresh editor spawn. The build/link phase is out of scope (different bottleneck, different solutions). What we attack is everything between "editor process exec'd" and "PIE up, AS reloaded, tests running".

**Baseline metric:** Toolbox `--build --test` cycle, DebugGame config, IskmRenderer pattern (the loop we already spend our day in). Each deliverable below is validated against that baseline using the measurement layer we ship in deliverable #1.

## Current State (one-run snapshot, 2026-05-13)

| Stage | Wall-clock |
|---|---|
| Editor process start → `Engine is initialized` | ~18 sec |
| AS binding registration (`blueprinttype bindings`) | 457 ms |
| AS bindings total | 1.57 sec |
| AS class generator reload (1st pass) | 1.51 sec |
| AS script compilation total (1st pass) | 566 ms |
| AS post-full-reload (1st pass) | 178 ms |
| AS script reload (2nd full pass) | 2.70 sec |
| AS class generator reload (2nd pass) | 996 ms |
| AS new class propagation + post-reload (2nd pass) | 2.05 sec |
| AS sending debug database | 739 ms |
| **Total to "actually ready"** | **~26 sec** |

Two observations driving Phase 1 priorities:

1. About half of the post-engine-init time is AngelScript, and AS runs a full class-generator reload **twice** during startup. That smells like a fixable pattern, not load-bearing work — worth investigating cheaply before committing to engine work.
2. Engine init itself is ~18 sec. Some of that is platform `.ini` loading (10 platforms × ~0.10s = ~1.2s) and engine-plugin module init for plugins we don't use. Cheap project-side cleanup may shave 1–3 sec there.

## In Scope (Phase 1)

1. **Measurement plumbing** — without it, every change is anecdotal. We need a deterministic way to extract the timing markers from the log and compare two runs.
2. **Project-side bundle** — platform `.ini` cull, engine-plugin disable list, `.uproject` cleanup. Independent, low-risk, ships as one logical unit.
3. **AS double-reload investigation** — find what fires the second full reload during init. May produce a fix; may produce a "this is structural, defer to Phase 2" finding. Either is a legitimate outcome.
4. **AS debug-database deferral** — 0.74 sec saved if we defer until first AS-debugger connection. Small Hazelight-side change. Conditional on deliverable #3's findings (don't apply if #3 already regresses AS reload time).

## Out of Scope (Phase 2 candidates)

- Engine-fork tooltip backport (`SDeferredToolTip` from UE 5.8) — 1–5 sec if relevant in our shape, requires engine patch carry.
- Background / async AssetRegistry — high ceiling but substantial engine work.
- Aggressive culling of our own `Ck*` plugins — UX impact (the plugin's tooling disappears from this host project).
- Anything that requires touching downstream consumer projects (BusterBlock, Rewind99, etc.).

Phase 2 will get its own spec once Phase 1 measurements expose where the remaining headroom is. The trigger for kicking off Phase 2 is "Phase 1 measurements show we still want more". If Phase 1 already lands us where we want to be, Phase 2 doesn't happen.

## Components / Deliverables

Four atomic deliverables. Each is independently shippable. Ordering matters because the measurement layer underpins verification of the others.

### Deliverable 1 — `CkAuto/Measure-Startup.ps1` + `Compare-Startup.ps1`

A PowerShell script that parses `Saved/Logs/CkPlugins.log` and emits a JSON timing snapshot:

```json
{
  "run_id": "2026-05-13_17-26",
  "config": "DebugGame",
  "phases": {
    "engine_init_seconds": 18.1,
    "as_bindings_seconds": 1.57,
    "as_reload_1_seconds": 1.51,
    "as_reload_2_seconds": 2.70,
    "as_debug_db_seconds": 0.74,
    "total_to_pie_ready_seconds": 26.1
  }
}
```

Markers extracted by regex against existing UE log lines (`Engine is initialized`, `Angelscript: == bindings total ==`, `Angelscript: class generator reload`, `Angelscript: ==script reload total ==`, `Sending debug database took`). All these lines exist in the log today — no new instrumentation needed.

A new `--measure` flag on `UnrealToolbox.exe` writes the JSON alongside the log at `Saved/Logs/Startup-<timestamp>.json`. A second script `Compare-Startup.ps1` diffs two snapshots and prints a delta table.

**Contract:** zero editor-side instrumentation. Pure log-parsing. If a future UE version changes log wording, the parser breaks loudly (via Pester test) rather than silently producing wrong numbers.

### Deliverable 2 — Project-side cleanup bundle

One PR landing three independent edits:

- **`Config/DefaultEngine.ini` — restrict platform `.ini` loading.** The 10 `Loading <platform> ini files took ~0.10 seconds` lines = ~1.2 sec we don't need on a Windows-only dev host. Exact setting name and key to confirm during implementation (likely `[Core.System] PlatformsToLoad=Windows` or equivalent in 5.5).
- **`CkPlugins.uproject` — engine-plugin disable list.** Audit candidates against the current `.uproject`, propose a list, user confirms before flipping any. Likely targets based on the "plugin host project, no shipping content" workload: `XRBase`, `OpenXR`, `OnlineFramework`, `MediaCompositing`, `LegacyEditorWidgets`, anything Composure/AR/VR/Mobile-specific. **Not** flipping plugins our `Ck*` set depends on (e.g. CommonUI stays).
- **`Config/DefaultEngine.ini` — sweep for other concrete settings.** E.g. disable Live Coding auto-load if it's not used during toolbox runs; disable any verbose-by-default log categories that contribute zero useful information. On a "found something concrete with a clear win, propose it" basis — not a blanket cleanup.

Plugin toggles are flipped **one at a time** during the audit pass, with a toolbox run after each, only the confirmed-safe ones batched into the bundle PR.

### Deliverable 3 — AS double-reload investigation

The log shows AS does a `class generator reload (1.51s) → post full reload (178ms)` and then **immediately** does another `script reload total (2.70s) → class generator reload (996ms) → post full reload (1.41s)`. That's ~5 sec of redundant work if both passes do the same thing.

Investigation only at this stage:

1. Read `FAngelscriptCodeModule` and `FAngelscriptManager` reload paths in the engine plugin (`D:\Repos\UnrealEngineAngelscript\Engine\Plugins\Angelscript\Source\AngelscriptCode\Private\`).
2. Identify what triggers the second reload (typical suspects: editor delegate fires twice during init; first reload is "discovery", second is "post-discovery rebind"; precompile callbacks running synchronously twice).
3. Write findings as a short doc inside `docs/superpowers/plans/` or as a comment on the implementation plan.
4. **Output:** either a one-line fix (Hazelight has a guard we can flip, or a single redundant call we can elide) **or** a "this needs structural work, deferring to Phase 2" recommendation.

**Budget for a Phase 1 fix:** ≤50 lines of engine-code change. Anything bigger is a Phase 2 conversation.

### Deliverable 4 — AS debug-database deferral (conditional)

`Sending debug database took 0.739 seconds` runs at startup unconditionally. The debug DB is only used by the AS debugger client. Goal: defer the send until the first debugger connection (or skip entirely when no debugger is attached).

This is a one-call-site change in the AS plugin if Hazelight already has a "debugger attached?" check; potentially a small new flag if not. **Budget:** ≤30 lines of engine-fork change. Conditional on Deliverable #3's outcome (skip if #3 already regressed AS reload time).

## Data Flow

Once Phase 1 is in, the loop for verifying any change becomes:

```
1. ./CkAuto/UnrealToolbox.exe --build --test --test-pattern <whatever> --measure
   → writes Saved/Logs/Startup-<timestamp>.json next to the log
2. Make change (config edit, .uproject toggle, engine patch)
3. Rebuild if engine-side, otherwise skip rebuild
4. ./CkAuto/UnrealToolbox.exe --test --test-pattern <same> --measure
5. ./CkAuto/Compare-Startup.ps1 Saved/Logs/Startup-<before>.json Saved/Logs/Startup-<after>.json
   → prints a delta table; non-trivial regression on any phase = flag for review
```

Single observability surface. No editor-side instrumentation, no DTrace, no profile captures — just parsed log markers. Cheap to add new markers as we discover them (e.g. if Phase 2 wants asset-registry timing, we add the regex and re-snapshot, no engine changes needed).

## Error Handling

### Measurement layer

- **Marker missing from log** (e.g. UE version bump changed wording) → script logs `"phase": null` for that phase and keeps going. Compare script flags missing-on-one-side phases explicitly so we don't call a regression a fix.
- **Two runs configured differently** (Debug vs Development) → snapshots include the `--config` flag, compare refuses to diff and prints why.
- **Editor crashed mid-startup** → no `Engine is initialized` marker → script exits non-zero with a clear "this run didn't reach engine init" message rather than emitting a fake snapshot.

### Project-side bundle

- **Disabled plugin had a transitive dependency we didn't know about** → editor fails to launch with `Plugin X failed to load` → revert the offending toggle, no commit lands. Mitigated by toggling plugins one at a time during the audit pass.
- **Platform .ini cull breaks something Windows-specific** → tests fail or editor logs new warnings → revert that one line.

### AS investigation

- **Root cause is structural** → output is "defer to Phase 2" with a one-page write-up. Zero engine code lands. No regression risk.
- **Root cause is a single redundant call** → small fix lands in Hazelight engine-fork branch + version bump + version-log row. Measurement layer immediately validates the saving.

## Testing

- **Deliverable 1** unit-testable in isolation: feed it the existing `CkPlugins.log` from the repo, assert the JSON fields. One PowerShell Pester test in `CkAuto/Tests/`. Catches "regex drifted from current UE log wording".
- **Deliverable 2** — toolbox build+test must still complete cleanly. Existing IskmRenderer test set is sufficient as a smoke test. No new tests needed for `.ini` / `.uproject` edits.
- **Deliverable 3** — no code = no tests. If a fix lands, it must keep the IskmRenderer test set green AND not introduce new AS startup-time markers worse than baseline.
- **Deliverable 4** — same as #3. Additional smoke: connect the AS debugger after deferral and confirm DB still gets sent on connection.

## Sequencing

1. Land Deliverable 1 first. Capture baseline snapshot.
2. Land Deliverable 2 in one PR. Re-snapshot.
3. Do Deliverable 3 as discovery. Decision point at end: land Phase 1 fix or defer to Phase 2.
4. Do Deliverable 4 if #3 didn't already produce a regression in AS reload time.

After all four, re-snapshot one more time. Compare against baseline. If we're satisfied, Phase 1 ends. If meaningful headroom remains, kick off Phase 2 spec.

## Success Criteria

Phase 1 ships when:

- Deliverables 1 and 2 are merged and verified.
- Deliverable 3 has produced either a merged fix or a Phase 2 deferral write-up.
- Deliverable 4 has produced either a merged fix or an explicit skip decision.
- Final snapshot shows the toolbox build+test cycle is faster than baseline. No specific second target — even a 2-3 sec save on a 26 sec cycle is meaningful given the rate at which we run this loop.
