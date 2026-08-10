# One-line summary

Restore the Crowd Debugger's Unreal Recast navmesh and PathNetwork sidewalk rendering inside the new 3D viewport without regressing VoxelNav, agent picking, camera behavior, the black background, or PIE-safe teardown.

# Repo state

- Workspace: `E:\Repos\CkPlugins_Other`
- Root branch: `feature/3d-navigation`, HEAD `cc5a5ff907536438908027bb6ca3e4745dab8b4e`, tracking `origin/feature/3d-navigation` at parity. PR: https://github.com/chainkemists/CkPlugins/pull/10 against `dev`.
- `Plugins/CkGameplayDebugger`: branch `feature/voxelnav-debugger`, HEAD `77aff95773dc62bf37629e58e954989e36e716a2`, tracking its remote feature branch at parity; the same tip is on that repository's `origin/dev`.
- Relevant debugger commits:
  - `d2a71bdf70dad6fd56f4870b32c59cf24e8ff781` added the VoxelNav 3D viewport and replaced the legacy center viewport widget.
  - `77aff95773dc62bf37629e58e954989e36e716a2` fixed the black preview, live agents, direct selection, orthographic presets, camera speed, icons, and editor shutdown lifetime.
- `Plugins/CkTests`: clean at `f705c46c98523a4164e91d6655922a9cc4298fe3`, branch `feature/ckvoxelnav-port`, remote parity.
- `Plugins/CkFoundation`: HEAD `41de55d5deb76478e7216b827cd673bd11e55049`, branch `feature/ckvoxelnav-port`, remote parity, but contains many unrelated modified `Content/CkUsf/GeneratedLooks/*.uasset` files. Preserve them exactly. Do not stage, stash, reset, restore, re-save, or otherwise touch them.
- Root also has unrelated untracked `.agents/`, `.codex/`, `AGENTS.md`, and `CONTINUATION_RelayClientRegistration/`. Preserve and exclude them.
- No fix for this regression is currently in flight. This handoff file is the only new root file created for the continuation.

# Active bugs or open questions

User report: "with the 3d navigation, the crowd debugger no longer shows the sidewalks and unreal navmesh that it used to before."

Required outcome:

- The Crowd Debugger's new 3D viewport must show Unreal Recast navmesh geometry and authored PathNetwork sidewalk ribbons again.
- It must be possible to see these together with VoxelNav cells/volumes and crowd agents, not by reverting to the old 2D-only viewport.
- Preserve the current grouped toolbar, black background, perspective and true orthographic presets, mouse-wheel camera speed adjustment, direct agent selection, native icons, and crash-free editor shutdown.
- Determine the legacy availability boundary from code and runtime evidence. The old collector is selected-world driven and historically useful in PIE. Restore that parity first. Do not promise outside-PIE Recast/sidewalk data unless the selected editor world can supply it safely and accurately.
- Clarify through the existing UI whether `Unreal Nav Projection` means the game-world overlay, the debugger viewport layer, or both. Avoid a checked control that has no effect in the debugger viewport.

# Why prior fixes or investigations were insufficient

- Commit `d2a71bd` changed `SCkCrowdDebuggerWindow::_ViewportPanel` from `SCkCrowdDebugger_ViewportPanel` to `SCkCrowdDebugger_3dViewport`.
- The old `SCkCrowdDebugger_ViewportPanel` still contains the rendering implementation for Recast triangles and PathNetwork ribbons, but it is no longer instantiated by the window.
- `FCkCrowdDebugger_DataCollector` still gathers both datasets: `_NavTriVerts` from `ARecastNavMesh::GetDebugGeometryForTile` and value-only `FCkCrowdDebugger_PathNetworkRibbonSnapshot` objects from `ACk_PathNetwork_UE::Get_WorldRibbons()`.
- `FCkCrowdDebugger_ViewModel` still exposes `Get_NavTriVerts()` and `Get_PathNetworkRibbons()`.
- The current window tick sends only agents via `Set_AgentSnapshots()` and VoxelNav via `Set_VoxelNavSnapshot()`. The new 3D viewport has no API or storage for Recast triangles or sidewalk ribbons and therefore cannot draw them.
- The toolbar's `ck.Crowd.DrawNavProjection` checkbox controls an existing game-world diagnostic CVar; it does not publish the collected Recast triangle soup to the isolated preview viewport.
- Commit `77aff957` deliberately focused on viewport stability and interaction. It did not restore these two orphaned legacy render layers.

# Available diagnostics and the first evidence to collect

The leading hypothesis is a disconnected presentation path, not missing navigation data.

First, reproduce in PIE with the `Path Network` gym and the Crowd Debugger open. Before changing rendering, prove these three values in the selected PIE world:

