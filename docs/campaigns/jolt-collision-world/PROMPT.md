# Campaign: Jolt Collision World (CkJolt)

**Status:** ACTIVE — started 2026-07-16
**Requirements source:** `D:\Users\neilj\Downloads\jolt-prompt-corrected.md` (user-authored, decisions delegated)
**Reference implementation:** `D:\Repos\UnrealJolt\UnrealJolt-master` (study-only; do not copy its Y-up world flip, PIE-once cook, or UE-thread-pool stub)

## Mission

Extract CkFoundation's Jolt integration out of CkSpatialQuery into a new module **CkJolt** that owns the
Jolt world, then extend it into a full collision world with Chaos parity: static world baking AND
dynamic/kinematic body simulation. CkSpatialQuery's Probe becomes a consumer of CkJolt.

## Success criteria (observations, not activities)

1. Every pre-campaign CkSpatialQuery/Probe AutoTest passes unchanged after the split; CkCrowd, CkEqs,
   CkProjectile, CkWatermark build and test clean against the new dependency shape.
2. A test level's static geometry (simple/complex static meshes, HISM, landscape, splines, brushes,
   Nanite fallback) baked into the CkJolt world answers N seeded raycasts/sweeps/overlaps identically
   to Chaos (per-primitive-type mismatch count == 0 within tolerance).
3. Dynamic scenarios (box stack, ramp roll, CCD projectile, kinematic platform carry) are qualitatively
   equivalent and stable in both engines (paired AutoTests green).
4. `ck.Jolt.DebugDraw.Enabled 1` draws Jolt wireframe (static + dynamic, sleep-state coloring) over
   Chaos collision in PIE.
5. VALIDATION.md holds benchmark tables: sim step + transform sync at N=500/2k/10k, Chaos-via-actors
   vs CkJolt-via-ECS, plus the contact-island worst-case verdict.
6. Cooked Jolt data (per streaming cell, versioned + source-hashed) loads in lockstep with level
   streaming; stale data ensures loudly and is never silently used.

## Locked decisions

- **Module**: CkJolt under `Plugins/CkFoundation/Source/CkJolt/`; deps grow per phase
  (P0: Core,Ecs,Log,Settings,ThirdParty → P1: +Landscape(editor-cond),PhysicsCore → P3: +EcsExt).
  Tier-table row in the T4 band next to CkPhysics. Editor twin `CkJoltEditor` from Phase 1.
- **Feature naming**: `JoltBody` / `JoltCharacter` (the backend IS the contract — the Chaos-XOR-Jolt
  ownership rule is user-facing). NOT `RigidBody`.
- **Coordinates**: Ck stays Z-up world; Y→Z axis correction on capsule/cylinder/heightfield shapes
  (existing `Get_ShapeAxisCorrection_YToZ()` convention). Never UnrealJolt's world Y-up flip.
- **Bake source**: BodySetup cooked data ONLY (AggGeom simple / `TriMeshGeometries[0]` complex per
  CollisionTraceFlag; Nanite = same path). No collision → CK_ENSURE + skip. Never render geometry,
  never a bounding-box fallback.
- **Layer mapping**: one Jolt ObjectLayer per unique collision *signature*
  {ObjectChannel, ResponseMask, CollisionEnabled, Static|Dynamic domain}, table built from
  `UCollisionProfile` at startup. Block-vs-Overlap resolved at query sites (channel-carrying filters).
  Broadphase: Static=0, Dynamic=1.
- **Cooked data**: UAsset-per-cell + per-map index under `/Game/CkJoltData`;
  `Shape::SaveWithChildren` w/ shared ShapeToIDMap (dedup); `_CookVersion` + `_JoltVersionId` +
  per-actor `_SourceHash`/`_RuntimeCheckHash`. PIE default = live extraction; packaged = cooked only,
  mismatch = ensure + hard skip (no runtime re-extract).
- **Streaming**: `FWorldDelegates::LevelAddedToWorld/RemovedFromWorld` (covers WP + legacy), records
  keyed by level-relative actor FName, batch `AddBodiesPrepare/Finalize`.
- **Sim step**: fixed timestep (60Hz default, max 4 steps/frame) with render interpolation; step
  relocates (Phase 3) from subsystem Tick into `FProcessor_JoltWorld_Step` in NEW scheduler group
  `FGroup_Physics_Jolt` (after `FGroup_Transform`, before `FGroup_Transform_Finalize`) so kinematic
  pushes see same-frame transforms and writeback precedes SyncToActor. Jolt→ECS sync iterates
  `PhysicsSystem::GetActiveBodies` only.
- **JobSystem**: Jolt's own thread pool (already the case) — NOT UE task graph. `_AsyncPhysicsUpdate`
  preserved (step overlaps the rest of the frame; next frame waits).
- **Ownership rule**: counted tags `FTag_PhysicsOwnership_Chaos`/`_Jolt` in CkEcsExt +
  `TryClaim_*` (CK_ENSURE + invalid-handle return) called from every relevant `Add()` —
  Marker/Sensor/RaySense claim Chaos; Probe/JoltBody/JoltCharacter claim Jolt.
- **Events**: worker-thread Jolt callbacks queue under lock; ONE game-thread drain point routes to
  per-entity signals (`CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE`). Never invoke delegates from Jolt threads.
- **CCD**: JoltBody LinearCast = native `JPH::EMotionQuality::LinearCast` (Probe's manual-CastShape
  LinearCast is a distinct sensor workaround — both documented).

## Non-goals

- No Chaos replacement engine-wide (CharacterMovement, ragdolls, cloth, Niagara collision, nav/EQS/
  perception scene queries stay Chaos).
- No constraint/joint system (don't block adding it later).
- No `JPH_CROSS_PLATFORM_DETERMINISTIC` flag flip this campaign (documented as future rollback enabler).
- No silent fallbacks anywhere.

## Ruled out (do not re-litigate)

- Per-channel or per-profile ObjectLayers (lose per-component custom responses) → signature-based.
- Loose-file JoltData dir (packaging/streaming friction) → UAsset-per-cell.
- Runtime re-extract on stale cooked data (hides broken cooks) → ensure + skip.
- UnrealJolt's LandscapeSpline reflection hack (LocalMeshComponents are registered components) → normal sweep.
- Keeping the step in `FGroup_Physics` (runs before Transform_SyncFrom → one-frame kinematic lag) →
  new `FGroup_Physics_Jolt`.

## Reading list

- `Plugins/CkFoundation/CLAUDE.md` + `Source/CLAUDE.md` (doctrine, tier table, module-authoring rules)
- `Plugins/CkFoundation/Source/CkTimer/` (canonical quartet exemplar)
- `Plugins/CkFoundation/Source/CkSpatialQuery/` (extraction source; Probe body lifecycle incl. the
  EndPlay DestroyBody leak-fix comment)
- `Plugins/CkTests/CLAUDE.md` (four test pipelines) + `ck-tests-authoring-and-running` skill
- Phase docs: `PHASE_0.md` … `PHASE_5.md` beside this file; volatile state in `PROGRESS.md`
