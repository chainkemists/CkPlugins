# PHASE 1 — Static world baking + cooked data + streaming

**Status:** PENDING (blocked by Phase 0 gate)
**Gate:** green compile + Phase-1 tests + Phase-0 regression suite + gym station.

## Work items (ordered)

1. **Cooked data types first** (no churn later):
   `Public/CkJolt/StaticWorld/CkJoltStaticWorld_Data.h/.cpp` — `FCk_Jolt_CookedBodyRecord`
   {ShapeIndex, Position, Rotation (scale baked into shape), Signature, Friction, Restitution,
   SurfaceType}, `FCk_Jolt_CookedActorGroup` {SourceActorName, SourceActorPath(debug), SourceHash,
   RuntimeCheckHash, Bodies}, `UCk_Jolt_CookedCell_UE : UDataAsset` {CookVersion, JoltVersionId,
   CellId, ShapeBlob (SaveWithChildren stream), ShapeCount, ActorGroups},
   `UCk_Jolt_CookedWorldIndex_UE : UDataAsset` {CookVersion, JoltVersionId, SourceMapPackage, Cells
   (soft refs + bounds), ActorLookup}, `ck::jolt::CookVersion_Current` constexpr.
   `Public/CkJolt/CollisionLayers/CkJoltCollisionLayer_Data.h/.cpp` — `ECk_Jolt_BodyDomain`
   {Static, Dynamic}, `ECk_Jolt_PairInteraction` {Ignore, Overlap, Block},
   `FCk_Jolt_CollisionSignature` {ObjectChannel, 2-bit×32 ResponseMask uint64, CollisionEnabled,
   Domain} — hashable, ==. (Table itself is Phase 2; Phase-1 bodies go on existing `Non_Moving`
   layer while records carry real captured signatures.)
2. **Extraction library**: `Public/CkJolt/StaticWorld/CkJoltBakeExtraction.h/.cpp`
   (`namespace ck::jolt::bake`). `ExtractActor(AActor&, sink)` sweep order:
   USplineMeshComponent (before StaticMesh — subclass; per-instance deformed `GetBodySetup()`) →
   UInstancedStaticMeshComponent incl. HISM (shape cache keyed (BodySetupGuid, scale, traceflag);
   `< _CompoundShapeInstanceThreshold` → per-instance bodies sharing JPH::Ref, else one
   StaticCompoundShape per component; per-instance nonuniform scale → ScaledShape) →
   UStaticMeshComponent (AggGeom: Box/Sphere→direct, Sphyl→Capsule w/ Y→Z RotatedTranslated,
   Convex→ConvexHullShape, multi-elem→StaticCompoundShape w/ GetTransform() locals;
   CTF_UseComplexAsSimple → TriMeshGeometries[0] → MeshShape w/ b/c winding swap; no valid
   collision → CK_ENSURE + skip) → UBrushComponent (BrushBodySetup ConvexElems) →
   ULandscapeComponent per ALandscapeProxy incl. StreamingProxy (`#if WITH_EDITOR`,
   FLandscapeComponentDataInterface heights → HeightFieldShapeSettings, Y→Z wrap + row flip,
   visibility holes → no-collision samples).
   Skip: unregistered / IsEditorOnly / IsVisualizationComponent / NoCollision / IsSimulatingPhysics /
   Mobility==Movable. Capture GetCollisionObjectType + GetCollisionResponseToChannels +
   GetCollisionEnabled per body. Build.cs: +PhysicsCore; +Landscape editor-conditional.
