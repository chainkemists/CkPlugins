# Continuation Prompt — Fix client-side CkActorRelay ensure ("No ActorRelay group registered for tag")

**One-line summary:** A relay group subsystem never registers itself in the tag→subsystem registry **on clients** (its `Initialize` early-returns on `NM_Client` *before* `DoRegisterGroup`), so any client-side `Request_AcquireChannel(<tag>)` trips the `No ActorRelay group registered for tag [...]` ensure. Fix = register the group on clients too; keep only the spawn/login wiring server-gated. Root cause is **fully traced** below — do not re-derive. This is a small, surgical C++ change in `CkFoundation/Source/CkActorRelay`.

> This folder is a self-contained handoff. The raw ensure is in `ensure_log.txt` next to this file. The repo paths below exist on the target machine (same project/submodules).

---

## The bug (user-reported, verbatim from `ensure_log.txt`)

```
Frame#[1045944] PIE-ID[Client 1]
[Client] `FoundSubsystem != nullptr`
Message: No ActorRelay group registered for tag [ActorRelay.Generic]

AS CallStack:
[1] FCk_Handle_PendingActorRelay Request_AcquireChannel(FGameplayTag)            (Generated.utils_actor_relay:16)
[2] void UBb_StoreDriver_EntityScript::Begin_AcquireChannel()                    (line 379)
[3] ECk_EntityScript_ConstructionFlow UBb_StoreDriver_EntityScript::DoConstruct  (line 140)

C++ CallStack:
UCk_ActorRelay_Subsystem_UE::Get_GroupSubsystem()    [CkActorRelay_Subsystem.cpp:19]  <-- ensure fires here
UCk_ActorRelay_Subsystem_UE::Request_AcquireChannel()[CkActorRelay_Subsystem.cpp:36]
UCk_Utils_ActorRelay_UE::Request_AcquireChannel()    [CkActorRelay_Utils.cpp:174]
... AngelScript thunk ...
UCk_GenericEntityScript_UE::DoConstruct()
FProcessor_EntityScript_SpawnEntity_HandleRequests::ForEachEntity()
```

**Symptom:** Fires on **Client 1** (PIE), during entity-script construction, when the BusterBlock store-driver AS script (`UBb_StoreDriver_EntityScript::DoConstruct` → `Begin_AcquireChannel`) calls `UCk_Utils_ActorRelay_UE::Request_AcquireChannel(world, ActorRelay.Generic)`. Server side is fine; only the **client** ensures.

---

## Root cause (fully traced — do NOT re-derive)

