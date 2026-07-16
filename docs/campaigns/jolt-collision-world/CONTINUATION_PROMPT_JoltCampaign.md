# CONTINUATION PROMPT — jolt-collision-world campaign (Phases 3–5 remaining)

You are picking up a 6-phase campaign at the start of Phase 3's main work: **Phases 0–2 are done,
committed, and gated; Phase 3's first two slices are committed (enum migration + physics-ownership
exclusivity); the fixed-timestep step relocation, the JoltBody quartet, Phase 4, and Phase 5 remain.**

Read these IN ORDER before touching anything:
1. `docs/campaigns/jolt-collision-world/PROMPT.md` — mission, locked decisions, non-goals, ruled-out list.
2. `docs/campaigns/jolt-collision-world/PROGRESS.md` — dated ledger of everything done + every deviation.
3. `docs/campaigns/jolt-collision-world/PHASE_3.md`, `PHASE_4.md`, `PHASE_5.md` — the remaining work,
   including a load-bearing **design revision inside PHASE_3.md item 3** (scheduler placement).
4. `Plugins/CkFoundation/Source/CkJolt/Claude.md` — the module you're extending.
5. The original requirements: `D:\Users\neilj\Downloads\jolt-prompt-corrected.md` (reference copy of
   UnrealJolt at `D:\Repos\UnrealJolt\UnrealJolt-master` — study-only).

---

## Repo state (as of 2026-07-16, this session's end)

All work is on `dev` branches, committed locally, **NOT pushed** (house default: push only when asked).

| Repo | Tip | Contents |
|---|---|---|
| CkPlugins (host) | `80460ae` + campaign-doc commits after | Campaign docs, pointer bumps, host ini rename |
| Plugins/CkFoundation | Phase-0 `74c33059e` → Phase-1 `ca08d8b46` → Phase-2 `4b73f9f80` → Phase-3 slices 1+2 (see PROGRESS.md for final hashes) | CkJolt + CkJoltEditor modules, all campaign code |
| Plugins/CkTests | `68ac401` → `f594983` → (Phase-3 pointer as committed) | 3 C++ test files, 7 AS autotests, regenerated wrapper artifacts |

**Scope warning:** the host repo has OTHER sessions' dirty submodule pointers (`CkAuto`,
`Plugins/CkGameplayDebugger`, `Plugins/GitLink`) and untracked root files (`HOMING_INTEGRATION_PLAN.md`,
`.claude/skills/{debug-issue,explore-codebase,refactor-safely,review-changes}.md`). **Never stage them.**
Stage submodule pointer bumps by explicit path only (`git add Plugins/CkFoundation Plugins/CkTests`).

**Regression baseline** (captured before any campaign edit): full suite **783 total / 782 passed /
1 failed** — the one red, `Ck_AutoTest_Crowd_PathRefresh_InsideBandPlansOut`, is FLAKY (passed on
re-runs). Diff all "no regressions" claims against this.

---

## What remains (in execution order)