1. `FCkCrowdDebugger_ViewModel::Get_NavTriVerts().Num()` is non-zero.
2. `FCkCrowdDebugger_ViewModel::Get_PathNetworkRibbons().Num()` is non-zero and at least one ribbon has two or more points.
3. `SCkCrowdDebuggerWindow::Tick()` updates the 3D viewport with agents/VoxelNav but has no call that transfers either legacy dataset.

Use a breakpoint, debugger watch, or short default-off diagnostic. Do not tune navigation generation or rebuild Recast until those values are known. The Navmesh Status panel and `Run Health Check` are also available to distinguish absent Recast data from an orphaned renderer.

The last verified gate log is `E:\Repos\CkPlugins_Other\Saved\Logs\BuildTest-PostRebase-CrowdDebugger.log`: generated project files, Development Editor build success, and `Ck.VoxelNav.DebugSnapshot` 7/7 with no ensure, script, compiler, linker, or test failures.

# Likely symptom-to-cause map

| Symptom | Likely cause | Evidence / files |
|---|---|---|
| VoxelNav cells and agents render, but Unreal navmesh does not | The new viewport receives only VoxelNav and agent snapshots | `SCkCrowdDebuggerWindow.cpp`, `SCkCrowdDebugger_3dViewport.cpp` |
| Sidewalk ribbons disappeared at the same commit | The old 2D panel was the only consumer of `Get_PathNetworkRibbons()` | `SCkCrowdDebugger_ViewportPanel.cpp`, commit `d2a71bd` |
| `Unreal Nav Projection` is checked but the debugger viewport stays unchanged | The checkbox drives `ck.Crowd.DrawNavProjection`, a game-world overlay CVar, not a 3D viewport layer | `SCkCrowdDebuggerWindow.cpp` toolbar code |
| Navmesh status says Recast is healthy but no green geometry appears | Collection/status still works; presentation is disconnected | `CkCrowdDebugger_DataCollector.cpp`, `SCkCrowdDebugger_NavmeshStatusPanel.cpp` |
| Data arrays are empty in PIE | Wrong selected world, no default `ARecastNavMesh`, navmesh not built, or no `ACk_PathNetwork_UE` in that gym | world selector, data collector lines around nav/ribbon collection |
| Frame All ignores sidewalks/navmesh after rendering is restored | New viewport bounds currently include only VoxelNav and agents | `SCkCrowdDebugger_3dViewport.cpp::Get_AllFrameBounds()` |
| Fix works but becomes expensive on large maps | Large triangle/ribbon arrays are copied or rebuilt every frame | collector's one-second nav pull throttle and the new publication API |

# Critical files

1. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.cpp` - creates the new viewport, ticks the view model, publishes agents/VoxelNav, and owns the navigation toolbar.
2. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Window/SCkCrowdDebuggerWindow.h` - viewport ownership and retained VoxelNav state.
3. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.cpp` - current PDI rendering, camera presets, agent picking, frame bounds, and snapshot storage.
4. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h` - value-only publication contract; explicitly must not retain a `UWorld`.
5. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Window/SCkCrowdDebugger_ViewportPanel.cpp` - orphaned legacy renderer; Recast triangle rendering begins near the `Navmesh walkable triangles` comment and sidewalk rendering uses `Get_PathNetworkRibbons()` near the `PathNetworkOpacity` block.
6. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Window/SCkCrowdDebugger_ViewportPanel.h` - legacy pan/zoom/click and cached transform behavior; use as reference, not as the desired final viewport.
7. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.cpp` - throttled Recast geometry collection and value-only PathNetwork ribbon collection.
8. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/Data/CkCrowdDebugger_Types.h` - navmesh status and `FCkCrowdDebugger_PathNetworkRibbonSnapshot` definitions.
9. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/Public/CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h` - existing getters for nav triangles and sidewalk ribbons.
10. `Plugins/CkGameplayDebugger/Source/CkCrowdDebugger/CkCrowdDebugger.Build.cs` - module dependencies if the 3D rendering implementation needs an existing editor/rendering module.

# Things ruled out

- The legacy collectors were not deleted. Both Recast triangle and PathNetwork ribbon collection code remain current.
- The legacy viewport renderer was not deleted. `SCkCrowdDebugger_ViewportPanel` remains in the module, but the main window no longer creates it.
- The reported regression is not evidence that PathNetwork removed or corrupted Unreal navmesh. PathNetwork reads Recast and can reject routes independently; keep navigation availability separate from debugger presentation.
- VoxelNav snapshot construction itself is not the likely cause. Its focused 7-test suite and the editor build passed after the latest debugger changes.
- Reverting the entire 3D viewport is not acceptable because it would discard user-requested 3D rotation, orthographic presets, voxel cells, agent rendering/picking, black background, and teardown fixes.

# Architecture notes and gotchas

- Preserve the isolated preview design: publish copied/value-only render data to `SCkCrowdDebugger_3dViewport`; never retain `UWorld`, actors, ECS handles, registries, or octree references across PIE teardown.
- Prefer one explicit legacy-navigation render snapshot/API containing Recast triangles, ribbon points/half-widths, validity/generation state, and bounds. Clear it immediately when the selected world is invalid or changes.
- Do not blindly copy a large all-tiles triangle soup every 250 ms or every Slate tick. Reuse the collector's throttling/generation semantics or publish only when the value data changes. Measure or at least inspect allocation/copy behavior on a non-trivial map.
- Preserve the old visual meaning where practical: translucent/outlined green walkable Recast geometry and cyan sidewalk ribbons with visible widths. In perspective and every orthographic preset, the layers must be spatially correct in world coordinates.
- Include navmesh/ribbon bounds in `Frame All`. `Frame Selection` must remain agent-specific.
- Decide layer controls deliberately. The existing world-overlay CVar should not silently masquerade as an internal viewport toggle. A grouped `Unreal Navmesh` viewport toggle and a `Sidewalks` toggle/opacity under Navigation may be clearer, while retaining any existing world-overlay control with an explicit label.
- Rendering must remain deterministic on the black background and must not reintroduce the Epic advanced-preview scene or eye-adaptation flicker.
- Preserve direct plain-left-click agent picking and RMB plus mouse-wheel camera speed changes.
- Preserve the `FCoreDelegates::OnEnginePreExit` viewport release path and validate closing the editor with the debugger open.
- Read the plugin skills `ck-gameplaydebugger-extension` and `ck-slate-tools` before editing.

# Concrete diagnostic and verification flow

1. Read this file, repository `AGENTS.md`, and the two debugger/Slate skills. Inspect all dirty paths before editing and explicitly exclude unrelated root and CkFoundation dirt.
2. Reproduce the regression in PIE using the `Path Network` gym. Select the PIE world in the Crowd Debugger and capture nav triangle/ribbon counts plus the visible layers.
3. Compare `d2a71bd^` with `d2a71bd`, especially the `_ViewportPanel` type replacement. Confirm the collector arrays are populated and the missing handoff to the new viewport is the first broken edge.
4. Define a value-only, teardown-safe navigation render snapshot/publication API. Review copy/allocation cost before implementation.
5. Add Recast and sidewalk drawing to `FCkCrowdDebugger_3dViewportClient::Draw()`, include their extents in `Frame All`, and clear stale data on world loss/change.
6. Make the grouped Navigation controls unambiguous and independently capable of hiding/showing VoxelNav, Unreal navmesh, and sidewalks. Preserve the existing sidewalk opacity setting or migrate it compatibly.
7. Add focused non-visual coverage for snapshot transfer, clearing, bounds, visibility flags, and any caps/generation behavior that can be tested without a GPU viewport. Do not use tests as a substitute for visual PIE acceptance.
8. Close Unreal Editor before building. Use only `CkAuto/UnrealToolbox.exe` with `--project=E:\Repos\CkPlugins_Other`, launched detached through PowerShell with `Start-Process -WindowStyle Hidden`; never pass `--no-progress-window`. Include `--build --generate --config=Auto --target=Editor`, the relevant focused tests, and a unique `--output=Saved\Logs\...` path. Monitor the detached process and inspect the final log for build/test summaries, ensures, and script errors.
9. Manually verify in the `Path Network` gym: agents, VoxelNav, Unreal navmesh, and sidewalks can appear simultaneously; each intended toggle works; sidewalks retain width; `Frame All` includes all enabled navigation layers; perspective and every orthographic view are correct.
10. Recheck the earlier acceptance surface: consistent black background, no missing-font symbols, direct agent selection, RMB plus wheel speed adjustment, and editor exit with the debugger open.
11. Report verified versus manual-only evidence. Do not commit or publish unless the user asks in the new conversation.

Suggested detached gate shape:

```powershell
$toolboxArgs = @(
    '--build',
    '--generate',
    '--config=Auto',
    '--target=Editor',
    '--test',
    '--test-pattern=Ck.VoxelNav.DebugSnapshot',
    '--project=E:\Repos\CkPlugins_Other',
    '--output=Saved\Logs\BuildTest-CrowdDebugger-RestoreLegacyNavigation.log'
)
$toolbox = Start-Process -FilePath '.\CkAuto\UnrealToolbox.exe' -ArgumentList $toolboxArgs -PassThru -WindowStyle Hidden
```

# Suggested first message

I have read the continuation prompt and inspected the current root and CkGameplayDebugger state. I will first reproduce the regression in the Path Network gym and prove whether the existing Recast triangle and sidewalk ribbon snapshots are populated but no longer published to the new 3D viewport; I will preserve all unrelated dirty assets and will not change navigation behavior until that presentation boundary is confirmed.
