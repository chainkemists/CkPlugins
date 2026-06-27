// Language=angelscript
//============================================================================
// PLACEABLE TEST ENTITY SCRIPTS
//============================================================================
// Minimal EntityScripts that opt in to the editor's "Ck Entity Scripts" tab of
// the Place Actors panel via `default _ShowInPlaceActors = true;`.
//
// Drag any of these from that tab straight into a level — each drop spawns a
// configured ACk_EntitySpawner_UE with the script pre-assigned (no Blueprint
// wrapper). The spawner injects the placed actor's world transform into the
// script's `SpawnTransform` property (the default transform-injection target),
// which DoConstruct then applies to the entity.
//
// All three are non-replicated so they spawn immediately (no ActorRelay
// channel needed). The Cube/Sphere variants render an ISM mesh at the placed
// transform; the Marker is the minimal, dependency-free validator.
//============================================================================

// ---------------------------------------------------------------------------
// Marker — minimal placeable entity (transform + tag only). Confirms the panel
// + drag-to-spawn + transform injection with no visual dependency.
// ---------------------------------------------------------------------------
class UCk_PlaceableTest_Marker_EntityScript : UCk_GenericEntityScript_UE
{
    default _ShowInPlaceActors = true;
    default _Replication = ECk_Replication::DoesNotReplicate;

    UPROPERTY(ExposeOnSpawn)
    FTransform SpawnTransform = FTransform::Identity;

    UFUNCTION(BlueprintOverride)
    ECk_EntityScript_ConstructionFlow DoConstruct(FCk_Handle& InHandle)
    {
        utils_transform::Add(InHandle, SpawnTransform, ECk_Replication::DoesNotReplicate);
        utils_entity_tag::Add(InHandle, n"TAG_PlaceableTest_Marker");
        return ECk_EntityScript_ConstructionFlow::Finished;
    }
}

// ---------------------------------------------------------------------------
// Cube — visible placeable entity. Renders an ISM cube at the placed transform.
// ---------------------------------------------------------------------------
class UCk_PlaceableTest_Cube_EntityScript : UCk_GenericEntityScript_UE
{
    default _ShowInPlaceActors = true;
    default _Replication = ECk_Replication::DoesNotReplicate;

    UPROPERTY(ExposeOnSpawn)
    FTransform SpawnTransform = FTransform::Identity;

    UFUNCTION(BlueprintOverride)
    ECk_EntityScript_ConstructionFlow DoConstruct(FCk_Handle& InHandle)
    {
        utils_transform::Add(InHandle, SpawnTransform, ECk_Replication::DoesNotReplicate);
        utils_entity_tag::Add(InHandle, n"TAG_PlaceableTest_Cube");

        auto IsmProxyParams = FCk_Fragment_IsmProxy_ParamsData(ck::Asset_PlaceableTest_Cube);
        auto IsmProxyTransform = InHandle.As_Transform();
        utils_ism_proxy::Add(IsmProxyTransform, IsmProxyParams);

        return ECk_EntityScript_ConstructionFlow::Finished;
    }
}

// ---------------------------------------------------------------------------
// Sphere — visible placeable entity. Renders an ISM sphere at the placed
// transform. A second visible entry so the panel shows multiple scripts.
// ---------------------------------------------------------------------------
class UCk_PlaceableTest_Sphere_EntityScript : UCk_GenericEntityScript_UE
{
    default _ShowInPlaceActors = true;
    default _Replication = ECk_Replication::DoesNotReplicate;

    UPROPERTY(ExposeOnSpawn)
    FTransform SpawnTransform = FTransform::Identity;

    UFUNCTION(BlueprintOverride)
    ECk_EntityScript_ConstructionFlow DoConstruct(FCk_Handle& InHandle)
    {
        utils_transform::Add(InHandle, SpawnTransform, ECk_Replication::DoesNotReplicate);
        utils_entity_tag::Add(InHandle, n"TAG_PlaceableTest_Sphere");

        auto IsmProxyParams = FCk_Fragment_IsmProxy_ParamsData(ck::Asset_PlaceableTest_Sphere);
        auto IsmProxyTransform = InHandle.As_Transform();
        utils_ism_proxy::Add(IsmProxyTransform, IsmProxyParams);

        return ECk_EntityScript_ConstructionFlow::Finished;
    }
}
