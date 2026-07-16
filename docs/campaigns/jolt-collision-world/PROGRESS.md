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

- **PHASE 0 COMPLETE + COMMITTED.** CkFoundation dev `74c33059e` (29 files; settings files detected
  as renames). Host dev: `32a28d6` (campaign docs) + `5806b80` (pointer bump + host ini rename,
  coupled deliberately). Crowd 16/16 + Eqs 10/10 confirmed post-deletion. PHASE_0.md → DONE.
- Phase 1 (static world baking) started.
- Phase 1 authored (build iterating): extraction library (`CkJoltBakeExtraction`), collision
  signature + cooked data types, `UCk_JoltStaticWorld_Subsystem_UE` (level-streaming lockstep,
  live/cooked, batch add, OptimizeBroadPhase policy), `UCk_Utils_JoltStaticWorld_UE` (AS surface),
  `CkJoltEditor` module (WorldCooker + EditorSubsystem + Commandlet + Tools menu), settings
  additions, `Static_World` object layer (pairs with nothing — keeps probes untouched pre-Phase-2),
  ref-counted `Request_GlobalJoltInit/Shutdown` helpers, extraction policy
  (LevelSweep skips Movable; ExplicitActor bakes it — runtime-spawn + test path).
- **Phase-1 test-scoping decisions** (autonomous):
  - Tests use RUNTIME-SPAWNABLE content only (engine cube, runtime HISM/spline components) —
    .umap/.uasset authoring isn't possible headlessly. 5 AS autotests + 2 C++ tests
    (heightfield known-heights axis pin, shape-blob roundtrip).
  - Landscape end-to-end, brush volumes, real sublevel streaming, cooked-data load path, and the
    cook commandlet → `[EDITOR-VERIFY]` items (code paths ship; heightfield math is C++-pinned;
    add/remove lifecycle proxied by the RemoveActor AS test).
  - Complex-trimesh (CTF_UseComplexAsSimple) extraction ships; automated coverage deferred
    (needs authored asset or transient-mesh cook harness) — coverage gap logged.
  - Gym station deferred until after Phase-1 tests are green.

- **PHASE 1 COMPLETE + COMMITTED.** CkFoundation `ca08d8b46` (30 files, +3744), CkTests `68ac401`
  (12 files). Gate: Jolt 6/6 (2 C++ pins + 4 AS), Probe 16/16, Crowd 16/16. Debug loop findings:
  toolbox caches its test list (`--discover-fresh` needed after adding tests); AS binding
  auto-injects WorldContext params (drop them at AS call sites); `Assert_False` doesn't exist in
  the AutoTest base; plain-AActor spawns have no root so ::Create'd components need explicit
  SetWorldLocation; the shape cache is PIE-session-lived so cross-test asserts need unique cache
  keys; Jolt heightfields are half-open at the local-origin edge (seams covered by neighbor
  components in real landscapes).
- Phase 2 (layer mapping + scene queries) started.
- Phase 2 authored (build iterating): `FCk_Jolt_CollisionLayerTable` (one ObjectLayer per unique
  signature, seeded from UCollisionProfile, fixed-capacity lock-free reads, atomic count publish),
  3 table-driven filters replace the hardcoded world, probe layer = fixed signature (WorldDynamic,
  Overlap→WorldDynamic only) + broadphase guard (probe never tests the Static tree), layer context
  published to the registry (`FCk_Jolt_LayerContext`), probe setup reads it (was `ObjectLayer{1}`),
  static bodies resolve captured signatures at load (indices never serialized),
  `CkJoltShapeFactory` (shared with Phase-3 bodies), `UCk_Utils_JoltQuery_UE`
  (Get_RayCast/ShapeCast/Overlap w/ channel+min-response filters, entity + actor-name hit
  attribution). Tests: C++ table-matrix pin + 3 AS (Block/Overlap/Ignore semantics, sweep parity
  vs Chaos, probe-ignores-static-world). Decision: AS profile-matrix test skipped — the C++ test
  covers the matrix without expanding the BP surface for test-only introspection.

- **PHASE 2 COMPLETE + COMMITTED.** CkFoundation `4b73f9f80` (12 files, +1016), CkTests `f594983`.
  Gate: Jolt 10/10 (all Phase-2 tests green on FIRST run), Probe 17/17, Crowd 16/16, Eqs 10/10.
- Phase 3 (step relocation + JoltBody dynamics) started.
- **Phase 3 slices 1+2 COMMITTED**: enum migration `9f250bc59` (ECk_MotionType/Quality/BackFaceMode →
  CkJolt_Common.h + Conv moves + 3 EnumRedirects), physics ownership `abf1b6cbe`
  (CkEcsExt/PhysicsOwnership counted tags + TryClaim_* retrofitted into Probe/Sensor/Marker/RaySense
  Adds). Gate: Jolt 10/10, Probe 17/17, Overlap 73/73 (no existing content violates the rule),
  RaySense 4/4, Crowd 16/16. (Gate was interrupted by a session limit mid-run and finished on resume.)
- **SESSION HANDOFF POINT (2026-07-16)**: 5-hour session limit reached mid-Phase-3. Remaining work
  (Phase 3 step relocation + JoltBody quartet, Phases 4-5) handed off via
  `CONTINUATION_PROMPT_JoltCampaign.md` in this folder. PHASE_3.md carries the load-bearing
  scheduler-placement revision (step processors in FGroup_Transform w/ RunAfter
  Transform_HandleRequests — NOT the originally-designed group re-parenting).

### [EDITOR-VERIFY] items (accumulating; for the user when back)

- `ck.SpatialQuery.PreviewAllProbesUsingJolt 1` in PIE still draws probe wireframes (debug-draw gate
  now flows CkSpatialQuery settings → CkJolt subsystem).
- Watermark panel "Jolt" row shows "ST" (single-threaded) in test/PIE config.
- **Phase 1**: on a map with a landscape — Tools → "Cook Jolt Static World (Current Map)" produces
  `/Game/CkJoltData/...` assets; then set project setting `PIE Static World Mode = Cooked` and PIE:
  `ck.SpatialQuery.PreviewAllProbesUsingJolt 1` shows the landscape/static wireframes; stale-data
  test: move a static mesh, PIE again → loud ensure naming the actor, its bodies skipped.
- **Phase 1**: a map with a BlockingVolume — bake (live PIE) and verify probe raycast vs volume.
- **Phase 1**: SPLINE MESH parity — an editor-authored spline mesh (deformed collision only cooks
  with editor machinery; runtime-spawned spline meshes never created collision headlessly — Chaos
  itself had nothing to hit, 2 attempts). Verify: bake, then compare a Chaos trace vs
  `Get_RayCastStaticWorld` along the deformed span (≤2uu). The AS test was removed — the
  extraction path (per-instance BodySetup dispatch) is code-shipped but its deformed-content
  correctness is editor-verified only. COVERAGE GAP logged.
- **Phase 1**: real sublevel/World Partition streaming — bodies appear/disappear with cell loads
  (`ck.Jolt` Verbose logging shows per-level add/remove counts).
