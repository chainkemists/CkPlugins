# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

CkPlugins is an Unreal Engine 5.5 project that serves as the **development host for the Chainkemists plugin ecosystem**. The host project itself is intentionally minimal — a default `GameModeBase` and a near-empty `Source/CkPlugins/` module. The real content is the plugin submodules under `Plugins/`, which are developed, tested, and iterated on inside this clean project before being consumed by downstream game projects.

Use this project when you need to work on a Chainkemists plugin in isolation: you get a full UE project to compile against, the AngelScript runtime via CkFoundation, and the CkTests harness — without the weight of a full game project on top.

## Framework Development Guides

Before writing code against CkFoundation, read the appropriate guide in the submodule:

- **AngelScript (.as):** [Plugins/CkFoundation/Script/AGENTS.md](Plugins/CkFoundation/Script/AGENTS.md) — language differences from C++, `utils_*` shortcuts, entity script lifecycle, asset definitions, dynamic handle registration gotcha.
- **C++ framework patterns:** [Plugins/CkFoundation/Source/AGENTS.md](Plugins/CkFoundation/Source/AGENTS.md) — full development guidelines: function formatting, ECS patterns, `CK_PROPERTY`, request structs, component lifetimes, module tier table.
- **C++ quick reference:** [Plugins/CkFoundation/AGENTS.md](Plugins/CkFoundation/AGENTS.md) — condensed architecture overview (macros, fragments, processors, naming).

## Per-plugin versioning

When changes touch `Plugins/GitLink/Source/`, follow the bump rule in [Plugins/GitLink/AGENTS.md](Plugins/GitLink/AGENTS.md)'s **Versioning** section at end of session:

1. Bump `GITLINK_VERSION` in `Plugins/GitLink/Source/GitLink/Public/GitLink/GitLink_Version.h`.
2. Bump `VersionName` (and `Version`) in `Plugins/GitLink/GitLink.uplugin` to match.
3. Add a row at the top of the **Version log** table in `Plugins/GitLink/AGENTS.md`.
4. Rebuild so the binary's build timestamp refreshes.

Other plugins under `Plugins/` may adopt the same pattern over time; check their `AGENTS.md` for an analogous section before assuming there isn't one.

## Repository Structure

- `/Source/CkPlugins/` — Minimal host module (`GameModeBase` only). You will rarely edit this.
- `/Plugins/` — The Chainkemists plugin submodules and a few third-party dev tools. See **Plugin Ecosystem** below.
- `/CkAuto/` — Shared developer scripts (build, run, submodule management) — itself a submodule.
- `/Config/` — UE project config (`DefaultEngine.ini`, `DefaultGame.ini`, etc.). Mostly stock.
- `/Content/` — Minimal — host project has almost no Blueprint or asset content. Real assets live inside each plugin's `Plugins/<Name>/Content/`.
- `/Script/` — Empty by design. AngelScript content for the plugin ecosystem lives in `Plugins/CkFoundation/Script/` and other plugins' `Script/` folders.
- `/.runreal/` — Build pipeline configuration using the runreal build system.

## Build System and Development Commands

The project uses the **runreal** build system. The engine source comes from `https://github.com/Chainkemists/UnrealEngine-Internal` (configured in `runreal.config.json`).