### Phase 3 (main work) — step relocation + JoltBody quartet
1. **Step relocation** (own gated commit): move `PhysicsSystem::Update` out of
   `UCk_Jolt_Subsystem::Tick` into `Public/CkJolt/World/CkJoltWorld_Processor.h/.cpp`:
   - `FProcessor_JoltWorld_Step`: **model it on `FProcessor_Transform_Cleanup`**
     (`CkEcsExt/Transform/CkTransform_Processor.h:191` — `TProcessorBase` subclass, explicit
     `DoTick(TimeType)`, registry ctor). Fixed-timestep pump: accumulator, `_FixedTimestepHz` (60)
     and `_MaxPhysicsStepsPerFrame` (4) added to `UCk_Jolt_ProjectSettings_UE`, spiral-of-death
     clamp = drop time + Verbose log (NO ensure). Per-step pose capture via the thread-safe
     `PhysicsSystem::GetActiveBodies(EBodyType::RigidBody, ...)` ONLY (never the Unsafe variant)
     into shared `FFragment_JoltBody_StepPose` (UE-space prev/curr) + alpha for interpolation.
   - **Scheduler placement (REVISED — see PHASE_3.md):** processors live INSIDE `FGroup_Transform`
     with `RunAfter = TDepList<FProcessor_Transform_HandleRequests>`. Do NOT re-parent
     `FGroup_Transform_Finalize` or touch `CkProcessorGroups.h`.
   - New `TSharedPtr<ck::FJoltWorld>` registry context (PhysicsSystem + TempAllocator + JobSystem +
     accumulator + alpha) published ALONGSIDE the existing `TWeakPtr<JPH::PhysicsSystem>` context
     (which probes/CkEqs read — do not disturb it).
   - `FProcessor_JoltWorld_DrainEvents`: drain point moves here from subsystem Tick; Phase-0's
     multicast delegate (`Get_OnContactEventsDrained`) evolves into a router registry on the
     FJoltWorld context; CkSpatialQuery's bridge re-registers its probe router in the same commit.
   - `_AsyncPhysicsUpdate`: step kicks the while-loop onto the task graph; a WaitForAsync processor
     at the head of the group blocks next frame.
   - **GATE before any dynamics code:** entire existing probe/crowd/EQS suite green — canaries are
     `Ck_AutoTest_Probe_LinearCastPerf` and `Ck_AutoTest_Crowd_Separation_ProbeIsotropy`.
2. **JoltBody quartet** at `Public/CkJolt/Body/CkJoltBody_{Fragment_Data,Fragment,Processor,Utils}`:
   mirror **CkTimer** exactly (the canonical quartet). Full fragment/param inventory is in
   PHASE_3.md item 5. Key points: `_ShapeSource {ExplicitShape (FCk_Jolt_ShapeDimensions — exists,
   `Query/CkJoltQuery_Data.h`), StaticMeshAsset (reuse `bake::BuildShape_FromBodySetup`)}`;
   trimesh-on-Dynamic = CK_ENSURE fail; Setup batch-adds via `AddBodiesPrepare/Finalize` sorted by
   entity id; `UCk_Utils_JoltBody_UE::Add` must call `ck::physics_ownership::TryClaim_Jolt` (exists,
   `CkEcsExt/PhysicsOwnership/`); EndPlay = release connections → `RemoveBody` if added → ALWAYS
   `DestroyBody` (mirror the probe leak-fix comment at `CkProbe_Processor.cpp:~1180`);
   `FProcessor_JoltBody_WritebackInterpolated` uses
   `UCk_Utils_Transform_UE::Apply_SetTransform_DirectWrite` + `FTag_Transform_Updated`.
3. **Tests** (PHASE_3.md item 6): `Test_JoltBody_Lifecycle.spec.cpp` (churn-1000 → `GetNumBodies`
   baseline), `Test_JoltWorld_FixedTimestep.spec.cpp`, AS: DynamicBoxRestsOnStaticFloor,
   BoxStackOfFiveSettlesAndStays, KinematicPlatformCarriesDynamicBox,
   RestingBodySleepsAndWakeRequestReactivates (tag-level in P3; signal-level in P4). Also
   `Test_JoltBody_OwnershipExclusivity.spec.cpp` (expected-ensure tests MUST be C++ — the AS
   harness escalates warnings; use `AddExpectedError`).

### Phase 4 — requests + events + JoltCharacter + query extensions (PHASE_4.md is complete and current)
### Phase 5 — overlay + parity sampler + Chaos twins + benchmarks → VALIDATION.md (PHASE_5.md current)
   Plus the deferred **gym stations** (none exist yet — registry is
   `Plugins/CkTests/Script/Common/CkTests_GymRegistry.as`, scripts co-located in `Script/CkJolt/`).

---

