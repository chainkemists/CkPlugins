# PHASE 2 — Collision layer mapping + scene-query extensions

**Status:** DONE (2026-07-16 — CkFoundation `4b73f9f80`, CkTests `f594983`; gate: Jolt 10/10,
Probe 17/17, Crowd 16/16, Eqs 10/10). Deviations: the AS profile-matrix test is covered by the
C++ table test instead (no BP surface expansion for test-only introspection); CkEqs untouched
(its JPH-typed overload usage keeps working — optional migration remains optional).

## Work items

1. **Table + filters**: `Public/CkJolt/CollisionLayers/CkJoltCollisionLayerTable.h/.cpp` —
   `ck::jolt::FCollisionLayerTable`: `TArray<FCk_Jolt_CollisionSignature>` (index == JPH::ObjectLayer;
   fixed capacity reserved at init, appends under game-thread lock, atomic count publish — Jolt filter
   callbacks are worker-thread lock-free reads), `TMap<Signature, uint16>` resolve-or-register.
   `Build_FromCollisionProfiles()`: iterate `UCollisionProfile::Get()` GetNumOfProfiles/
   GetProfileByIndex; register (profile→Static) + (profile→Dynamic) signatures for every template with
   CollisionEnabled != NoCollision; deterministic seed order. Pair rule:
   `min(a.Response[b.Channel], b.Response[a.Channel])`; `FObjectLayerPairFilter_Table::ShouldCollide`
   = interaction != Ignore. `FBroadPhaseLayerInterface_Table`: layer→Domain (Static=0, Dynamic=1).
   `FObjectVsBroadPhaseLayerFilter_Table`: Static-vs-Static false, else true.
   Per-query `FObjectLayerFilter_Channel` {table, channel, min-response} for Block-vs-Overlap
   resolution at query sites.
   Subsystem swaps the hardcoded 2-layer classes for table-driven ones.
   **Probe default signature**: `ProbeDefault_Signature()` = WorldDynamic, Ignore statics, Overlap
   dynamics, Domain Dynamic — preserves probe-pairs-with-probes, avoids pairing with baked statics.
   `CkJoltCollisionLayers_Utils.h/.cpp` test introspection: `Get_ShouldCollide_ByProfileNames`,
   `Get_ResponseOfProfileToChannel`, `Get_NumRegisteredLayers`.
2. **Loaders resolve stored signatures → table layers** (record format already carries signatures).
3. **Channel-filtered scene queries**: `Public/CkJolt/Query/CkJoltQuery_Data.h` +
   `CkJoltQuery_Utils.h/.cpp` — `UCk_Utils_JoltQuery_UE`: `Get_RayCast_Single/_Multi`,
   `Get_ShapeCast_Single/_Multi` (FCk_Jolt_ShapeDimensions), `Get_Overlap`; all take
   `FCk_Jolt_QueryFilter` {LayerSource Channel|Profile, CollisionChannel, CollisionProfile,
   IgnoredEntities, BackFace modes}. Pure reads = `Get_*` naming (deliberate divergence from
   ProbeTrace's side-effectful `Request_*`).
4. **Tests.**

## Tests

AS: Layers_EngineProfilePairs_MatchChaosMatrix, Query_ChannelRaycast_HonorsBlockOverlapIgnore,
Query_SweepByChannel_MatchesChaosSweep, Probe_DefaultSignature_IgnoresStaticWorld (stats-based).
C++: Test_JoltLayers_TableBuild_FromCollisionProfiles, Test_JoltLayers_SignatureRegistration_
GrowthAndLookup (capacity/no-reallocation invariant).

## Notes

- CkEqs keeps calling CkProbeTrace_Utils' JPH-typed overloads — migrating CkEqs onto CkJolt query
  primitives is OPTIONAL scope here; do it only if the probe re-layering forces it (behavior gate:
  CkEqs tests stay green either way).
