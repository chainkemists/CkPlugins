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

- **SESSION RESUMED (2026-07-16, new orchestrator session, Fable-tier).** Resume ritual done:
  all three repos confirmed on `feature/jolt-collision-world`; tips match the handoff table
  (CkFoundation `abf1b6cbe`, CkTests `f594983`, host `2a0bfc7` = handoff-doc commits atop `28b9a3`);
  spot-checks: 5 campaign commits present in CkFoundation log, PhysicsOwnership/enum-migration
  commits verified by SHA. Phase docs 3/4/5 + CkJolt Claude.md re-read.
- **BLOCKER — foreign WIP breaks the build.** Two dirty files in CkFoundation, NOT campaign work,
  last written 2026-07-15 ~21:00 (before this session): `CkCrowd/.../CkCrowdAgent_Fragment_Data.h`
  (derives FCk_Handle_CrowdAgent from FCk_Handle_Transform via CK_GENERATED_BODY_HANDLE_DERIVED)
  + `CkEcs/.../CkHandle_TypeSafe.h` (mixin-ctor StaticCast tweak). Pre-flight build FAILED on them:
  C2248 private-ctor access at CkCrowdAgent_Fragment_Data.h(42) — the WIP itself doesn't compile.
  Attempted a named `git stash push` of exactly those 2 files (restore = `git stash pop`);
  DENIED by the session's permission classifier (foreign-work guard). **RESOLVED: user approved
  the stash** — now `stash@{0}` in Plugins/CkFoundation ("SIBLING-SESSION WIP (CrowdAgent handle
  derivation...)"); restore with `git stash pop`. Tree clean at `abf1b6cbe`. Nothing else touched;
  host-repo foreign items (CkGameplayDebugger pointer, untracked Content/Maps/) left alone as before.