## Build/test invocation (the toolbox owns everything)

```powershell
Set-Location "D:\Repos\CkPlugins"
./CkAuto/UnrealToolbox.exe --build --config=Development --target=Editor --test --test-pattern Jolt --discover-fresh --output=Saved/Logs/X.log --project="D:\Repos\CkPlugins"
```
- Pre-flight EVERY spawn: probe the editor lock on `Saved/Logs/CkPlugins.log` (see
  `CkAuto/.claude/skills/build-test/SKILL.md`). Run in background, await notification, never poll.
- `--generate` only after Build.cs/uplugin changes. **`--discover-fresh` after adding/removing any
  test** — the toolbox caches its test list and silently reports "No tests matched" otherwise.
- Test-log greps must target `Saved/Logs/CkPlugins.log` (the editor's own log) for sub-Display
  lines — the toolbox `--output` mirror filters them.

## Gotcha branch table (hard-won this session — do not re-derive)

| Symptom | Cause | Fix |
|---|---|---|
| "No tests matched — nothing to run" | Toolbox cached test list | `--discover-fresh` |
| AS error "No matching signatures … (AActor&, …)" on a utils call | AS bindings auto-inject `WorldContext` params | Drop the context argument at AS call sites |
| AS error on `Assert_False` | Doesn't exist in `UCk_AutoTest_Base` | `Assert_True(!x, …)` |
| AS warning "StaticClass is deprecated" | Hazelight dialect | Pass the type directly: `LoadObject(UStaticMesh, "…")`, `SpawnActor(AActor, …)` |
| Runner says "Passed (N assertions)" but controller says Fail | A Warning/Error was LOGGED inside the test window (harness escalation) | Grep the window in CkPlugins.log; fix the log source (this is how the `JPH_ASSERT` layer-bound bug was found) |
| Components spawned via `UX::Create(Actor)` sit at world origin | Plain `AActor` has no root component | `Comp.SetWorldLocation(...)` explicitly |
| Shape-cache count asserts flaky across tests | The cache is PIE-session-lived, tests share it | Use unique scale/mesh per test (cache key = BodySetupGuid+scale+traceflag) |
| Heightfield rays miss at exact far world-Y edge | Jolt heightfield is half-open at its local-origin edge | Expected; seams are covered by neighbor components — probe 1uu inside |
| Runtime-spawned `USplineMeshComponent` has NO collision (Chaos misses too) | Deformed collision never cooks headlessly | `[EDITOR-VERIFY]` only; don't retry (2 attempts logged) |
| `uint64` UPROPERTY UHT error | Not BP-exposable | Plain `UPROPERTY()` without BP flags |
| Non-BlueprintType USTRUCT breaks AS registration of a UCLASS's CK_PROPERTY accessors | AS generator registers accessors whose param types must be AS-known | Mark data structs `USTRUCT(BlueprintType)` |
| `jolt::X` unresolved inside a file-local namespace | using-directive imports names, not the namespace name | `namespace jolt = ck::jolt;` alias |

## Critical files

- `Plugins/CkFoundation/Source/CkJolt/Public/CkJolt/Subsystem/CkJolt_Subsystem.{h,cpp}` — owns the
  world; Tick currently: wait-async → drain+broadcast → OptimizeBroadPhase-if-requested → Update →
  debug draw. The step relocation carves Update/drain OUT of here.
- `.../CkJolt/CollisionLayers/CkJoltCollisionLayerTable.{h,cpp}` — signature table + filters +
  `FCk_Jolt_LayerContext` (registry context; probe layer id).
- `.../CkJolt/StaticWorld/CkJoltBakeExtraction.{h,cpp}` — shared extraction (reuse
  `BuildShape_FromBodySetup` for JoltBody StaticMeshAsset shapes).
- `.../CkJolt/CkJoltShapeFactory.{h,cpp}` — `CreateShape_FromDimensions` (JoltBody ExplicitShape path).
- `.../CkJolt/Query/CkJoltQuery_{Data,Utils}.*` — `FCk_Jolt_ShapeDimensions`, `FCk_Jolt_QueryFilter`,
  `FCk_Jolt_HitResult`, the channel-filtered queries.
- `Plugins/CkFoundation/Source/CkEcsExt/Public/CkEcsExt/PhysicsOwnership/*` — TryClaim_Jolt/Chaos.
- `Plugins/CkFoundation/Source/CkTimer/Public/CkTimer/*` — THE quartet exemplar (naming, request
  drain via `ck::algo::ForEachRequest` + `ck::Visitor` + per-type `DoHandleRequest` overloads,
  `CK_REGISTER_PROCESSOR` in the cpp).
- `Plugins/CkFoundation/Source/CkSpatialQuery/Public/CkSpatialQuery/Probe/CkProbe_Processor.cpp` —
  body-lifecycle patterns to mirror (Setup ~398, EndPlay ~1132 w/ leak-fix comment, the
  `CK_PROBE_FACTORY` registry-context injection at :51).
- `Plugins/CkFoundation/Source/CkEcsExt/Public/CkEcsExt/Transform/CkTransform_Processor.{h,cpp}` —
  `FProcessor_Transform_Cleanup` (DoTick exemplar), `Apply_SetTransform_DirectWrite`,
  `FProcessor_Transform_HandleRequests` (your RunAfter anchor).

## Ruled out / already adjudicated (do not re-litigate)

- Step in `FGroup_Physics` (one-frame kinematic lag) and re-parenting `FGroup_Transform_Finalize`
  (group-graph surgery) → step lives in `FGroup_Transform` w/ RunAfter HandleRequests.
- Naming `RigidBody` → it's `JoltBody`/`JoltCharacter` (backend IS the contract; ownership rule is user-facing).
- Runtime re-extract on stale cooked data → ensure + hard skip.
- Per-channel/per-profile layers → signature-based (committed, tested).
- `JPH_CROSS_PLATFORM_DETERMINISTIC` / `JPH_OBJECT_STREAM` build flags → stay OFF this campaign.
- Probes overlapping the static world → deliberately OFF (fixed probe signature + broadphase guard);
  future opt-in, not a bug.

## Recommended flow for the new session

1. `git log --oneline -6` in CkPlugins + both submodules; read PROGRESS.md bottom-up. Confirm the
   Phase-3 slices 1+2 commits exist (if the tree is instead dirty with them, the interrupted gate is
   yours to finish: run Overlap/RaySense/Crowd patterns, then commit as two commits — enum migration,
   ownership exclusivity — messages sketched in PROGRESS.md).
2. Re-run the Jolt pattern once (`--discover-fresh`) to confirm your local binaries match HEAD
   (stale-green discipline: never trust a green run that predates your checkout).
3. Execute Phase 3 step-relocation as its own commit with the probe/crowd/EQS gate BEFORE any
   JoltBody code. Then the quartet. Then tests+gyms. Commit in-submodule first, host pointer bump after.
4. Phases 4, 5 per their PHASE docs. Populate VALIDATION.md with REAL numbers only.
5. Keep PROGRESS.md append-only current — it is the campaign's memory across sessions.
6. `[EDITOR-VERIFY]` items accumulate in PROGRESS.md for the human — never fake them with automation claims.

## Suggested first message

> I'm continuing the jolt-collision-world campaign. Read
> `docs/campaigns/jolt-collision-world/CONTINUATION_PROMPT_JoltCampaign.md` fully, then PROMPT.md and
> PROGRESS.md in the same folder. Phases 0–2 are committed and gated; verify the Phase-3
> enum-migration + ownership commits landed, then execute the rest of Phase 3 (step relocation gated
> first, then the JoltBody quartet), then Phases 4–5, following the phase docs and committing at each
> gate. Don't push anything.
