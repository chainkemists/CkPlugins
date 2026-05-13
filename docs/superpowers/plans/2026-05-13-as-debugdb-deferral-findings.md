# Task 11 (editor-startup): Defer AS Debug Database — Findings

**Date:** 2026-05-13
**Status:** NOT NEEDED — already lazy in current upstream
**Engine fork:** `D:\Repos\UnrealEngineAngelscript`
**Task budget:** ≤30 lines engine patch
**Actual change shipped:** 0 lines (documentation only)

## Summary

The Hazelight AS debug database send (`FAngelscriptDebugServer::SendDebugDatabase`)
is **already fully gated on an explicit client request** in the current upstream
fork. No startup-time or post-engine-init unconditional call exists. The
historical `"Sending debug database took %.3g seconds"` log line at
`AngelscriptDebugServer.cpp:1981` is reachable, but only via the two call
sites below, neither of which fires during editor init.

## Code path audit

`grep -rn "SendDebugDatabase"` in
`Engine/Plugins/Angelscript/Source/AngelscriptCode/`:

| Site | File:Line | Trigger |
| --- | --- | --- |
| Call A | `Private/Debugging/AngelscriptDebugServer.cpp:715` | `HandleMessage(EDebugMessageType::RequestDebugDatabase)` — a connected client explicitly asked for the DB. |
| Call B | `Private/Debugging/AngelscriptDebugServer.cpp:1364` | `BroadcastDebugDatabase()` — loops `ClientsThatWantDebugDatabase`, which is populated *only* at line 714 inside Call A's handler. |
| Definition | `Private/Debugging/AngelscriptDebugServer.cpp:1368` | The function itself. |

`BroadcastDebugDatabase` is invoked from:

- `AngelscriptManager.cpp:3082` (public passthrough)
- `Bind_FGameplayTag.cpp:135` (on gameplay-tag-table change; pushes a *refresh*
  to already-connected debuggers).

Neither runs during engine init unless a debugger client is connected. With
zero clients, both are no-ops (the loop body never executes).

## Connection-accept path also clean

`AngelscriptDebugServer.cpp:300` binds `HandleConnectionAccepted` which queues
new sockets into `PendingClients`. `ProcessMessages` (line 602) drains the
queue into `Clients` and logs `"Added angelscript debug client from %s"` — it
does **not** proactively `SendDebugDatabase`. The client must send a
`RequestDebugDatabase` message after handshake. So the design is already
"lazy on first request" by protocol.

## Why the log marker disappeared

The plan brief noted the historical `Sending debug database took X seconds`
line is no longer observed during startup. Confirmed: it's still present at
`AngelscriptDebugServer.cpp:1981`, but it only runs inside `SendDebugDatabase`,
which only runs when a client has connected and asked. No debugger client
attaches during a normal toolbox `--test` editor launch, so the line never
fires. The upstream fork (or some prior local refactor) already implements
the deferral this task was scoped to add.

## What we did NOT do

- Did not rebuild the engine (no code change to validate).
- Did not run the `--test --measure` build (no change to measure).
- Did not commit anything in `D:\Repos\UnrealEngineAngelscript`.

## Recommendation

Mark Task 11 as **already satisfied upstream**. If a future profiler trace
shows any remaining debug-DB cost during editor init, re-audit — it would
have to come from something other than `SendDebugDatabase` (e.g. eager
DB construction inside `FAngelscriptDebugDatabase::FillTypes` called from
some other path). None was found in this audit.

Any `engine_init_seconds` savings the plan hoped to harvest from this task
do not exist in the current fork — they were already harvested whenever the
upstream change landed.
