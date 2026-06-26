// Language=angelscript
//============================================================================
// PLACEABLE TEST — VISUAL MESH ASSETS
//============================================================================
// Heavyweight `asset ... of ...` declarations live in a *_Assets.as file (per
// the AngelScript authoring guide). These IsmRenderer data assets give the
// placeable test EntityScripts a visible mesh when dropped into a level.
//============================================================================

namespace ck
{
    asset Asset_PlaceableTest_Cube of UCk_IsmRenderer_Data
    {
        _Mesh = Cast<UStaticMesh>(utils_i_o::LoadAssetByName("/Engine/BasicShapes/Cube.Cube",
            ECk_AssetSearchScope::Engine)._Asset);
        _Mobility = ECk_Mobility::Movable;
    }

    asset Asset_PlaceableTest_Sphere of UCk_IsmRenderer_Data
    {
        _Mesh = Cast<UStaticMesh>(utils_i_o::LoadAssetByName("/Engine/BasicShapes/Sphere.Sphere",
            ECk_AssetSearchScope::Engine)._Asset);
        _Mobility = ECk_Mobility::Movable;
    }
}
