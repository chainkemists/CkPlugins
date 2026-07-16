# PHASE 0 — Extract CkJolt module, zero behavior change

**Status:** DONE (2026-07-16 — CkFoundation `74c33059e`, host `32a28d6` + `5806b80`; gate: 783/783
full suite, Probe 16/16 + Crowd 16/16 + Eqs 10/10 on final binary, JobSystemSingleThreaded tripwire
confirmed in editor log)
**Gate:** existing full AS+C++ suite green vs pre-campaign baseline; JobSystem log tripwire; static greps.

## Entry criteria

- Baseline captured: full suite pass/fail counts + failing names recorded in PROGRESS.md BEFORE any edit.
- CkFoundation dev clean @ 02f404171.

## Work items (ordered; each compiles before the next)

1. **Scaffold CkJolt** (compiles empty):
   - `Source/CkJolt/CkJolt.Build.cs` — `CkModuleRules`; engine: Core, TraceLog, CoreUObject, Engine,
     DeveloperSettings; Ck: CkThirdParty, CkCore, CkEcs, CkLog, CkSettings.
   - `CkJolt_Module.h/.cpp`, `CkJolt_Log.h/.cpp` (category `CkJolt`, `CK_DEFINE_LOG_FUNCTIONS` in
     `namespace ck::jolt`), `CkJolt_Stats.h` (`STATGROUP_CkJolt`), `Claude.md`.
   - `CkFoundation.uplugin` entry (Runtime/Default/Win64+Mac+Linux). Requires `--generate` on next build.
2. **Move conversions**: `Public/CkJolt/CkJolt_Utils.h/.cpp` takes the `ck::jolt` Conv overloads,
   `Get_ShapeAxisCorrection_YToZ`, renamed generic `Get_BodyUserData` + new `TryGet_EntityFromBody`.
   Slim `CkSpatialQuery_Utils.h` re-exports the header (zero include churn in probe files), keeps the
   3 enum Conv overloads (enums stay in Probe until Phase 3) + `TryGet_ProbeFromBodyHit` shim.
   CkSpatialQuery.Build.cs += CkJolt.
3. **Move the world**:
   - `Public/CkJolt/CkJolt_ContactEvent.h`: `FCk_Jolt_ContactEvent` (verbatim FCk_ContactEvent fields)
     + `FCk_Jolt_OnContactEventsDrained` multicast delegate.
   - `Public/CkJolt/Subsystem/CkJolt_Subsystem.h/.cpp`: `UCk_Jolt_Subsystem` (tickable) — GJoltRefCount
     global init + Trace/AssertFailed hooks, temp allocator, JobSystem selection, the 3 layer/filter
     classes (names unchanged), CkContactListener/CkBodyActivationListener/CkJoltDebugger,
     `jolt.EnableParallelPhysics`/`jolt.EnableAsyncPhysicsUpdate` CVars verbatim,
     `SetContext<TWeakPtr<JPH::PhysicsSystem>>`, Tick = wait-async → DrainQueue → Broadcast →
     paused-check → Update → debug-draw (identical order to old lines 602-693). Debug-draw gated by a
     bridge-installed gate function (`Set_DebugDrawGate`).
   - `Public/CkJolt/Settings/CkJolt_ProjectSettings.h/.cpp`: `UCk_Jolt_ProjectSettings_UE` +
     `UCk_Utils_Jolt_ProjectSettings`, DisplayName "Jolt", 10 properties unchanged.
   - **SAME COMMIT**: `Config/DefaultCkFoundation.ini` section rename
     `[/Script/CkSpatialQuery.Ck_SpatialQuery_ProjectSettings_UE]` →
     `[/Script/CkJolt.Ck_Jolt_ProjectSettings_UE]` (pins `_EnableParallelPhysics=False`; missing this
     silently flips the suite to multithreaded Jolt — the gate's log tripwire exists for this).
   - Rewrite `CkSpatialQuery_Subsystem.h/.cpp` as non-tickable bridge (`UCk_Game_WorldSubsystem_Base_UE`):
     `InitializeDependency` on EcsWorld + Jolt subsystems, registers `ProcessQueuedContacts` (verbatim
     translation body) on the delegate, installs the debug-draw gate reading
     `UCk_Utils_SpatialQuery_Settings::Get_DebugPreviewAllProbesUsingJolt`.
   - `CkProbeTrace_Utils.cpp`: subsystem include + 4 `Get_WorldSubsystem<>` sites → `UCk_Jolt_Subsystem`.
     `CkProbeTrace_Processor.cpp:9` stale include deleted.
4. **CkWatermark**: widget cpp re-points to `UCk_Jolt_Subsystem`; Build.cs SpatialQuery→CkJolt.
5. **Docs in passing**: CkThirdParty CLAUDE.md+Claude.md (3 stale CkPerception spots), CkSpatialQuery
   CLAUDE.md (drop CkAggro, add CkJolt dep + bridge note), Source/CLAUDE.md (CkJolt row, CkSpatialQuery
   + CkWatermark rows, lookup row), CkWatermark Claude.md.

## Untouched by design

Probe/ProbeTrace quartets (except the 5 include/type sites above), CkEqs (zero edits), CkEcsDebugger,
CkCrowd, CkProjectile, CkTests, user settings (`CkSpatialQuery_Settings.h`) + all `ck.SpatialQuery.*` CVars.

## Exit criteria (all in the landing commits)

- Build green (Development Editor, `--generate` on the scaffold commit).
- Full AS+C++ suite green; explicitly named: 8 `Script/CkProbe` tests, `Script/CkCrowd` (incl.
  Separation_ProbeIsotropy), CkEqs, CkProjectile, CkOverlapBody.
- Test log contains "JobSystemSingleThreaded" evidence (ini migration tripwire) — NOTE: grep
  `Saved/Logs/CkPlugins.log` (the editor's own log), NOT the toolbox `--output` mirror, which
  filters sub-Display verbosity and never carries this line.
- Static greps: `CkSpatialQuery` CODE refs (includes/symbols) inside `Source/CkJolt/` == 0
  (comments naming the consumer are fine); `UCk_SpatialQuery_ProjectSettings` anywhere == 0.
- `[EDITOR-VERIFY]` (user): `ck.SpatialQuery.PreviewAllProbesUsingJolt 1` still draws wireframes in PIE;
  watermark panel shows Jolt threading stats.

## Risks

1. ini section rename missed/typoed → silent thread-pool flip. Tripwire above.
2. Bridge subsystem world-type gating parity (editor worlds have no Jolt subsystem — bridge must
   tolerate null; check `UCk_Game_WorldSubsystem_Base_UE` gating matches the tickable base).
