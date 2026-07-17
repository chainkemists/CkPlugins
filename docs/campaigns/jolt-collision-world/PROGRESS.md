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

- **JoltBody quartet IMPLEMENTED (opus executor), build green; fresh Fable review in flight.**
  8 new files (Fragment_Data pair, Fragment.cpp, Processor pair, Utils pair, CkJolt_ActivationEvent.h)
  + 7 modified (+262/-22): PlanStep extracted from Step per the pre-ruled split, activation queue on
  the subsystem listener, FJoltWorld activation-drain + Remove_PoseBufferEntry. Executor deviations
  accepted after scrutiny: FCk_Jolt_LayerContext::_Table const→non-const (Setup needs the sanctioned
  game-thread Get_OrRegisterLayer; probe reads only _ProbeLayer), Step keeps its own paused guard
  (behavior-identical split), forward-declared KinematicPush in Step's RunAfter (breaks include cycle;
  scheduler resolves by type name), sibling CkJolt_ActivationEvent.h with forward-declared enum,
  GetProfileTemplate-by-name derivation (~18 lines, under the STOP threshold).
- **[P3-RULING] Initial-Asleep gap** (executor-flagged, correctly not improvised): a body with
  `_InitialSleepState = Asleep` is batch-added DontActivate but got no `FTag_JoltBody_Sleeping`
  (Jolt never fires OnBodyDeactivated for a never-activated body → Get_SleepState would lie
  "Awake" until first activate+sleep). RULED: `Add()` also adds FTag_JoltBody_Sleeping when
  InitialSleepState==Asleep — the tag mirrors intended state from composition. Orchestrator
  applies the fix after the review returns (single rebuild with any review fixes).