**Build + run automation/Gauntlet tests via the Unreal Toolbox** — use the
`/build-test` skill (canonical doc: [CkAuto/.Codex/skills/build-test/SKILL.md](CkAuto/.Codex/skills/build-test/SKILL.md);
the project-root `.Codex/skills/build-test/` is a thin wrapper over it, needed
because skills inside submodules aren't auto-discovered). Never invoke
`Build.bat`, UnrealBuildTool, or `UnrealEditor-Cmd.exe -ExecCmds="Automation ..."`
directly for build/test automation — the toolbox owns engine resolution, the
machine-wide build lock, watchdogs, and structured results.

### Setup and building

```bash
# Install and setup Unreal Engine
runreal engine install
runreal engine update --setup

# Build the editor (runs the "Build Editor" workflow from runreal.config.json)
runreal build editor
```

Configurations available: Development, Test, Shipping, Debug.

#### Building the editor directly (Build.bat)

For editor-only iteration without going through runreal, invoke the engine's `Build.bat` directly. The engine path is resolved dynamically from this project's `.uproject` `EngineAssociation` GUID via a tiny helper — do not hardcode it:

```powershell
$engine = & "$env:CLAUDE_PROJECT_DIR\CkAuto\Get-ProjectEnginePath.ps1"
& "$engine\Engine\Build\BatchFiles\Build.bat" CkPluginsEditor Win64 Development `
    -Project="$env:CLAUDE_PROJECT_DIR\CkPlugins.uproject" -WaitMutex -FromMsBuild
```

The same `PreToolUse` hook that guards git ops (see *Hooks / safety guards*) also blocks `Build.bat` invocations whenever UnrealEditor is running for this project — building while the editor has DLLs loaded corrupts hot-reload state. Close the editor first, or set `SKIP_UNREAL_GUARD=1` if you know what you're doing.

### Running

```bash
# Run with basic logging
CkAuto/CkRun_LogOnly.bat

# Run with full tracing (network, CPU, frames)
CkAuto/CkRun_TraceAll.bat
```

## Submodule Management

This project uses Git submodules heavily — every plugin under `Plugins/` is a submodule, as is `CkAuto/`. The shared scripts in `CkAuto/` provide bulk operations:

```bash
# Initialize all submodules (after a fresh clone)
git submodule update --init --recursive

# Update all submodules to latest dev branch
CkAuto/UpdateAllSubmodules_PULL_DEV_ToLatest.bat

# Update all submodules to latest main branch
CkAuto/UpdateAllSubmodules_PULL_MAIN_ToLatest.bat

# Push changes to dev branch
CkAuto/UpdateAllSubmodules_PUSH_DEV.bat

# Run a custom command across every submodule
CkAuto/SubmodulesCustomCommand.bat "git status"
```

## Plugin Ecosystem

### Chainkemists plugins (the reason this project exists)

- **CkFoundation** — ECS framework using EnTT 3.15.0; the foundation everything else builds on.
- **CkApplication** — Application-level functionality on top of CkFoundation.
- **CkGameplayDebugger** — Debug tools integration with UE's gameplay debugger.
- **CkTests** — Test harness (AutoTests + Gym framework). See its own AGENTS.md and the `Script/Common/` specifications for authoring tests and gyms against CkFoundation features.
- **GitLink** — Source-control / git integration plugin.

### Third-party dev tools

- **AutoSizeComments** — Comment node enhancements in the Blueprint editor.
- **BlueprintAssist** — Blueprint editing enhancements.
- **NodeGraphAssistant** — Node graph editing utilities.
- **GitSourceControl** (chainkemists fork of UEGitPlugin) — Git source control provider for the editor.
- **ZenMode** — Editor focus / zen mode.

## CkFoundation Architecture Notes

When working in the plugin ecosystem (most edits in this project), the patterns to know:

- **ECS-first.** CkFoundation provides an EnTT-backed entity-component-system; gameplay logic is data-oriented, with processors operating over fragment groups rather than UObject methods. Read the framework guides linked above before adding new features.
- **EntityBridge** components connect Unreal `AActor`s to ECS entities when interop is needed.
- **AngelScript** is the primary scripting language for plugin content (`.as` files). Entity Scripts and Entity Construction Scripts express data-driven entity behaviour.
- **Asset definitions** (data-driven config) are preferred over Blueprint subclassing for new gameplay systems.

## Working in CkFoundation submodules

Most work in this project is *inside* a plugin submodule (e.g. editing `Plugins/CkFoundation/Source/...`). When you do this:

1. The change lives in the submodule's git history, not CkPlugins's.
2. Commit and push from inside the submodule (`cd Plugins/CkFoundation && git commit && git push`).
3. Then bump CkPlugins's pointer to the new submodule SHA: `cd <project root> && git add Plugins/CkFoundation && git commit -m "chore(submodule): bump CkFoundation"`.
4. The same submodule may also need pointer-bumping in any downstream consumer that uses it.

The `CkAuto/UpdateAllSubmodules_PUSH_DEV.bat` helper can automate steps 2–3 across all submodules in one pass.

## Hooks / safety guards

`.Codex/settings.json` registers a `PreToolUse` hook (`CkAuto/Check-UnrealNotRunning.ps1`) that intercepts file-mutating git commands (`checkout`, `switch`, `rebase`, `merge`, `reset`, `pull`, `clean`, `restore`, `cherry-pick`, `revert`, `stash pop/apply`) and engine `Build.bat` invocations. Behaviour:

- **Editor closed for this project** → silent pass.
- **Editor open, op only touches source/config** → soft-warn prompt (`permissionDecision: "ask"`); user confirms or declines.
- **Editor open, op touches engine-locked paths** (`.uasset`/`.umap`/`Content/`/`Binaries/`/`Saved/`/`Intermediate/`/`DerivedDataCache/`/`Plugins/*/{Content,Binaries,Intermediate}/`) → hard block (`permissionDecision: "deny"`), enforced even in `--dangerously-skip-permissions` mode.
- **Editor open, command invokes `Build.bat`** → hard block (`permissionDecision: "deny"`). Building the editor while it's running corrupts hot-reload state.

Detection is per-project: probes `Saved/Logs/*.log` for an exclusive write lock (UE holds the active log exclusively while running). Other UE instances open for unrelated projects do not trip the guard, and renamed editor binaries don't matter (no process-name scan).

Submodule-aware: commands like `cd Plugins/CkFoundation && git checkout <ref>` are recognised — the script resolves the effective repo root via `git rev-parse --show-toplevel`, enumerates against that repo, and prefixes the resulting paths with the submodule's offset under the project root before classification.

**Limitation — submodule-rooted sessions:** the hook is wired through `CkPlugins/.Codex/settings.json`, which Codex only loads when the session's project root *is* CkPlugins. If you launch Codex from inside a submodule, our hook is not active. Workarounds: (a) launch Codex from the CkPlugins root for any session that may do git ops, or (b) add a personal `~/.Codex/settings.json` invoking a copy of the script kept somewhere stable outside the repo — note this only protects you, not teammates.

Override for the deny tier: `SKIP_UNREAL_GUARD=1`. Use only when you know the affected assets aren't loaded in the editor — the natural recovery is to close the editor and retry.

## Naming Conventions

### Code
- `Ck` prefix for plugin classes (e.g. `UCk_Handle_X`, `FCk_Fragment_X`, `ACk_GameMode`).
- Consistent naming patterns are enforced across all `Ck*` plugins — check the framework guides in `Plugins/CkFoundation/`.

### Assets
- Plugin assets live under `Plugins/<Plugin>/Content/` with the plugin's prefix in the asset name.
- Type suffixes: `_BP` (Blueprint), `_DA` (DataAsset), `_ST` (Struct), `_WBP` (Widget Blueprint).
