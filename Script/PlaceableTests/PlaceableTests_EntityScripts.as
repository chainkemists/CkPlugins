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
// All of these are non-replicated so they spawn immediately (no ActorRelay
// channel needed). The Cube/Sphere variants render an ISM mesh at the placed
// transform; MeshComponent renders a real UStaticMeshComponent through
// CkUnrealComponent (the other editor-preview visual path); the Marker is the
// minimal, dependency-free validator.
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

// ---------------------------------------------------------------------------
// MeshComponent — visible placeable entity. Renders a real UStaticMeshComponent
// (cylinder) through CkUnrealComponent — the OTHER editor-preview visual path
// next to Cube/Sphere's shared-ISM rendering. The component is deliberately
// mounted on a SCENE-NODE CHILD (offset 1m up) rather than the script entity:
// scene nodes are lifetime children of their anchor, so editor-selection-owner
// resolution must walk through them — clicking the floating cylinder selects
// the spawner below it.
// Component setup is async: the mesh asset is assigned in the OnAdded handler.
// ---------------------------------------------------------------------------
class UCk_PlaceableTest_MeshComponent_EntityScript : UCk_GenericEntityScript_UE
{
    default _ShowInPlaceActors = true;
    default _Replication = ECk_Replication::DoesNotReplicate;

    UPROPERTY(ExposeOnSpawn)
    FTransform SpawnTransform = FTransform::Identity;

    UFUNCTION(BlueprintOverride)
    ECk_EntityScript_ConstructionFlow DoConstruct(FCk_Handle& InHandle)
    {
        utils_transform::Add(InHandle, SpawnTransform, ECk_Replication::DoesNotReplicate);
        utils_entity_tag::Add(InHandle, n"TAG_PlaceableTest_MeshComponent");

        auto TransformHandle = InHandle.As_Transform();
        auto MountLocalTransform = FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, 100.0), FVector::OneVector);
        auto MountSceneNode = utils_scene_node::Create(TransformHandle, MountLocalTransform);
        auto MountEntity = FCk_Handle(MountSceneNode);

        const auto Params = utils_unreal_component::Make_Params(UStaticMeshComponent, ECk_UnrealComponent_TickPolicy::DoNotTick, n"PlaceableTest_MeshComponent");
        auto ComponentHandle = utils_unreal_component::Add(MountEntity, Params);

        utils_unreal_component::BindTo_OnAdded(
            ComponentHandle,
            FCk_Delegate_UnrealComponent_OnAdded(this, n"OnMeshComponentAdded"));

        return ECk_EntityScript_ConstructionFlow::Finished;
    }

    UFUNCTION()
    private void OnMeshComponentAdded(FCk_Handle_UnrealComponent InHandle)
    {
        auto Mesh = Cast<UStaticMeshComponent>(utils_unreal_component::Get_Component(InHandle));
        if (Mesh == nullptr) { return; }

        auto Cylinder = Cast<UStaticMesh>(LoadObject(this, "/Engine/BasicShapes/Cylinder.Cylinder"));
        if (Cylinder != nullptr) { Mesh.SetStaticMesh(Cylinder); }

        Mesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
