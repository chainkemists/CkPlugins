// Auto-generated EntityScript spawn-params — DO NOT EDIT.
// This file is regenerated on editor startup and after every AngelScript recompile.
//
// For each UCk_EntityScript_UE subclass, two declarations are emitted:
//   - FCk_MyEntityScript_SpawnParams  (file-scope USTRUCT, unique name — avoids the
//     `Params` name-collision across namespaces that trips the Unreal naming check)
//   - namespace UCk_MyEntityScript { FCk_MyEntityScript_SpawnParams Params() { ... } }
//     so callers can still write `UCk_MyEntityScript::Params()`.
//
// Properties are flattened across the hierarchy (AS has no struct inheritance). Non-
// trivial struct defaults outside the CkReflection_Utils allowlist are emitted without
// an initializer — set them on the instance before calling Request_SpawnEntity.

USTRUCT()
struct FGridSystem_C_SpawnParams
{
}

namespace UGridSystem_C
{
    FGridSystem_C_SpawnParams Params()
    {
        return FGridSystem_C_SpawnParams();
    }
}

