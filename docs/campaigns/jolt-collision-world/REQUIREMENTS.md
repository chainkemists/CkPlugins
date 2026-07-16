/goal Extract CkFoundation's existing Jolt integration out of CkSpatialQuery into a new module, CkJolt, then extend it into a full collision world with Chaos parity: static world baking AND dynamic body simulation. Today, `UCk_SpatialQuery_Subsystem` (inside CkSpatialQuery) owns a live JPH::PhysicsSystem with broadphase, contact/activation listeners, debug-render scaffolding, and parallel/async tick options, and the Probe feature creates real Jolt bodies (Box/Sphere/Capsule/Cylinder, Static/Kinematic/Dynamic motion tags, a CCD/LinearCast tag) as an implementation detail of its overlap-query semantics. That conflates "owns the Jolt world" with "one feature built on the Jolt world." Study UnrealJolt (<path here>) for its cooking pipeline and layer config as a reference, but the target is CkFoundation-native: a new lower-tier module that owns the world, with CkSpatialQuery becoming a consumer of it.

## Module Restructuring: extract CkJolt

- **New module `CkJolt`**, sitting below CkSpatialQuery in the dependency tier table (roughly where CkPhysics/CkChaos sit — depends on CkCore, CkEcs, CkLog, CkThirdParty; does NOT depend on CkSpatialQuery, CkShapes-as-feature, or CkProvider). Owns:
  - The Jolt world itself: migrate `UCk_SpatialQuery_Subsystem`'s JPH::PhysicsSystem, broadphase/object-layer filters, `CkContactListener`, `CkBodyActivationListener`, `JPH_DEBUG_RENDERER` scaffolding, and the `_ParallelPhysicsEnabled`/`_PhysicsThreadCount`/`_AsyncPhysicsUpdate` tick options.
  - Generic Jolt body primitives: shape creation (Box/Sphere/Capsule/Cylinder and, per Scope Part 1, baked static/complex shapes), body create/destroy tied to entity lifetime, the CCD/motion-type tag vocabulary currently living on `FFragment_Probe_Current`.
  - Static world baking (Scope Part 1 below) — this is world-ownership, not a query feature, so it belongs here rather than in CkSpatialQuery.
  - Collision layer mapping (UE channels/profiles → Jolt ObjectLayers/BroadphaseLayers).
  - Scene-query primitives (raycast/sweep/overlap against the Jolt world) — migrate `CkProbeTrace_Utils`/`_Processor`'s underlying trace logic here as the generic entry point; CkSpatialQuery's Probe becomes a thin caller.
- **CkSpatialQuery keeps**: the Probe feature's ECS surface — `FFragment_Probe_Current`, `UCk_Utils_Probe_UE`, the `FCk_Request_Probe_*` family, begin/end-overlap signals — but re-expressed as a consumer of CkJolt's body/query primitives instead of owning them. CkSpatialQuery's public API and existing consumers (CkAggro, CkCrowd, CkEqs, CkWatermark — all currently declared dependents per the module tier table) must not observe a behavior change from the extraction alone.
- Dynamic/kinematic rigid-body simulation (Scope Part 2) is new capability that should be designed as CkJolt's own feature (its own fragment/processor/request set for full rigid-body dynamics), not bolted onto Probe — Probe's job is volume queries, not general-purpose body ownership. Character/CharacterVirtual proxies likewise belong in CkJolt.
- This is a real module split (new Build.cs, uplugin entry, tier-table row, moved source files, updated includes across CkSpatialQuery and any other direct Jolt/CkThirdParty-Jolt consumers found in Research) — treat it as Phase 0, gated and tested (existing CkSpatialQuery/Probe AutoTests must stay green through the move) before any new capability is added on top.

## Research phase — mandatory before any implementation

1. Read `CkSpatialQuery/Claude.md`, `CkThirdParty/Claude.md` (flagged stale — says Jolt is "exposed only through CkPerception"; the real and only current consumer is CkSpatialQuery, fix the doc in passing), and every file under `CkSpatialQuery/Public/CkSpatialQuery/{Subsystem,Probe}/`.
2. Grep the full Plugins/ tree for any other direct JoltPhysics/CkThirdParty-Jolt includes outside CkSpatialQuery — confirm CkSpatialQuery is the sole consumer before assuming the extraction is a clean, isolated move.
3. Produce a written comparison: UnrealJolt's cooking pipeline/layer config vs. what `UCk_SpatialQuery_Subsystem` already does, PLUS the CkJolt/CkSpatialQuery split boundary (exactly which classes/files move, which stay, what the new inter-module API looks like). Resolve in writing before touching code:
   - **CkJolt's dynamic-body feature shape**: fragment/processor/request naming for full rigid-body dynamics (mass/inertia/restitution/impulses) — don't assume `FCk_Fragment_JoltBody_*` naming up front; derive it from a feature quartet the way CkTimer is the canonical exemplar, scoped to CkJolt not CkSpatialQuery.
   - **JobSystem**: `_ParallelPhysicsEnabled`/`_PhysicsThreadCount`/`_AsyncPhysicsUpdate` already exist — audit what they currently do (UE task graph vs. Jolt's own thread pool) before deciding whether to change it, and before deciding where they live post-split. (UnrealJolt's UE-thread-pool integration is admittedly incomplete — do not copy it blindly.)
   - **Debug render**: `JPH_DEBUG_RENDERER` is already scaffolded — audit what it currently draws before building a new overlay, and decide whether it moves to CkJolt wholesale.