- **Fresh-Fable adversarial review of the quartet: 2 BLOCKER + 4 MAJOR + 1 MINOR, ALL CONFIRMED
  and fixed by the orchestrator** (review earned its cost — both blockers trace to a design defect
  in PHASE_3.md item 5 itself):
  - [P3-FIX] **KinematicPush redesign** (both BLOCKERs): the phase doc's FTag_Transform_Updated-gated
    view (a) dropped one-shot moves landing on zero-step frames (tag cleared by Transform_Cleanup
    before the next stepping frame), and (b) left Jolt's PERSISTENT MoveKinematic velocity un-zeroed
    → a once-moved kinematic body sails away forever. Fix: view no longer gated on the tag; every
    added KinematicFromECS body is MoveKinematic'd to its CURRENT ECS transform each stepping frame
    (target==current ⇒ velocity zero — Jolt-idiomatic). PHASE_3.md's design is superseded on this point.
  - [P3-FIX] Get_OrRegisterLayer result guarded against cObjectLayerInvalid (mirrors StaticWorld).
  - [P3-FIX] Probe+JoltBody coexistence made CORRECT (not forbidden): pose apply and SleepStateMirror
    now require the buffer key / event body-id to match the entity's Current._BodyId — another Jolt
    body sharing the entity's UserData (its Probe) can no longer clobber StepPose or the Sleeping tag.
    (Phase-4 note: JoltCharacter's shared-StepPose plan must revisit this body-id check.)
  - [P3-FIX] Mass>0 ensures on Explicit and FromStaticMesh (ClampMin only guards the editor UI;
    zero mass → JPH_ASSERT/NaN) — fall back to shape-calculated mass, loudly.
  - [P3-FIX] EndPlay: WorldTypeRequirement RuntimeOnly added (probe parity) + Release_Jolt moved
    unconditional + invalid-BodyId early-out BEFORE the PhysicsSystem ensure (a Setup-skipped body
    is legal, not an ensure).
  - [P3-FIX] CreateBody failure re-arms FTag_JoltBody_NeedsSetup (transient slot exhaustion retries;
    other ensure-skips stay permanent).
  Review also cleared: chain ordering (incl. forward-declared RunAfter edge), PlanStep/Step split
  behavior-identity, batch add, trimesh walk, const→non-const table thread-soundness, activation
  queue contract, Fragment/Utils doctrine compliance.
- **Orchestrator additions with the fixes**: `ck::jolt::ComputeStepPlan` extracted from PlanStep
  (pure, CKJOLT_API — the fixed-timestep test pins it without a physics world; PlanStep rewired,
  behavior-identical) + the [P3-RULING] Initial-Asleep fix (Add mirrors InitialSleepState==Asleep
  onto FTag_JoltBody_Sleeping). Rebuild in flight: Saved/Logs/P3Quartet-ReviewFixes-Build.log.
- **[P3-FIX] PRODUCT DEFECT found by the test executor (STOP honored — tests were not bent):
  the Jolt world never called SetGravity.** Jolt's default is (0, -9.81, 0) — Y-down in METERS —
  while CkJolt is Z-up passthrough in UE centimeters: dynamic bodies drifted -Y at 9.81uu/s²
  instead of falling -Z (all 4 AS dynamics tests red on it; the box-stack even false-passed its
  spacing asserts because nothing moved). Latent since Phase 0 — probes are gravity-less kinematic
  sensors, so nothing ever fell. PHASE_3.md never specified gravity; genuine design omission.
  Fix (orchestrator): `_PhysicsSystem->SetGravity(Conv({0,0,GetWorld()->GetGravityZ()}))` after
  Init — UE world gravity for Chaos parity, per-world overrides respected (CkJolt_Subsystem.cpp).
  Executor interim results: FixedTimestep 5/5 green (ComputeStepPlan pinned), OwnershipExclusivity
  3/3 green; lifecycle specs blocked only by an environmental BrushComponent ensure from
  /Engine/Maps/Entry's default brush (whitelisted; gravity-independent re-run pending).
- **Phase-3 test suite GREEN on gravity-fixed binaries (orchestrator-verified from logs, not the
  executor's word):** post-gravity Jolt pattern 24/25 (P3Tests-PostGravity.log) with the single red
  — KinematicPlatformCarriesDynamicBox "box 38.1 vs platform 200" — being PHYSICS-CORRECT sliding
  (Jolt-default friction, fast platform), not a product bug; the executor re-tuned the test to
  assert friction-carry under carry-valid conditions (friction 1.0 both surfaces, gentler motion)
  → re-run PASSED 1/1 (P3Tests-Kinematic.log). Net: all 8 world-less C++ tests (ComputeStepPlan 5,
  OwnershipExclusivity 3), 3 lifecycle specs (churn-1000 baseline, batch-500, destroy-while-
  sleeping), 4 AS dynamics tests (RestsOnFloor, BoxStackOfFive, KinematicCarry, SleepsAndWakes) +
  all pre-existing Jolt-pattern tests green. **GATE (b) full-suite regression IN FLIGHT**
  (GateB-FullSuite.log; diff vs 783/782+1-flaky baseline).
- **Test-executor final report received (all claims log-verified): 15/15 authored tests green.**
  7 new files (3 .spec.cpp + 4 .as) + regenerated `Script/Generated/CkTests_AutoTestActors.as`
  + 4 External-Actor uassets auto-staged by the editor's GitSourceControl on map-save (the placed
  wrapper actors — Phase-1 committed equivalents; they ship with the CkTests commit). Notable
  test-design hardening: BoxStack spawn spacing widened to 200uu so the settle assertion can
  never false-pass again if simulation silently stops (it false-passed under the gravity bug at
  105uu). Ownership tests use the hermetic TryClaim primitives (the exact internal call Marker/
  Sensor/RaySense make) instead of a UWorld-dependent Marker — refusal path identical, no PIE.
  FOLLOW-UP (one line, not chased): Phase-1 static-world bake fires a loud ensure on
  /Engine/Maps/Entry's stock default brush (BrushComponent with collision but no BrushBodySetup)
  — bake robustness smell; tests whitelist it as environmental.
- **GATE (b) run 1: 793 total / 792 passed / 1 failed — root-caused to a PHASE-1 TEST BUG, not a
  Phase-3 regression.** The red (`Ck_AutoTest_RaySense_LineTrace_HitFiresSignal`, impact (0,0,300)
  at step=1) was session contamination: the committed Phase-1 test
  `CkAutoTest_CkJolt_StaticBake_SimpleBox_RaycastMatchesChaos.as` spawned its BlockAll engine cube
  at (0,0,300) — exactly the RaySense test's trace origin — and NEVER destroyed it, so the leaked
  Chaos cube blocked the trace at its start. Latent since Phase 1: this was the first full
  shared-session suite run since the Jolt content tests exist (Phases 1-3 gated via per-pattern
  runs, each its own editor session; Phase 0's full run predates the tests). Evidence: mechanism
  explains hit-at-start + signal-fired + step=1; RaySense isolated re-run 4/4 green
  (GateB-RaySense-Isolated.log); no ensures in the failure window; every other campaign test parks
  content at a unique Y offset — only this one squatted on the origin. Fix (test-side, in the
  committed Phase-1 file): relocated to the unique parking spot (0,9000,300) + un-bake
  (Request_RemoveActor) + DestroyActor before FinishSuccess. **GATE (b) run 2 IN FLIGHT**
  (GateB-FullSuite-Run2.log; AS-only fix, binaries unchanged).
- **[P3-RULING] Gyms deferred to Phase 5** — PHASE_3.md item 6 lists BoxStack/KinematicPlatform
  gyms in gate (b), but the (fresher) continuation prompt moved ALL gym stations to Phase 5's
  deferred bucket, consistent with the Phase-1 precedent. Gate (b) = dynamics tests green + full
  regression; gyms accumulate in Phase 5. Executor-flagged Phase-4 items for later ruling:
  probe+JoltBody on one entity shares the UserData id space (SleepStateMirror can't attribute
  activation source — exotic composition, out of v1).

- **GATE (b) PASSED: full suite 793/793 (zero failed, zero skipped)** on run 2 after the
  contamination fix — strictly better than the 783-baseline (782 pass + 1 flaky; the flaky Crowd
  red passed here too). Log: GateB-FullSuite-Run2.log.
- **PHASE 3 COMPLETE + COMMITTED.** CkFoundation `b23f1bddd` (step relocation, 14 files) +
  `5f5128c48` (quartet + review fixes + gravity fix, 15 files, +1832); CkTests `4a2c4df`
  (13 files, +1183: 3 spec.cpp + 4 AS + wrapper artifacts + 4 placed wrapper actors + the
  Phase-1 SimpleBox leak fix). PHASE_3.md → DONE with design supersessions noted.
  Session log: 2026-07-16/17, Fable orchestrator; routing — opus executors for step relocation,
  quartet, and tests (each from a written dispatch spec); fresh-Fable adversarial reviewer for
  the quartet (found 2 blockers + 4 majors, all fixed); explore agent for exemplar extraction;
  judgment work (design, rulings, review-fix implementation, gates, commits) inline at Fable tier.
- Next: **Phase 4** per PHASE_4.md (requests + events/signals + JoltCharacter + query extensions).

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
