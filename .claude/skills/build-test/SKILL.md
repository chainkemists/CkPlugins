---
name: build-test
description: Build the Unreal editor and run automation tests via UnrealToolbox (CkAuto). Use after writing C++ or AngelScript code to compile and verify it — never invoke Build.bat, UnrealBuildTool, or UnrealEditor-Cmd directly. Also covers process-level Gauntlet test runs (--gauntlet).
---

# Build & Test (Unreal Toolbox) — wrapper

This is a thin wrapper: the canonical skill ships inside the CkAuto submodule so
every CK-family project (and every teammate) gets updates on submodule pull,
instead of via pasted copies.

**Read and follow [CkAuto/.claude/skills/build-test/SKILL.md](../../../CkAuto/.claude/skills/build-test/SKILL.md)
in full before invoking the toolbox** — it owns the pre-flight editor-lock
check, the build/test phases, the log-reading rules, the Gauntlet variant, and
the traps. Do not act from this wrapper alone.

(Why a wrapper at all: Claude Code does not discover `.claude/skills/` inside
submodules, so the project root needs this stub for the skill to surface.)
