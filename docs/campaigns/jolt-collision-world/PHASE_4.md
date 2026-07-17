# PHASE 4 — Requests + events/signals + JoltCharacter + query extensions

**Status:** DONE — CkFoundation `e2687e2e1` / CkTests `fb6a727` (2026-07-17).
**Gate:** phase tests green + full regression — PASSED: CkJolt pattern 34/34 after the
PushPolicy test-defect fix (P4Tests-Run2.log + P4Tests-PushPolicy-Fix1.log), full suite
**802/802** vs the 793/793 Phase-4 entry baseline (+9 new rows, delta-zero elsewhere;
GateP4-FullSuite.log).
Design supersessions vs the items below (rulings in PROGRESS.md): MoveKinematic request
dropped (KinematicPush drives kinematic bodies from the ECS transform each stepping frame);
PushPolicy maps to CharacterContactSettings {mCanPushCharacter, mCanReceiveImpulses} in
OnContactAdded (not an OnContactValidate filter); characters step in
DoStepCharacters_AnyThread before each PhysicsSystem::Update substep; gyms deferred to
Phase 5.

## Work items

1. **Request family** (CkJoltBody_Fragment_Data.h; all `: FCk_Request_Base`,
   CK_REQUEST_DEFINE_DEBUG_NAME, essentials-in-ctor + fluent Set_*):
   AddForce, AddForceAtLocation, AddTorque, AddImpulse, AddImpulseAtLocation, AddAngularImpulse,
   SetLinearVelocity, SetAngularVelocity, MoveKinematic (target loc+rot, velocity-based),
   Teleport (loc+rot, `ECk_Jolt_TeleportVelocityPolicy {KeepVelocity, ResetVelocity}` default Reset;
   handler direct-writes ECS transform + snaps StepPose prev=curr), SetSleepState
   (`ECk_Jolt_SleepState {Awake, Asleep}` → ActivateBody/DeactivateBody).
   `FProcessor_JoltBody_HandleRequests` (RunAfter Setup, MarkedDirtyBy Requests, drains BEFORE step;
   ck::Visitor + per-type DoHandleRequest). Utils `Request_*` UFUNCTIONs.
2. **Events → signals**: extend FCk_Jolt_ContactEvent (+Body1Id/Body2Id, IsSensor1/2,
   PenetrationDepth, RelativeNormalVelocity — computed in callback from body velocities; true solver
   impulses = documented v2 via EstimateCollisionResponse). Router registry on FJoltWorld ctx
   (`RegisterContactRouter`). JoltBody router: liveness-guarded UserData resolve → broadcast
   `OnJoltBodyContactAdded` / `OnJoltBodyContactPersisted` (gated by FTag_JoltBody_PersistContacts) /
   `OnJoltBodyContactRemoved` — payload {OtherEntity, ContactPoints, ContactNormal,
   RelativeNormalSpeed, OtherIsSensor}. Activation drain broadcasts
   `OnJoltBodySleepStateChanged(handle, ECk_Jolt_SleepState)` + toggles Sleeping tag + snaps pose.
   No `_IsSensor` param on JoltBody v1 (sensors are Probe's domain).
3. **JoltCharacter quartet** (`Public/CkJolt/Character/`): CharacterVirtual-based; Params
   {CapsuleRadius, CapsuleHalfHeight (essentials), MassKg, MaxSlopeAngleDegrees,
   `ECk_JoltCharacter_PushPolicy {PushAndBePushed, PushOnly, BePushedOnly, Neither}` (mass +
   CharacterContactListener::OnContactValidate filter), LayerSource}; Current {Ref<CharacterVirtual>,
   `ECk_JoltCharacter_GroundState {OnGround, OnSteepSlope, InAir, NotSupported}` mirror, pending
   move/jump intent}; ExtendedUpdate per fixed step inside FProcessor_JoltWorld_Step BEFORE
   PhysicsSystem::Update; pose into shared StepPose → shared writeback; EndPlay destroys.
   Requests: Move (desired velocity), Jump (velocity), Teleport (loc + optional rot).
   Utils: Add (TryClaim_Jolt), Get_GroundState/Normal/Velocity, BindTo_OnGroundStateChanged.
   Signal detected after last substep, queued, broadcast in DrainEvents.
   v1 non-goals: crouch/shape-switch, WalkStairs tuning, inner body, networking.
4. **Scene-query extensions**: whatever Phase 2 deferred (shape-sweep multi variants, overlap
   entity+body results) completed on UCk_Utils_JoltQuery_UE.
5. **Tests + gyms**: AS ImpulseChangesVelocity, TeleportMovesBodyAndResetsVelocity,
   ContactSignalsFireOnImpact, RestingBodySleeps… (upgrade to signal-level),
   FastProjectileWithCcdStopsAtThinWall, SphereRollsDownRampToBottom;
   JoltCharacter_ReportsGroundStateTransitions, MoveRequestDrivesCapsule,
   PushPolicyGovernsBoxDisplacement. Gyms: RampRoll, ProjectileCcd, SleepWake, Character.

## Risks

- Contact callbacks fire on Jolt worker threads mid-Update — event structs must copy everything
  (no Body pointers past the callback); queue-under-lock pattern is the law.
- CharacterVirtual push-vs-dynamic interaction quality depends on update ordering (character update
  BEFORE world step, per Jolt docs) — pinned by PushPolicyGovernsBoxDisplacement.
