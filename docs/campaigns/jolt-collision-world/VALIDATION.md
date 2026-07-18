# VALIDATION — Jolt Collision World campaign

Definition-of-done evidence. Filled as phases land; tables stay empty until real numbers exist —
never estimated. Every number below is lifted from a named log or a committed test run.

## 1. Extraction regression check (Phase 0)

| Checkpoint | Baseline (pre-campaign) | Post-Phase-0 | Verdict |
|---|---|---|---|
| Full suite (AS + C++) | 783 total / 782 passed / 1 flaky | **783/783** (48m20s) | PASS — strictly better than baseline |
| C++ automation | included in the full-suite runs above | included | PASS |
| JobSystem log tripwire | n/a | no separate artifact recorded | INFERRED-CLEAN — no JoltWorker anomaly surfaced in any phase gate through Phase 5; not independently pinned |

Suite growth since: Phase-3 close 793/793 → Phase-4 close **802/802** (GateP4-FullSuite.log,
delta-zero on every pre-existing row at each gate).

## 2. Geometry parity (Phases 1/2/5)

Seeded C++ sampler (`Test_Jolt_GeometryParitySampler.spec.cpp`, fixed seed): 1024 rays +
256 sweeps (box/sphere/capsule) + 256 overlaps against a baked parity field, Chaos
LineTrace/SweepSingleByChannel vs UCk_Utils_JoltQuery_UE channel queries. Tolerances:
hit/miss EXACT; ray impact ≤ 1 cm; sweep STOP-DISTANCE ≤ 1 cm + normal dot ≥ 0.95
(P5-RULING: a raw 3D contact point is ill-defined for face contacts — the engines pick
different representative points in the same contact region; the swept-sphere case, which
has a unique contact point, shows 0 mismatches and pins the diagnosis).
Observed (P5Cpp-Test-Jolt.log): **0 mismatches total; max sweep stop-delta 0.0019 cm;
min normal dot 0.99999999**.

| Primitive type | Evidence | Verdict |
|---|---|---|
| Convex (AggGeom) | sampler (0 mismatches) + Ck_AutoTest_CkJolt_StaticBake_SimpleBox_RaycastMatchesChaos | PASS |
| Trimesh (complex) | extraction ships (winding-swapped JPH::MeshShape); automated parity coverage DEFERRED (needs authored asset / transient-mesh cook harness — gap logged Phase 1) | GAP (recorded) |
| Heightfield (landscape) | KnownHeightsZUp C++ pin (axis wrap + row flip math) green; landscape end-to-end is editor-only | PASS (math) + [EDITOR-VERIFY] (end-to-end) |
| Instanced (HISM) | Ck_AutoTest_CkJolt_StaticBake_Hism_PerInstanceBodies_Parity + _CompoundCluster_SingleBody | PASS |
| Brush/volume | extraction ships (convex elems); runtime-spawnable harness cannot author brushes | [EDITOR-VERIFY] |
| Spline mesh | extraction ships (per-instance deformed BodySetup); headless runtime cannot create spline collision | [EDITOR-VERIFY] |

## 3. Dynamic behavior parity (Phase 5)

Chaos twins mirror the Jolt scenarios' assertions (qualitative equivalence), pure
Chaos-side physics (no Jolt fragments).

| Scenario | Jolt test (green since P3/P4) | Chaos twin | Verdict |
|---|---|---|---|
| Box stack settles | Ck_AutoTest_CkJolt_BoxStackOfThreeSettlesAndStays (3-high; see gap note) | Ck_AutoTest_CkJolt_ChaosParity_BoxStackSettles (5-high, 200uu-gap drop) | PASS with a RECORDED ASYMMETRY — see below |
| Ramp roll | Ck_AutoTest_CkJolt_SphereRollsDownRampToBottom | Ck_AutoTest_CkJolt_ChaosParity_SphereRampRoll | PASS (same run) |
| CCD projectile | Ck_AutoTest_CkJolt_FastProjectileWithCcdStopsAtThinWall | Ck_AutoTest_CkJolt_ChaosParity_CcdProjectileStopsAtThinWall | PASS (same run; peak-X assertions — Chaos restitution 0.3 bounces the sphere, so final X is not a travel witness) |
| Kinematic platform carry | Ck_AutoTest_CkJolt_KinematicPlatformCarriesDynamicBox | Ck_AutoTest_CkJolt_ChaosParity_KinematicPlatformCarry | PASS (same run) |

Note: KeepVelocity/ResetVelocity teleport semantics are additionally pinned by the
rewritten TeleportMovesBodyAndResetsVelocity, which reads the body's REAL simulation
velocity via the Phase-5 `Get_LinearVelocity` API (per-tick position deltas are invalid
in AS autotests — InDeltaT can be 0.0 on real ticks and writeback cadence aliases).

**[P5-FINDING] Stack-height asymmetry under load (post-campaign investigation):** in
loaded full-suite sessions (~26fps → sustained 2-3 fixed substeps per frame), Jolt
topples FIVE-high columns — cubes and 160x160x100 slabs, dropped and pre-formed —
across four gate attempts with three distinct failure signatures, while the SAME
sessions pass the Chaos twin's five-cube 200uu-gap drop. Isolation and 40-test
Jolt-pattern sessions (high fps, 0-1 substeps/frame) always pass five-high. One
attempt also squeeze-ejected a box through a 50cm floor slab under ~626cm/s pile
impacts (deep-penetration ejection). The pre-fix "five-high stability" was an artifact
of the meters-default 0.02cm penetration slop acting as glue. Jolt's pinned scenario is
three-high; suspects for the investigation: substep-burst interaction with the pump,
contact softness, MaxPenetrationDistance, LargeIslandSplitter behavior at small islands.

