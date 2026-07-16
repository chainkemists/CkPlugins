# VALIDATION — Jolt Collision World campaign

Definition-of-done evidence. Filled as phases land; tables stay empty until real numbers exist —
never estimated.

## 1. Extraction regression check (Phase 0)

| Checkpoint | Baseline (pre-campaign) | Post-Phase-0 | Verdict |
|---|---|---|---|
| Full AS suite | (pending) | (pending) | — |
| C++ automation | (pending) | (pending) | — |
| JobSystem log tripwire | n/a | (pending) | — |

## 2. Geometry parity (Phase 1/2/5)

Seeded sampler: 1024 rays + 256 sweeps + 256 overlaps per test level; tolerance: hit/miss exact,
impact ≤ 1 cm, normal dot ≥ 0.95 (0.999 for parity-map primitives).

| Primitive type | Samples | Mismatches | Verdict |
|---|---|---|---|
| Convex (AggGeom) | — | — | — |
| Trimesh (complex) | — | — | — |
| Heightfield (landscape) | — | — | — |
| Instanced (HISM) | — | — | — |
| Brush/volume | — | — | — |
| Spline mesh | — | — | — |

## 3. Dynamic behavior parity (Phase 5)

| Scenario | Jolt test | Chaos twin | Verdict |
|---|---|---|---|
| Box stack settles | CkAutoTest_JoltBody_BoxStackOfFiveSettlesAndStays | CkAutoTest_JoltParity_ChaosBoxStackSettles | — |
| Ramp roll | CkAutoTest_JoltBody_SphereRollsDownRampToBottom | CkAutoTest_JoltParity_ChaosSphereRampRoll | — |
| CCD projectile | CkAutoTest_JoltBody_FastProjectileWithCcdStopsAtThinWall | CkAutoTest_JoltParity_ChaosCcdProjectileStopsAtThinWall | — |
| Kinematic platform carry | CkAutoTest_JoltBody_KinematicPlatformCarriesDynamicBox | CkAutoTest_JoltParity_ChaosKinematicPlatformCarry | — |

## 4. Debug draw overlay (Phase 5) — [EDITOR-VERIFY]

- [ ] `ck.Jolt.DebugDraw.Enabled 1` draws Jolt wireframe (distinct color from Chaos `show Collision`)
- [ ] Static AND dynamic bodies drawn
- [ ] `ck.Jolt.DebugDraw.SleepColoring 1` shows sleeping bodies in the sleep color

## 5. Benchmarks (Phase 5)

Methodology: 60-frame warmup, 600 measured frames; Jolt = STAT_CkJolt_WorldStep + Writeback +
KinematicPush; Chaos = frame-delta vs empty-control run of the same map. Non-gating.

### Throughput

| N bodies | Jolt step ms (p50/p95/max) | Jolt sync ms | Chaos frame-delta ms | Machine |
|---|---|---|---|---|
| 500 | — | — | — | — |
| 2 000 | — | — | — | — |
| 10 000 | — | — | — | — |

### Contact-island worst case (10k bodies)

| Layout | Threads=1 | Threads=cores-1 | Scaling verdict |
|---|---|---|---|
| Spread grid (10k islands) | — | — | — |
| Single pile (~1 island) | — | — | — |
| 10 piles × 1k | — | — | — |

LargeIslandSplitter present in vendored 5.2.1 — benchmark decides whether it suffices.

## 6. Cooked data & streaming (Phase 1)

- [ ] Cook roundtrip == live extraction (counts, transforms, shape types, dedup count)
- [ ] Stale `_RuntimeCheckHash` → ensure + skip (test green)
- [ ] `_CookVersion`/`_JoltVersionId` mismatch → ensure + skip (test green)
- [ ] Sublevel load/unload → body count follows (test green)