3. **Live path + streaming container**: `Public/CkJolt/StaticWorld/CkJoltStaticWorld_Subsystem.h/.cpp`
   (`UCk_JoltStaticWorld_Subsystem_UE` + `ck::jolt::FStaticWorldBodies`): LevelAdded/RemovedFromWorld
   delegates; actor-FName lookup → LoadSynchronous cells → sRestoreWithChildren once per cell
   (refcounted) → batch AddBodiesPrepare/Finalize(DontActivate); removal → RemoveBodies/DestroyBodies;
   OptimizeBroadPhase policy (once post-populate; dirty>512 or any-removal → before next Update,
   never mid-async). PIE mode from `_PIEStaticWorldMode` (LiveExtract default / Cooked / Disabled).
   `Public/CkJolt/StaticWorld/CkJoltStaticWorld_Utils.h/.cpp` — `UCk_Utils_JoltStaticWorld_UE`
   (`Request_BakeActor/RemoveActor`, `Get_NumStaticBodies`, `Get_NumUniqueShapes`; ScriptMixin-less
   BPFL — no handle type; AS surface via UFUNCTIONs).
4. **AS parity autotests + heightfield C++ test** (heightfield test lands BEFORE landscape map work).
5. **Editor module**: `Source/CkJoltEditor/` — Build.cs (Editor type; UnrealEd, Landscape,
   AssetRegistry + CkJolt/CkCore/CkLog), Module, `Public/CkJoltEditor/Cook/CkJoltCook_WorldCooker.h/.cpp`
   (shared cooker; WP via FWorldPartitionHelpers::ForEachActorWithLoading; bake-grid assignment by
   body position; SavePackage index + cells; orphan cleanup), `CkJoltCook_EditorSubsystem.h/.cpp`
   (UEditorSubsystem: Cook_CurrentWorld/Cook_Map/Cook_AllMaps/Validate_Map + Tools-menu entry),
   `CkJoltCook_Commandlet.h/.cpp` (`-run=CkJoltCook -Map=… | -AllMaps [-DryRun] [-Report=…]`;
   CK_ENSUREs DirectoriesToAlwaysCook ini entry). uplugin + EDITOR_MODULES.md + tier table.
6. **Cooked load path** in the static-world subsystem + C++ cook tests + test content.
7. **Gym station** + Claude.md updates.

## Settings additions (UCk_Jolt_ProjectSettings_UE)

`_CookedDataRootPath` (/Game/CkJoltData), `_BakeGridCellSize` (25600), `_CompoundShapeInstanceThreshold`
(32), `_BroadphaseOptimizeThreshold` (512), `_PIEStaticWorldMode`, `_CookExcludedMapPathPrefixes`.

## Tests

AS (`Plugins/CkTests/Script/CkJolt/`): StaticBake_SimpleBox / MultiElemConvex / ComplexTrimesh
_RaycastMatchesChaos, StaticBake_Hism_PerInstanceBodies_Parity (8 inst + NumUniqueShapes==1),
StaticBake_Hism_CompoundCluster_Parity (100 inst + body count 1), StaticBake_SplineMesh_
DeformedTrimeshParity, StaticBake_NoCollisionMesh_EnsuresLoudly (expected errors + rays miss),
Streaming_SubLevelLoadUnload_BodiesFollow.
C++ (`Source/CkTests/Private/UnitTests/CkJolt/`): Test_JoltBake_HeightField_KnownHeightsZUp,
Test_JoltCook_RoundTrip_CookedMatchesLiveExtraction, Test_JoltCook_StaleHash_EnsuresAndSkips,
Test_JoltCook_VersionMismatch_EnsuresAndSkips, Test_JoltCook_BlockingVolume_ConvexFromBrush,
Test_JoltCook_Landscape_HeightfieldMatchesLandscapeData.
Content (`Plugins/CkTests/Content/CkJolt/`): SM_JoltComplexOnly, SM_JoltMultiElem,
TestMaps/L_JoltBake_Volumes, L_JoltBake_Landscape, L_JoltBake_StreamingSub.
NOTE: .umap/.uasset authoring needs the editor — create minimal maps programmatically via
editor-subsystem/commandlet path or flag `[EDITOR-VERIFY]` map-authoring steps for the user.

## Risks

1. Actor-FName key stability through cooked WP pipeline → RuntimeCheckHash makes drift loud;
   packaged smoke after item 6; composite-key fallback already in format.
2. Commandlet world boot → editor-subsystem path ships first; WorldPartitionBuilder pivot contained.
3. Heightfield axis/mirroring → known-heights C++ test before any landscape content.
