# PHASE 5 — Parity validation + benchmarks + debug overlay

**Status:** PENDING (blocked by Phase 4 gate)
**Gate:** VALIDATION.md fully populated with real numbers; final full-suite regression vs baseline;
all `[EDITOR-VERIFY]` items enumerated in PROGRESS.md for the user.

## Work items

1. **Debug overlay** (extend the migrated CkJoltDebugger — never a new renderer):
   `ck.Jolt.DebugDraw.Enabled` + `ck.Jolt.DebugDraw.SleepColoring` CVars (UCk_Utils_CVar_UE);
   sleep coloring via `JPH::BodyManager::DrawSettings` color mode switch (MotionTypeColor ↔ SleepColor);
   distinct color scheme vs Chaos `show Collision`; draws static AND dynamic.
   The old probe-only gate (`ck.SpatialQuery.PreviewAllProbesUsingJolt`) keeps working (bridge gate ORs).
   Gym: CkJoltGym_DebugDrawOverlayStation `[EDITOR-VERIFY]`.
2. **Geometry-parity sampler**: Test_Jolt_GeometryParitySampler.spec.cpp — seeded FRandomStream,
   1024 rays + 256 sweeps (box/sphere/capsule) + 256 overlaps against the Phase-1 parity content;
   Chaos LineTraceSingleByChannel/SweepSingleByChannel vs Get_RayCast/Get_ShapeCast; hit/miss exact,
   impact ≤ 1cm, normal dot ≥ 0.95; per-primitive-type mismatch counters; assert total == 0.
3. **Dynamic-parity Chaos twins** (AS): ChaosBoxStackSettles, ChaosSphereRampRoll,
   ChaosKinematicPlatformCarry (InterpTo/tick-moved kinematic mesh), ChaosCcdProjectileStopsAtThinWall
   (FBodyInstance bUseCCD) — same asserts as the Jolt versions (qualitative equivalence).
4. **Benchmark harness**: Test_JoltBody_Benchmark.cpp (non-gating; logs ready-to-paste markdown):
   N ∈ {500, 2000, 10000} dynamic boxes batch-spawned; 60-frame warmup + 600 measured;
   STAT_CkJolt_WorldStep / _Writeback / _KinematicPush (new stats) p50/p95/max; Chaos side = N
   simulate-physics AStaticMeshActors, frame-delta vs empty-control run; island variants
   (spread / 1 pile / 10×1k at threads 1 vs cores-1). Results + island verdict → VALIDATION.md.
   Gauntlet process-level fallback documented if PIE timing too noisy.
5. **Docs wrap**: CkJolt/Claude.md (phase-order diagram, determinism section, tunable-knob list);
   tier-table final check; PROGRESS.md final entry + full `[EDITOR-VERIFY]` list; VALIDATION.md complete.

## Definition of done (campaign)

All six PROMPT.md success criteria observed and evidenced in VALIDATION.md.