1. `UCk_ActorRelay_Subsystem_UE::Get_GroupSubsystem(tag)` (`CkActorRelay_Subsystem.cpp:11-28`) looks the tag up in `_RegisteredGroups` (a `TMap<FGameplayTag, TWeakObjectPtr<...>>`) and fires `CK_ENSURE_IF_NOT(FoundSubsystem != nullptr, "No ActorRelay group registered for tag [{}]")` when it's missing.
2. `_RegisteredGroups` is populated **only** by `UCk_ActorRelay_Subsystem_UE::DoRegisterGroup(this)` (`CkActorRelay_Subsystem.cpp:61-85`).
3. `DoRegisterGroup` is called **only** from `UCk_ActorRelay_Group_Subsystem_Base_UE::Initialize` (`CkActorRelay_GroupSubsystem.cpp:63-109`) — and that call sits **after** this early-return:

   ```cpp
   if (GetWorld()->IsNetMode(NM_Client))
   { return; }                                   // <-- CLIENT BAILS HERE, before DoRegisterGroup

   auto RelaySubsystem = GetWorld()->GetSubsystem<UCk_ActorRelay_Subsystem_UE>();
   CK_ENSURE_IF_NOT(ck::IsValid(RelaySubsystem), ...) { return; }
   RelaySubsystem->DoRegisterGroup(this);        // server-only today
   ... PostLoadMapWithWorld / PostLogin / Logout delegates ...
   if (GetWorld()->HasBegunPlay()) { DoSpawnChannels(); }
   ```

   So on a **client**, the group subsystem object exists (it's a per-world `UCk_Game_WorldSubsystem_Base_UE`), but it never inserts itself into `_RegisteredGroups`. Therefore `Get_GroupSubsystem(ActorRelay.Generic)` returns null on the client → ensure.

4. The early-return was meant to skip **server-authoritative spawning** (channel `SpawnActor`, `GameModePostLoginEvent`, `PostLoadMapWithWorld`), which is correct to keep server-only. But it *also* skips the harmless, needed **registry insertion**. That registry is what lets a client resolve the group and obtain a **pending** handle that completes once the server-spawned channels replicate in.

**Why client acquire is legitimate and must work:** `Request_AcquireChannel` returns a `FCk_Handle_PendingActorRelay`, not a live channel. On the client, the actual `ACk_ActorRelay_UE` channels arrive via replication and self-register into the group's pool through `ACk_ActorRelay_UE::OnRep_GroupSubsystemClass → DoTryRegisterWithGroupSubsystem → DoRegisterChannelActor` (`CkActorRelay_Actor.cpp:68-114`). The pending handle then resolves via `_OnChannelReadyChanged` (`DoBroadcastChannelReadyChanged`, `CkActorRelay_Actor.cpp:134`). **The only missing link is the group never being in `_RegisteredGroups` on the client**, so the very first lookup ensures before any of that can happen.

---

## The fix (approved approach — implement)

In `Plugins/CkFoundation/Source/CkActorRelay/Public/CkActorRelay/CkActorRelay_GroupSubsystem.cpp`, function `UCk_ActorRelay_Group_Subsystem_Base_UE::Initialize` (~line 63): **move the RelaySubsystem fetch + `DoRegisterGroup(this)` above the `NM_Client` early-return.** Registration happens on every world (server + client); only spawning/login wiring stays server-gated.

```cpp
auto
    UCk_ActorRelay_Group_Subsystem_Base_UE::
    Initialize(FSubsystemCollectionBase& InCollection) -> void
{
    Super::Initialize(InCollection);

    InCollection.InitializeDependency<UCk_ActorRelay_Subsystem_UE>();

    // Register the group in the tag->subsystem registry on BOTH server and client.
    // Clients must be able to resolve the group to acquire a (pending) channel; the
    // actual channels are server-spawned and replicate in, self-registering via
    // ACk_ActorRelay_UE::OnRep_GroupSubsystemClass. Without this, the very first
    // client-side Request_AcquireChannel ensures with "No ActorRelay group registered".
    auto RelaySubsystem = GetWorld()->GetSubsystem<UCk_ActorRelay_Subsystem_UE>();

    CK_ENSURE_IF_NOT(ck::IsValid(RelaySubsystem),
        TEXT("UCk_ActorRelay_Subsystem_UE is not available during group subsystem initialization"))
    { return; }

    RelaySubsystem->DoRegisterGroup(this);

    // Everything below is server-authoritative: only the server spawns channels and
    // reacts to login / map-load events. Clients receive channels via replication.
    if (GetWorld()->IsNetMode(NM_Client))
    { return; }

    _PostLoadMapWithWorldDelegateHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UCk_ActorRelay_Group_Subsystem_Base_UE::OnPostLoadMapWithWorld);

    _PostLoginEventDelegateHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(
        this, &UCk_ActorRelay_Group_Subsystem_Base_UE::OnPostLoginEvent);

    _LogoutEventDelegateHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(
        this, &UCk_ActorRelay_Group_Subsystem_Base_UE::OnPlayerLogout);

    // (unchanged spawn comment block here)
    if (GetWorld()->HasBegunPlay())
    {
        DoSpawnChannels();
    }
}
```

**Symmetry check (already correct, no change needed):** `Deinitialize` (`CkActorRelay_GroupSubsystem.cpp:143-159`) calls `PostLoadMapWithWorld.Remove(...)` / `GameModePostLoginEvent.Remove(...)` / `GameModeLogoutEvent.Remove(...)` on possibly-unset `FDelegateHandle`s (no-op on client — safe) and calls `RelaySubsystem->DoUnregisterGroup(this)` **unconditionally**. That now correctly pairs with the unconditional `DoRegisterGroup`. Leave `Deinitialize` as-is.

**Do NOT** also move the spawn delegates or `DoSpawnChannels()` — spawning is server-authoritative (see "Architecture notes").

---

## Why this is safe / what was verified about the downstream path

- `DoTryResolve` (`CkActorRelay_GroupSubsystem.cpp:243`) on a client, `ECk_ActorRelay_AcquireKind::Server` case: selects from `_ServerChannels`, which on the client is filled by replication+OnRep. On saturation it calls `DoMaybeGrowPool`, which **already early-returns on `NM_Client`** (recent lazy-spawn change) — clients never spawn. Good.
- `Request_AcquireChannel()` on the group subsystem ensures `Get_OwnershipPolicy() == ServerOwned` for the `Server` kind; `ActorRelay.Generic` is `ServerOwned` (`CkActorRelay_GenericGroupSubsystem.cpp`), so the client path is valid.
- `DoRegisterGroup` ensures `NOT _RegisteredGroups.Contains(GroupTag)` (no duplicate). Each group subsystem is a per-world singleton and registers exactly once in `Initialize`, so no duplicate-registration ensure.

---

## Repo state (target machine should `git pull` first)

- **CkFoundation** submodule (`Plugins/CkFoundation/`, branch `dev`): origin tip is `83dad665d` — `perf(CkActorRelay): lazy-spawn channel pools to fix client-join Iris burst` (already pushed). **This bug is independent of that lazy-spawn change** (the `NM_Client` early-return predates it), but lives in the same file (`CkActorRelay_GroupSubsystem.cpp`). Branch off `origin/dev` for this fix.
- **Parent CkPlugins**: `origin/dev` references CkFoundation `83dad665d` after a bump. (Whoever sent you this may still be finalizing/pushing the parent pointer — confirm `Plugins/CkFoundation` points at a commit that contains the lazy-spawn change before building.)
- **Sibling repo** `BusterBlock` (the repro vehicle): contains `UBb_StoreDriver_EntityScript` (the AS class in the callstack). The CkFoundation fix flows there via a submodule bump after it lands on CkFoundation `dev`. You don't need BusterBlock to *fix* this — only to reproduce the exact store-driver scenario.
- No work started on this bug yet. Nothing uncommitted related to it.

---

## Critical files

- `Plugins/CkFoundation/Source/CkActorRelay/Public/CkActorRelay/CkActorRelay_GroupSubsystem.cpp` — **the file to edit.** `Initialize` (~63, the early-return at ~73), `Deinitialize` (~143, already symmetric), `DoTryResolve` (~243), `DoMaybeGrowPool` (~417, already `NM_Client`-guarded), `DoSpawnChannels*` (~498+, server-only).
- `Plugins/CkFoundation/Source/CkActorRelay/Public/CkActorRelay/CkActorRelay_Subsystem.cpp` — `Get_GroupSubsystem` (~11, the ensure at :19), `Request_AcquireChannel` (~30), `DoRegisterGroup` (~61) / `DoUnregisterGroup` (~87). Read-only for this fix.
- `Plugins/CkFoundation/Source/CkActorRelay/Public/CkActorRelay/CkActorRelay_Actor.cpp` — `OnRep_GroupSubsystemClass` (~68) → `DoTryRegisterWithGroupSubsystem` (~98) → `DoRegisterChannelActor`; `DoStartBroadcastWhenReadyPolling` (~116) → `DoBroadcastChannelReadyChanged` (:134). This is how client channels self-register + unblock pending handles. Read-only.
- `Plugins/CkFoundation/Source/CkActorRelay/Public/CkActorRelay/CkActorRelay_GenericGroupSubsystem.cpp` — the `ActorRelay.Generic` group: `ServerOwned`, `RoundRobin`, `MaxEntitiesPerChannel == 0`, `ChannelCount == GenericChannelCount`. Confirms the acquire kind = `Server`.
- `Plugins/CkFoundation/Source/CkActorRelay/Public/CkActorRelay/CkActorRelay_Utils.cpp` — `Request_AcquireChannel` (:174, the BPFL the AS script calls) and `Promise_OnAcquired` / pending subscription. Read-only.
- `ensure_log.txt` (in this folder) — the raw report.

---

## Things ruled out — do NOT re-investigate

| Ruled out | Why |
|---|---|
| Caused by the recent lazy-spawn change (`83dad665d`) | The `NM_Client` early-return that skips `DoRegisterGroup` predates it. The lazy-spawn change only altered spawn counts + added a server-gated grow hook. |
| The AS script is calling the wrong API | `UBb_StoreDriver_EntityScript` correctly calls `UCk_Utils_ActorRelay_UE::Request_AcquireChannel` → `RelaySubsystem->Request_AcquireChannel(tag)`. Acquiring on the client is legitimate (returns a pending handle). |
| Tag not registered / typo in `ActorRelay.Generic` | The server registers and resolves it fine; the failure is purely the client-side `_RegisteredGroups` being empty. |
| Need new pending/replication plumbing | The pending → `OnRep` → `DoBroadcastChannelReadyChanged` path already works once the group is in the registry. Single-line-of-cause fix. |
| Should gate the client acquire instead (don't acquire on client) | The store driver legitimately needs the channel on the client; the framework already supports client acquires — the registry insertion is just missing. |

---

## Architecture notes / gotchas

- **Two registrations, don't conflate them:** (a) *group registration* = `DoRegisterGroup` inserts the subsystem into `_RegisteredGroups` keyed by `GroupTag`. (b) *channel registration* = `DoRegisterChannelActor` adds a spawned/replicated `ACk_ActorRelay_UE` into the group's pool. The bug is **(a) is skipped on clients**; (b) already happens on clients via `OnRep`.
- **Spawning is server-authoritative** and must stay so: `DoSpawnChannels*`, `GameModePostLoginEvent`, `PostLoadMapWithWorld` cleanup/respawn. Do not move these out of the server gate.
- **Group subsystems are per-world** (`UCk_Game_WorldSubsystem_Base_UE`): created/destroyed per world, so registration happens once per world in `Initialize`, and `Deinitialize` unregisters. No cross-world leakage.
- **Generic is `ServerOwned`** → acquire kind `Server` → single shared pool (post-lazy-spawn: 2 warm channels, never grows since `MaxEntitiesPerChannel == 0`). Those 2 channels replicate to each client and self-register.
- **`DoMaybeGrowPool` is already `NM_Client`-guarded** — after this fix, a client `DoTryResolve` that finds an empty/not-yet-replicated pool returns `{}` and the consumer stays pending (correct), without the client trying to spawn.

---

## Recommended implementation / verification flow

1. `git -C Plugins/CkFoundation pull` and confirm `dev` tip contains the lazy-spawn commit (`83dad665d` or descendant). Branch the fix off `dev`.
2. Apply the `Initialize` change above. Build the editor:
   - Toolbox (preferred): `CkAuto/UnrealToolbox.exe --build --target Editor --config Development --project <abs>/CkPlugins.uproject`
   - Editor must be **closed** (the build hook blocks `Build.bat` while it's open; poll `Saved/Logs/CkPlugins.log` for an exclusive lock if another session may have it open).
3. **Reproduce the client path.** Two options:
   - Run the two-player listen-server net gym (`Ck_Gym_GoTo Net Two-Player`) and confirm **no** `No ActorRelay group registered` ensure on `[Client 1]`.
   - Better: add/extend the **CkActorRelay net spec** to assert that a **client-side** `Request_AcquireChannel(ActorRelay.Generic)` resolves (via `Promise_OnAcquired`) without ensuring — this pins the regression. Run `CkAuto/UnrealToolbox.exe --test --test-pattern ActorRelay` and the broader `--test-pattern Net`.
4. Confirm the server path still works (no behavioural change there) — the same ActorRelay + Net test runs cover it.
5. Spot-check a downstream consumer (`CkCue`/`CkVfx`/`CkAudio`) still acquires fine: `--test-pattern Cue`.
6. Commit to CkFoundation `dev` (message e.g. `fix(CkActorRelay): register relay groups on clients so client-side acquire resolves`), rebase onto `origin/dev`, **ask before pushing**. Then bump the parent CkPlugins submodule pointer, and the BusterBlock pointer, as needed.

---

## Suggested first message (verbatim)

> Continuing the CkActorRelay client-registration fix. Read the continuation prompt fully first: `<this-folder>/CONTINUATION_PROMPT_RelayClientRegistration.md` (raw ensure in `ensure_log.txt` beside it). Root cause is fully traced: `UCk_ActorRelay_Group_Subsystem_Base_UE::Initialize` early-returns on `NM_Client` *before* `RelaySubsystem->DoRegisterGroup(this)`, so the tag registry is empty on clients and the first client-side `Request_AcquireChannel(ActorRelay.Generic)` ensures. Apply the approved fix (move the RelaySubsystem fetch + `DoRegisterGroup` above the `NM_Client` early-return; keep spawn/login wiring server-gated), build, then verify with the ActorRelay + Net test patterns and a two-player listen-server run.
