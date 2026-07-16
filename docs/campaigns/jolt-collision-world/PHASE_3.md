# PHASE 3 — Step relocation + JoltBody dynamics + ECS sync

**Status:** PENDING (blocked by Phase 2 gate)
**Gates:** TWO — (a) step-relocation commit: entire existing probe/crowd/EQS suite green (canaries:
LinearCastPerf, Crowd_Separation_ProbeIsotropy); (b) phase end: dynamics tests green + regression.

## Work items (ordered commits)

1. **Enum migration**: `ECk_MotionType`, `ECk_MotionQuality` (+`ECk_BackFaceMode` if convenient) →
   `Public/CkJolt/CkJolt_Common.h`; `+EnumRedirects` in `Plugins/CkFoundation/Config/
   DefaultCkFoundation.ini` (precedent: ECk_Net_NetRoleType redirect ~line 10). Exactly 3 .uassets
   reference ECk_MotionType (EntitySpawnParams_Interactable_Probe_Entt, Utils_CkSpatialQuery_FL,
   Interactable_Probe_Entt) — `[EDITOR-VERIFY]` resave. Probe keeps compiling via the re-export header.
2. **Ownership vocabulary**: `Source/CkEcsExt/Public/CkEcsExt/PhysicsOwnership/
   CkPhysicsOwnership_Fragment.h` — `CK_DEFINE_ECS_TAG_COUNTED(FTag_PhysicsOwnership_Chaos)`,
   `..._Jolt`; `CkPhysicsOwnership_Utils.h/.cpp` — `TryClaim_Jolt/TryClaim_Chaos` (CK_ENSURE_IF_NOT
   opposing-tag-absent → false; AddOrGet own tag). Retrofit: Marker/Sensor (CkOverlapBody), RaySense
   Adds claim Chaos; Probe Add claims Jolt. Test: Test_JoltBody_OwnershipExclusivity.spec.cpp
   (AddExpectedError; expected-ensure tests live in C++ — AS harness escalates warnings).
3. **Scheduler group**: `CkProcessorGroups.h` — add `FGroup_Physics_Jolt { RunAfter FGroup_Transform }`;
   re-parent `FGroup_Transform_Finalize::RunAfter` onto it; pipeline comment update.
   FIRST verify `FProcessor_Transform_Cleanup`'s actual execution point (its
   `RunAfter TDepList<ck::FGroup_Physics>` at CkTransform_Processor.h:194 must not clear
   `FTag_Transform_Updated` before our writeback's consumers run).
4. **Step relocation (gated commit)**: `Public/CkJolt/World/CkJoltWorld_Processor.h/.cpp` —
   `FProcessor_JoltWorld_WaitForAsync` (async mode only), `FProcessor_JoltWorld_Step` (fixed-timestep
   pump: Accumulator += dt, clamp at MaxSteps*FixedDt (drop time, Verbose log + stat, NO ensure),
   while >= FixedDt { characters ExtendedUpdate → PhysicsSystem->Update(FixedDt, CollisionSteps,
   TempAllocator, JobSystem) → pose capture via GetActiveBodies (thread-safe variant ONLY) →
   StepPose prev/curr + FTag_JoltBody_TransformDirty }, Alpha = Accumulator/FixedDt on FJoltWorld ctx),
   `FProcessor_JoltWorld_DrainEvents` (router registry; probe router re-registered by CkSpatialQuery
   here — Phase 0's subsystem multicast delegate retires in this commit).
   New `TSharedPtr<ck::FJoltWorld>` registry context published ALONGSIDE the untouched
   `TWeakPtr<JPH::PhysicsSystem>`. Subsystem Tick reduced to bookkeeping.
   Settings: `_FixedTimestepHz` (60), `_MaxPhysicsStepsPerFrame` (4).
   `_AsyncPhysicsUpdate`: step 7 kicks the while-loop onto the task graph; next frame's WaitForAsync
   blocks. **GATE (a) here.**
5. **JoltBody quartet**: `Public/CkJolt/Body/CkJoltBody_Fragment_Data.h/.cpp` (ParamsData per design:
   ShapeSource {ExplicitShape, StaticMeshAsset} + FCk_Jolt_ShapeDimensions + StaticMesh ref +
   MotionType/Quality + InitialSleepState + MassSource {FromShape, FromStaticMesh, Explicit} + COM
   source/offset + SurfaceSource {PhysicalMaterial, Explicit} + GravityFactor/Dampings + LayerSource +
   PersistContacts; FCk_Handle_JoltBody), `CkJoltBody_Fragment.h/.cpp` (Current {BodyID, Shape ref},
   StepPose {UE-space prev/curr loc+rot}, 8 tags, Requests variant, signals), `CkJoltBody_Processor.h/.cpp`
   (Setup — shape from source via shared factory, trimesh-on-Dynamic = CK_ENSURE fail, BodyCreationSettings
   w/ SetUserData(entity), per-tick collect + ONE AddBodiesPrepare/Finalize batch sorted by entity id;
   KinematicPush — view Current+FFragment_Transform+KinematicFromECS+FTag_Transform_Updated,
   MoveKinematic(target, PendingSimTime), skip when zero steps expected; WritebackInterpolated —
   TParallelProcessor over TransformDirty, lerp/slerp by alpha → Apply_SetTransform_DirectWrite +
   defer FTag_Transform_Updated; EndPlay — release connections, RemoveBody if added, ALWAYS DestroyBody),
   `CkJoltBody_Utils.h/.cpp` (Add w/ TryClaim_Jolt, Has/DoCast/Get_*). Activation-listener queue +
   Sleeping tag mirror (signal in Phase 4). Build.cs: +CkEcsExt.
6. **Tests + gyms**: Test_JoltBody_Lifecycle.spec.cpp (churn 1000 → GetNumBodies baseline; batch 500;
   destroy-while-sleeping), Test_JoltWorld_FixedTimestep.spec.cpp (synthetic dts → step counts, clamp,
   alpha ∈ [0,1)); AS DynamicBoxRestsOnStaticFloor, BoxStackOfFiveSettlesAndStays,
   KinematicPlatformCarriesDynamicBox, RestingBodySleepsAndWakeRequestReactivates (tag-level);
   gyms BoxStack + KinematicPlatform. **GATE (b).**

## Documented accepted costs

- SceneNode children of Jolt-driven bodies propagate one frame late (SceneNode runs in FGroup_Transform).
- Sleep deactivation snaps the final pose (near-zero velocity, imperceptible).
- Zero-step frames skip kinematic pushes (velocity persists to next stepping frame).
- Rendering lags sim ≤ one fixed step (standard fix-your-timestep interpolation).

## Risks

1. Step relocation changes probe/crowd observable cadence → isolated gated commit; fallback documented
   (per-frame dt for sensor-only worlds) but escalated, not silently taken.
2. Transform_Cleanup tag-clear ordering → verify before writeback lands.
3. Probe LinearCast vs JoltBody LinearCast confusion → comparison comments both files.