- **Step-relocation design locked this session** (extends PHASE_3.md item 4; recorded so no
  re-derivation): (1) `ck::FJoltWorld` (new `World/CkJoltWorld.h/.cpp`) owns accumulator/alpha/
  pending-step-count/optimize-flag/contact-router-registry/async-future + a **pose buffer**
  (TMap keyed by body index, plain data: UserData + prev/curr UE-space loc+rot + dirty flag) —
  the fixed-step while-loop (which may run on the task graph in async mode) captures
  `GetActiveBodies` poses into that buffer ONLY; ECS fragments are written by a game-thread apply
  (sync mode: end of Step DoTick; async mode: next frame's WaitForAsync). Rationale: pose capture
  inside the async task must never touch the registry — the phase doc's capture-into-fragment
  wording is unsafe under `_AsyncPhysicsUpdate`. Prev/curr continuity lives in the buffer across
  frames; zero-step frames leave prev/curr and let alpha keep growing (standard interpolation).
  (2) Processor order inside FGroup_Transform: WaitForAsync (RunAfter Transform_HandleRequests)
  → DrainEvents (RunAfter WaitForAsync) → Step (RunAfter DrainEvents) — preserves subsystem Tick
  order wait→drain→optimize→step; quartet's KinematicPush later slots before Step, Writeback after.
  (3) `CkJolt.Build.cs +CkEcsExt` moves UP into the step-relocation commit (RunAfter names
  FProcessor_Transform_HandleRequests, a CkEcsExt type) — PHASE_3.md item 5 placed it in the
  quartet commit; deviation noted. (4) Step math on game thread: NumSteps computed+clamped, then
  accumulator/alpha updated BEFORE kicking the async task; the task runs only Update()-loop +
  buffer capture. (5) Minimal `Body/CkJoltBody_Fragment.h` (FFragment_JoltBody_StepPose +
  FTag_JoltBody_TransformDirty) ships in the step commit; quartet extends the file.
  (6) `Get_OnContactEventsDrained` deleted in the same commit (no back-compat shims);
  subsystem gains Register/UnregisterContactRouter forwarding into FJoltWorld's router registry;
  CkSpatialQuery bridge re-registers `ProcessQueuedContacts` there. Debug draw stays in subsystem
  Tick (read-only; one-frame-late wireframe acceptable, documented).

- **JoltBody quartet design ruled this session** (extends PHASE_3.md item 5; do not re-derive):
  (1) **LayerSource v1 = collision-profile name only** (`FName _CollisionProfileName`, default
  `PhysicsActor`); Setup derives the signature from the profile (mirroring
  `Build_FromCollisionProfiles`) with **Domain = Static iff MotionType==Static, else Dynamic**,
  resolved via `FCk_Jolt_CollisionLayerTable::Get_OrRegisterLayer` (game-thread contract holds —
  Setup is a game-thread processor). Custom per-body response masks deferred (signature machinery
  already supports them; reflected-params surface not worth it in v1).
  (2) **Plan/execute split in the quartet commit**: extract `FProcessor_JoltWorld_PlanStep`
  (accumulator += dt, clamp/drop, NumSteps/Alpha/PendingSimTime onto FJoltWorld) out of Step so
  KinematicPush reads PendingSimTime instead of duplicating the math. Final chain in
  FGroup_Transform: WaitForAsync → DrainEvents → PlanStep → SleepStateMirror → KinematicPush →
  Step (execute-only) → WritebackInterpolated. Step-relocation commit keeps plan+execute fused
  (spec already dispatched); the split is a small mechanical refactor rolled into the quartet.
  (3) The 8 tags: NeedsSetup, MotionType_Static/_Kinematic/_Dynamic, KinematicFromECS,
  TransformDirty (ships in step commit), Sleeping, PersistContacts.
  (4) **P3 minimal request set**: `FCk_Request_JoltBody_SetSleepState` (`ECk_Jolt_SleepState
  {Awake, Asleep}`) + HandleRequests processor (RunAfter Setup, drains before KinematicPush/Step)
  — required by the RestingBodySleepsAndWakeRequestReactivates autotest; Phase 4 extends the variant.
  (5) **Add() composes ON the target entity** (Probe pattern, not CkTimer's child-entity ritual —
  a body IS the entity; matches ownership-exclusivity semantics); TryClaim_Jolt gates composition
  before any fragment lands; Add CK_ENSUREs the entity already has the Transform feature.
  (6) Writeback: TParallelProcessor, excludes KinematicFromECS, Lerp/Slerp StepPose by FJoltWorld
  alpha, Apply_SetTransform_DirectWrite + DeferAddOrGet<FTag_Transform_Updated>, clears
  FTag_JoltBody_TransformDirty registry-wide post-iteration (Transform-cleanup pattern).
  (7) Activation events: extend CkBodyActivationListener to queue {BodyID, UserData, Activated}
  under lock (mirror contact queue); FJoltWorld gets a drain fn; SleepStateMirror toggles
  FTag_JoltBody_Sleeping. Tag-level only in P3 (signals are P4).
  (8) Mass {FromShape default, FromStaticMesh via BodySetup CalculateMass, Explicit MassKg};
  COM {FromShape, ExplicitOffset + FVector}; Surface {PhysicalMaterial ref, Explicit
  friction/restitution}; GravityFactor + Linear/AngularDamping floats.
  (9) Trimesh-on-Dynamic = CK_ENSURE fail at Setup (walk decorated shapes to the leaf subtype).
  (10) Quartet processors read the FJoltWorld/PhysicsSystem context per DoTick (uniform with the
  world processors) instead of the probe's ctor-factory injection; absent context → silent return
  (legal non-Jolt worlds).

- **Step relocation IMPLEMENTED (opus executor) + orchestrator audit PASSED.** New:
  `World/CkJoltWorld.h/.cpp` (FJoltWorld step engine: fixed-timestep pump state, pose buffer,
  contact-router registry, async future), `World/CkJoltWorld_Processor.h/.cpp` (WaitForAsync →
  DrainEvents → Step chained in FGroup_Transform after Transform_HandleRequests),
  `Body/CkJoltBody_Fragment.h` (StepPose + TransformDirty). Modified: subsystem (Tick = debug draw
  only; owns/publishes TSharedPtr<FJoltWorld> context; Register/UnregisterContactRouter replaces
  the multicast), settings (+_FixedTimestepHz 60, +_MaxPhysicsStepsPerFrame 4), SpatialQuery bridge
  (router registration, weak-capture), Build.cs +CkEcsExt, module Claude.md. Executor deviations:
  none material (documented in its report; SetContext = entt try_emplace, no overwrite → shutdown
  nulls FJoltWorld's pointers instead of clearing the context — safe, commented in Deinitialize).
  Orchestrator audit fixes on top: RegisterContactRouter's silent-return upgraded to
  CK_ENSURE_IF_NOT (no legal null path — silent handling violated non-negotiable #3); deleted the
  orphaned FCk_Jolt_OnContactEventsDrained delegate + fixed its doc comment (CkJolt_ContactEvent.h);
  fixed stale module-doc anti-pattern line (processors own the step now).
  **Known limitation (pre-existing, carried over, async mode only):** with _AsyncPhysicsUpdate on,
  FGroup_Overlap probe processors touch BodyInterface later in the same frame the async step batch
  may still be running — this race predates the campaign (old subsystem async Tick had it too);
  pose capture inside the async batch inherits it. Async defaults OFF; revisit if async ships.
- **GATE (a) PASSED + STEP RELOCATION COMMITTED.** Rebuild on the final artifact (audit edits
  included), then sequential pattern runs — **Jolt 10/10, Probe 17/17, Crowd 16/16, Eqs 10/10,
  Overlap 73/73, RaySense 4/4 — delta-zero**; both canaries (LinearCastPerf,
  Crowd_Separation_ProbeIsotropy) green. Logs: Saved/Logs/GateA-*.log. CkFoundation commit
  `b23f1bddd` (14 files, +874/-102). PHASE_3.md items 3+4 DONE. Next: JoltBody quartet
  (dispatch spec ready; includes the pre-ruled PlanStep extraction).

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