## Scope Part 1: Static World Baking (confirmed net-new — no StaticMesh/Landscape/HISM/bake code exists today; lives in CkJolt per the restructuring above)

Bake the following into the CkJolt world, matching what Chaos sees at runtime:

- **Static meshes** — Bake ONLY the collision representation the mesh is authored to use, per BodySetup's Collision Complexity:
    - `UseSimpleAsComplex` / default → bake convex hulls/boxes/spheres/capsules from AggGeom
    - `UseComplexAsSimple` → bake the triangle mesh
    - Never bake render geometry when a simple representation exists.
- **Instanced static meshes / HISM / foliage** — one Jolt shape per unique mesh, instanced across bodies. Evaluate `JPH::StaticCompoundShape` for dense kitbashed clusters — thousands of individual bodies is the known perf trap.
- **Landscape + LandscapeSplines** — UnrealJolt auto-cooks on PIE; use its approach as a starting point but fix the "must PIE every level once" limitation — cooking must be invokable as a commandlet/editor action across all levels.
- **Spline mesh components** — deformed geometry; cook the deformed tri-mesh per instance.
- **Blocking volumes / brush geometry** — convex, trivial, do not forget them.
- **Nanite meshes** — bake from the fallback/proxy collision mesh Chaos uses, never Nanite render data.

## Scope Part 2: Dynamic & Kinematic Bodies (new CkJolt feature)

- **Dynamic rigid bodies** — full simulation: shape from the entity's authored collision (same complexity rules as static), mass/COM/inertia matching UE BodyInstance settings, restitution/friction from PhysicalMaterials mapped to Jolt materials.
- **Kinematic bodies** — both directions required:
    - *ECS → Jolt*: entities whose transform is authoritative elsewhere push into Jolt via `MoveKinematic` (velocity-based, not teleport).
    - *Jolt → ECS*: simulated bodies write transforms back to Fragments; downstream scene-component sync goes through the existing Ck transform flow, never a per-body game-thread actor update.
- **Character/NPC proxies** — Jolt's `Character`/`CharacterVirtual` are already vendored in `CkThirdParty` (`Source/CkThirdParty/Public/CkThirdParty/JoltPhysics/Jolt/Physics/Character/`) but unused — wire them up in CkJolt rather than treating this as a new dependency. Must interact correctly with dynamic bodies (push/be-pushed policy via enum, not bool). CkCrowd already depends on CkSpatialQuery/Physics/Navigation — verify it isn't broken by the CkJolt split or by this addition.
- **Request structs** — follow the established Ck request pattern (constructor essentials + fluent `Set_*` optionals, `CK_REQUEST_DEFINE_DEBUG_NAME`) for forces/impulses/velocity/teleport/activate-deactivate, scoped to CkJolt. Processors drain requests before the sim step, following the established Setup → HandleRequests → Update phase vocabulary.
- **Collision events → signals** — Probe already has begin/end overlap signals (`BindTo_OnBeginOverlap` etc. via `UCk_Utils_Probe_UE`); CkJolt's contact-added/persisted/removed and sensor-overlap events should follow the same `CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE` pattern. Jolt contact callbacks fire on worker threads — marshal to a deterministic point in the frame (a HandleRequests-equivalent phase), never invoke delegates from Jolt threads.
- **Scene queries** — extend CkJolt's migrated trace primitives with shape-sweep/overlap variants and UE-channel-equivalent filtering.
- **CCD** — the CCD/motion-type tag vocabulary migrates from Probe to CkJolt; map UE's per-body CCD flag onto Jolt's `LinearCast` motion quality (verify the current tag's exact semantics in Research phase before assuming it's already equivalent).
- **Sleeping** — respect Jolt's sleep system via the migrated `CkBodyActivationListener`; expose sleep state + wake requests; iterate only Jolt's active-body list in the ECS sync path.
- **Lifecycle** — body creation/destruction tied to entity lifetime (the standard Ck composition ritual — CreateEntity → label → fragments/tags → Record connect), batch-added when spawning many via `BodyInterface::AddBodiesPrepare/Finalize`. Streaming a cell out destroys its bodies cleanly.

