# PROGRESS — Jolt Collision World campaign

Append-only, dated. Newest entries at the bottom of each day. The ONLY home for volatile state.

## 2026-07-16

- Campaign started (autonomous session, user AFK, all decisions delegated).
- Research phase complete: 3 exploration reports (CkSpatialQuery map, Jolt vendoring + consumer audit,
  conventions survey) + UnrealJolt reference analysis. Key corrections vs the requirements doc:
  - CkSpatialQuery is NOT the sole `JPH::` consumer — CkEqs reads the registry context and calls
    CkProbeTrace_Utils' JPH-typed overloads (re-homed in Phase 2, untouched in Phase 0).
  - CkAggro does NOT depend on CkSpatialQuery (doc claim stale).
  - Engine is UnrealEngine-Angelscript 5.7.x (not 5.5/5.6).
  - `JPH_OBJECT_STREAM` compiled out; binary StreamOut/SaveWithChildren available — sufficient.
- Design phase complete: Phase 0 / Phases 1-2 / Phases 3-5 designs locked (see PHASE_N.md files).
- Plan approved. Git baseline: CkPlugins dev @ 6ec3cb7, CkFoundation dev @ 02f404171 (clean),
  CkTests dev @ 1f13cae (clean). Pre-existing dirty submodule pointers (CkAuto, CkGameplayDebugger,
  GitLink) belong to another session — never staged by this campaign.
- Baseline build+test run started (Development Editor, full suite) — results pending.
- **BASELINE CAPTURED** (Development Editor, full suite, 26m04s): **783 total / 782 passed / 1 failed / 0 skipped**.
  The one pre-existing red: `Ck_AutoTest_Crowd_PathRefresh_InsideBandPlansOut` ("walker's fresh plan never
  exited the band after 20 polls — worst clearance 39.77uu, need 50uu") — CkCrowd nav-band test, unrelated
  to Jolt, red BEFORE any campaign edit. Every later "no regressions" claim = 782 pass + only this red.
  Log: Saved/Logs/Baseline-BuildTest.log @ CkFoundation 02f404171 / CkTests 1f13cae.

- **Phase 0 authored + build GREEN first attempt** (Development Editor, --generate). Files: new
  `Source/CkJolt/` (Build.cs, Module, Log, Stats, Claude.md, CkJolt_Utils.h/.cpp,
  CkJolt_ContactEvent.h, Settings/CkJolt_ProjectSettings.h/.cpp, Subsystem/CkJolt_Subsystem.h/.cpp);
  CkSpatialQuery: Utils slimmed (re-export + enum Convs + probe shims), subsystem rewritten as
  non-tickable contact-translation bridge, ProbeTrace re-pointed, Build.cs +CkJolt; Watermark
  re-pointed; host ini section renamed `[/Script/CkJolt.Ck_Jolt_ProjectSettings_UE]`; plugin ini
  +ClassRedirect; uplugin +CkJolt; docs fixed (CkThirdParty ×3, CkSpatialQuery, CkWatermark,
  Source/CLAUDE.md tier rows + lookup). Full-suite gate run in flight.

- **Phase 0 GATE: full suite 783/783 PASSED (48m20s)** — strictly better than baseline (782/783;
  the baseline's one red `Ck_AutoTest_Crowd_PathRefresh_InsideBandPlansOut` passed this run →
  classified FLAKY, not deterministic). JobSystem tripwire CONFIRMED in editor log
  (`CkJolt: ... Creating JobSystemSingleThreaded`, ThreadPool 0 hits) — ini section rename verified
  at runtime. Gate-grep caught one real omission: old `CkSpatialQuery_ProjectSettings.h/.cpp` were
  superseded but not deleted → git rm'd, rebuilt green, Probe 16/16 green post-deletion.
  Crowd + Eqs pattern runs in flight; commit after.
- Learned: toolbox `--output` mirror filters sub-Display verbosity — tripwire greps must target
  `Saved/Logs/CkPlugins.log`. PHASE_0.md updated.

### [EDITOR-VERIFY] items (accumulating; for the user when back)

- `ck.SpatialQuery.PreviewAllProbesUsingJolt 1` in PIE still draws probe wireframes (debug-draw gate
  now flows CkSpatialQuery settings → CkJolt subsystem).
- Watermark panel "Jolt" row shows "ST" (single-threaded) in test/PIE config.