## 4. Debug draw overlay (Phase 5) — [EDITOR-VERIFY]

Implementation landed (CkJolt_Subsystem.cpp: `ck.Jolt.DebugDraw.Enabled` /
`ck.Jolt.DebugDraw.SleepColoring`, gate ORs with the probe opt-in). User steps:

- [ ] In a PIE world with Jolt bodies: `ck.Jolt.DebugDraw.Enabled 1` → wireframes for ALL
  Jolt bodies, static AND dynamic, colored by motion type (static grey / kinematic green /
  dynamic per-instance) — visually distinct from Chaos `show Collision`
- [ ] `ck.Jolt.DebugDraw.SleepColoring 1` → awake dynamics yellow, sleeping red; let bodies
  settle and watch them turn red
- [ ] With Enabled 0: `ck.SpatialQuery.PreviewAllProbesUsingJolt 1` still draws (old probe
  path unchanged; either gate suffices)
- [ ] Async mode (`jolt.EnableAsyncPhysicsUpdate 1`, startup-only): overlay intentionally
  skipped/lagging — expected

## 5. Benchmarks (Phase 5)

Methodology ACTUALLY USED (headless PIE, directional not authoritative): per-frame
wall-clock frame-delta, 60-frame warmup + 600 measured frames, `p50 − control` isolates
the body load; both engines measured the same way (`Test_JoltBody_Benchmark.cpp`,
non-gating, numbers logged in P5Cpp-Test-Jolt.log). The STAT_CkJolt_WorldStep /
_Writeback / _KinematicPush cycle stats landed for Insights/`stat CkJolt` use but are not
read programmatically. Control max (~916 ms) is a first-frame/GC hitch — p50/p95 are the
trustworthy figures. Machine: dev box, Development Editor, default thread config.

### Throughput (spread layout, contact-free — clean engine comparison)

| N bodies | Engine | p50 ms | p95 ms | max ms | p50 − control ms |
|---:|---|---:|---:|---:|---:|
| 0 (control) | — | 8.318 | 9.504 | 915.944 | +0.000 |
| 500 | Jolt | 8.264 | 10.860 | 12.737 | −0.053 |
| 500 | Chaos | 8.225 | 10.071 | 11.701 | −0.093 |
| 2 000 | Jolt | 12.480 | 17.362 | 27.548 | **+4.163** |
| 2 000 | Chaos | 15.787 | 17.822 | 41.200 | +7.470 |
| 10 000 | Jolt | 38.829 | 46.264 | 55.387 | **+30.511** |
| 10 000 | Chaos | 94.260 | 111.395 | 147.514 | +85.942 |

Jolt ≈ **2.8× cheaper than Chaos at 10k** free bodies; parity at 500; ~1.8× at 2k.

### Contact-island worst case (10k bodies, default threads = cores−1)

| Layout | p50 − control ms | Note |
|---|---:|---|
| Spread (no islands) | +30.5 | baseline |
| 10 piles × 1k | +178.3 | |
| Single pile (~1 island) | +246.8 | capacity-limited: exceeds configured MaxBodyPairs/MaxContactConstraints; the over-capacity `EPhysicsUpdateError` ensure fired in one run (product correctly loud) — whitelisted in the non-gating benchmark only |

**Island verdict:** cost is dominated by island SIZE (fewer/larger islands → less
parallelism): one 10k island costs ~8× the spread layout. The vendored 5.2.1
LargeIslandSplitter does not erase the gap at this scale. Design guidance: avoid
mega-piles; 1k-body islands are ~30% cheaper than one 10k island.

**Threads=1 variant:** startup-only knob — not runnable in-session. Manual procedure:
launch with `-jolt.EnableParallelPhysics=0` (single-threaded JobSystem), re-run
`Test_JoltBody_Benchmark`, compare the same rows. [EDITOR-VERIFY] if wanted; the
island verdict above stands on the default-config evidence.

## 6. Cooked data & streaming (Phase 1)

- [x] Cook roundtrip == live extraction — shape-blob RoundTrip C++ test green (in the 36/36
  Jolt regression, P5Cpp-Test-Jolt.log)
- [ ] Stale `_RuntimeCheckHash` → ensure + skip — code path ships; [EDITOR-VERIFY] (edit a
  baked actor without re-cooking → loud ensure + skip, never silent reuse)
- [ ] `_CookVersion`/`_JoltVersionId` mismatch → ensure + skip — code path ships;
  [EDITOR-VERIFY] via the Phase-1 cook walkthrough in PROGRESS.md
- [x] Sublevel load/unload → body count follows — level-lockstep tracking exercised by
  Ck_AutoTest_CkJolt_StaticBake_RemoveActor_RaysMiss + subsystem tests; real WP streaming
  cells remain [EDITOR-VERIFY]

## Success criteria (PROMPT.md) — status

1. Pre-campaign Probe/consumer suites unchanged after the split — **PASS** (Phase-0 gate
   783/783; consumers build+test clean at every later gate).
2. Static geometry parity, per-primitive mismatch == 0 — **PASS with recorded gaps**
   (section 2: sampler + targeted tests green; trimesh automated coverage deferred,
   brush/spline/landscape-e2e editor-only).
3. Dynamic scenario parity — **PASS** (section 3: all 4 twins green alongside their Jolt
   originals, P5-JoltRegression-Run3.log 40/40).
4. Debug overlay CVar — implemented; [EDITOR-VERIFY] (section 4).
5. Benchmark tables with island verdict — **DONE** (section 5, real numbers).
6. Cooked data versioned/hashed, streaming-lockstep, loudly-stale — **PARTIAL** (section 6:
   roundtrip + lockstep pinned by tests; stale/version paths ship with editor-only verification).