Explicit **ownership rule**: an entity is EITHER Chaos-simulated (CkOverlapBody's UShapeComponent path, or CkRaySense's `UKismetSystemLibrary::LineTraceSingle` path) OR Jolt-simulated (CkJolt, including CkSpatialQuery's Probe), never both. Make this structurally impossible via fragment/composition design rather than a runtime error log — an entity that has both an OverlapBody fragment and a CkJolt body fragment should be a composition-time conflict, not a logged warning.

## Collision Layer Mapping

Map UE collision channels/profiles → Jolt ObjectLayers + BroadphaseLayers so filtering matches Chaos, owned by CkJolt. Data-driven from existing UE collision profiles, not manually re-authored. Static and Dynamic broadphase layers are mandatory — audit what the migrated broadphase/object-layer filter classes already implement before adding new ones.

## Cooked Data & Streaming

- Cooked Jolt data serialized to a `JoltData`-style directory included in packaging (per UnrealJolt), owned by CkJolt.
- Partitioned per streaming cell/level so bodies add/remove in lockstep with World Partition / level streaming.
- Cook versioning + source-geometry hash so stale data is detected and re-cooked, never silently used.

## Performance Requirements (utmost priority — goal is to beat Chaos)

- The subsystem already exposes `_ParallelPhysicsEnabled`/`_PhysicsThreadCount`/`_AsyncPhysicsUpdate` — Research phase must document what these currently do before deciding whether UE's task graph or Jolt's own JobSystem/thread pool is in use, and whether to change it post-migration.
- Verify worst-case clustered-pile contact-island behavior (single giant island serializes onto one core) even though the NPC/vehicle-spread workload should suit the island model.
- Fixed-timestep sim with interpolation for render transforms; document where the Jolt step sits relative to Ck's processor phases (Setup/HandleRequests/Update/etc.).
- Transform sync is ECS-only and iterates Jolt's active-body list, not all bodies.
- Follow Jolt determinism guidelines so cross-platform determinism / future rollback stays on the table.

## Parity Validation (definition of "done")

1. **Geometry parity** — CkTests: per level, sample N random raycasts/shape-sweeps/overlaps against both Chaos and the CkJolt world; hit/miss and surface results must match within tolerance. Report per-primitive-type mismatch counts.
2. **Dynamic behavior parity** — CkTests scenarios (box stack, ramp roll, projectile CCD, kinematic platform carrying a dynamic body) in both engines; trajectories need not match exactly but must be qualitatively equivalent and stable.
3. **Debug draw overlay** — extend the migrated `JPH_DEBUG_RENDERER` path rather than building new; Jolt wireframe over Chaos collision (distinct colors), CVar-toggleable, static AND dynamic, sleep-state coloring.
4. **Benchmark harness** — CkTests scenario spawning N dynamic bodies (500 / 2k / 10k): sim step + transform sync cost, Chaos-via-actors vs. CkJolt-via-ECS. Numbers in VALIDATION.md.
5. **Extraction regression check** — every existing CkSpatialQuery/Probe AutoTest passes unchanged after the CkJolt split, and every declared dependent module (CkAggro, CkCrowd, CkEqs, CkWatermark) builds and tests clean against the new dependency shape.

## Testing (CkTests) — mandatory per phase, not a phase-5 afterthought

Every phase in the Process split below ships its own AutoTests (and a Gym station where the feature is visual/interactive) before being marked done — do not defer all testing to Scope Part "Parity Validation." Use CkTests' four pipelines (`Plugins/CkTests/CLAUDE.md`) by fit, not by default:

- **PIE AS autotests (pipeline a, preferred default)** — one `UCk_AutoTest_Base` subclass per `.as` file at `Script/CkJolt/CkAutoTest_CkJolt_<Scenario>.as` (and `Script/CkSpatialQuery/CkAutoTest_SpatialQuery_<Scenario>.as` for Probe-level regression). Scenario names state what's VERIFIED (`DynamicBoxRestsOnStaticFloor`, `KinematicPlatformCarriesDynamicBody`), never what the code does. Timeout is `default _TimeoutSeconds = X.Xf;` on the entity script CDO — NOT on the actor wrapper (a stale convention some old docs still show); the wrapper generator propagates it. Use this pipeline for: single-body/query-level correctness (shape creation, overlap signals, request handling, CCD tag behavior, sleep/wake) and the geometry-parity sampling (item 1 below) — Chaos and Jolt raycast/sweep/overlap results are both reachable from AS Utils.
- **Multi-PIE net autotests (pipeline b)** — only if kinematic ECS→Jolt push or Jolt→ECS transform sync needs network-authority coverage (e.g. a server-authoritative kinematic platform driven by a replicated entity). `Script/CkJolt/CkAutoTest_Net_<Name>.as` subclassing `UCk_AutoTest_NetBase`. **A new/renamed net test needs a full C++ rebuild before it shows up** (the stub generator emits `.spec.cpp`; AS recompile alone is not enough) — budget for this in the phase-3 (dynamic/kinematic) implementation loop.
- **Hand-written C++ automation (pipeline c)** — for anything AS can't reach: raw `JPH::PhysicsSystem`/`BodyInterface` state assertions, the benchmark harness (item 4 — needs `SCOPE_CYCLE_COUNTER`/`CkProfile` timing around the sim step and transform sync, not just pass/fail), and the extraction regression check's build-level assertions. Layout: `Source/CkTests/Private/UnitTests/CkJolt/Test_<Subject>_<Scenario>.cpp`. **A new `.cpp` test compiles but shows "No tests matched" until a touch+rebuild forces a relink** — don't assume `--generate` alone surfaces it.
- **Gauntlet** — CkTests ships the framework only (zero in-plugin tests); if the benchmark harness needs a true headless process-level run (no PIE overhead skewing the numbers), that's a host-authored (CkPlugins-side) Gauntlet test class, not something CkTests itself provides.
- **Gym stations (interactive, manual/visual — not automation)** — register in `Script/Common/CkTests_GymRegistry.as`, scripts co-located at `Script/CkJolt/` (there is no `Script/CkGyms/` directory). One station per major dynamic-body scenario mirroring the parity-validation list: box stack, ramp roll, projectile CCD, kinematic platform carrying a dynamic body, sleep/wake visualization, and the debug-draw overlay toggle (CVar-driven Jolt-vs-Chaos wireframe) — the overlay in particular is only meaningfully checkable visually, label its verification `[EDITOR-VERIFY]`. Stations face world -X by convention (`ECk_GymStation_Anchor::AgentSpawn*`, or keep content in -X) — don't fight the camera default.
- **Warning-escalation caveat**: the AutoTest harness fails a test on any `Warning`-level log during the run. New `CK_ENSURE_IF_NOT` calls in CkJolt/CkSpatialQuery code must not fire spuriously under normal test scenarios; if a specific scenario needs a Warning to legitimately occur, opt out per the existing gym/PIE-only escalation pattern rather than downgrading the log level globally.
- **Static-baking coverage (Phase 1)** needs its own AutoTests too: cook a small test level's worth of static meshes/HISM/landscape/splines/brushes/Nanite-fallback into the CkJolt world and assert against Chaos's collision — this is the seed for the geometry-parity harness in item 1, not a separate one-off.

## Non-Goals

- NOT replacing Chaos engine-wide. Chaos keeps CharacterMovement (for actual Characters), ragdolls, cloth, Niagara collision, and engine-facing scene queries (nav/EQS/perception). CkJolt owns ECS-driven simulation + its static world mirror.
- No constraint/joint system in this campaign — but don't architect anything that blocks adding Jolt constraints to CkJolt later.
- No fallbacks that hide problems: if a mesh has no valid collision to bake, CK_ENSURE and log loudly — never silently substitute a bounding box.

## Process

- Campaign methodology: `docs/campaigns/jolt-collision-world/` with PROMPT.md, PHASE_N.md, PROGRESS.md, VALIDATION.md (`ck-methodology` skill).
- Research → Plan → Implement. Research phase output: the CkJolt/CkSpatialQuery split boundary, the UnrealJolt-vs-existing comparison, the dynamic-body feature shape decision, and the JobSystem audit — all in writing before implementation starts.
- Suggested phase split: (0) extract CkJolt module with zero behavior change, regression-gated against existing CkSpatialQuery/Probe tests, (1) static world baking, (2) layer mapping + scene query extensions, (3) dynamic/kinematic bodies + ECS sync, (4) events/signal integration + request structs, (5) parity validation + benchmarks. Adjust in Plan phase if Research surfaces different dependencies. **Each phase's gate includes its own AutoTests (and a Gym station if the feature is visual/interactive) per the Testing section above — a phase is not done at green compile, it's done at green compile + green tests + (where applicable) a working gym station.**
- All code follows CkFoundation standards (root CLAUDE.md + `ck-macros-and-codegen`, `ckecs-architecture-contract` skills): auto + trailing return types, CK_PROPERTY encapsulation, CK_ENSURE_IF_NOT early-outs, enum-over-bool, three-environment compatibility (C++/BP/AngelScript). New-module rules (Build.cs via `CkModuleRules`, uplugin entry, tier-table row, `Claude.md`) per `Source/CLAUDE.md`'s "Module-authoring rules."
