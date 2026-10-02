# CkFoundation architecture audit for ECS online services

Date: 2026-10-02. Selected checkout: `D:\Repos\CkPlugins`. Foundation HEAD observed: `6d7019e53d6e79a087c747ecddce8e9074b87d57`; the checkout has existing documentation and ensure-related dirt. This document records live source evidence, not a build or runtime verdict. No build, editor boot, network operation, repository copy, worktree, or Git mutation was performed. The only authored file is this document.

For evidence below, `Source/...`, `Script/...`, and `DEVELOPMENT.md` are relative to `Plugins/CkFoundation/`; explicitly prefixed root paths are relative to the CkPlugins root. Lines refer to the selected checkout on the audit date. Existing docs can describe broader intent than live code; discrepancies are identified below.

## Findings that constrain the design

1. Ck has the ECS building blocks for an online-service domain, but no discovered online-service implementation. Its `CkNet` is world role/authority/replication plumbing inside **CkEcs**, not an online authentication/session/lobby provider or a standalone module.
2. Normal gameplay entities belong to a `UWorld` registry, which is destroyed at world teardown. Persistent login, presence, lobby, and outstanding service operations therefore need an explicit owner outside that world registry if they must survive travel.
3. `CkGameSession` already proves a **GameInstance-owned private FEcsWorld for ECS signals**. It does not prove a persistent processor pump: its private registry has no scheduler, and live code only translates UE GameMode player login/logout events.
4. Existing processor and scheduler APIs can accept a dedicated registry without spawning an Actor. A curated service graph and non-Actor pump are feasible source-level seams, but are **new infrastructure requiring a focused spike**, including lifecycle ordering and travel/shutdown tests.
5. Backend callbacks must be captured as bounded immutable completion data and applied on the game thread through the service domain. Ck signals execute subscribers synchronously and mutate registry storage; calling them from an arbitrary backend thread would violate the registry contract.
6. Replication is a separate concern. Existing gameplay replication requires an Actor in the owning chain. Online identities/lobbies/operations need not be replicated fragments or ActorRelay channels; player/world projections can use existing gameplay replication at a deliberate boundary.

## Actual EIK dependency and scan scope

The root `CkPlugins.uproject:131-132` declares `EOSIntegrationKit` with **Enabled=false**. `OnlineSubsystem` and `OnlineSubsystemUtils` are enabled at `CkPlugins.uproject:16-21`. `DisableEnginePluginsByDefault=true` is at `:6`; the engine association is a GUID at `:3` and was not resolved to or inspected in another repository.

`Plugins/EIK` is absent in the selected checkout (`Test-Path Plugins/EIK` returned false), there is no EIK entry in root `.gitmodules`, and `git ls-files Plugins/EIK` returned no entry. Thus this checkout does not expose EIK source for an API-by-API migration census. It does not establish whether the associated engine has EIK installed. No other engine/repository directory was browsed.

Live dependency/content scan covered root `Source/`, `Config/`, `Script/`, every present plugin's `.Build.cs` / `.uplugin` / `.ini`, and explicitly Foundation `Source/` / `Script/` plus CkTests source. Searches used `rg --no-ignore` for code/scripts so ignored generated AS files were included. EIK/OSS name hits in present implementation were only the root descriptor, commented template suggestions in `Source/CkPlugins/CkPlugins.Build.cs:21-23`, and a historical test-tool comment in `Plugins/CkTests/Source/CkTestsEditor/Private/CkAutoTestMapPopulator.cpp:1713`. No Foundation EIK/OSS module dependency or direct online interface consumer was found in this scope. `Plugins/CkApplication` is a root gitlink but its Source/Script directories are not populated here; no absence claim applies to that submodule's unavailable content.

Excluded from migration-consumer proof: binary assets (`Content/`), `Saved/`, `Intermediate/`, `Binaries/`, `_scratch`, release output, other projects/checkouts, the installed/associated engine, and external source. A broad historical docs scan did find older EIK removal/bisect plans and historical JSON snapshots; those are not current dependency evidence. Root `compile_commands.json` points at external engine OSS sources but was not treated as live EIK dependency or source access authority.

Suggested repeatable scans:

```powershell
rg -n -i --no-ignore '\b(EOSIntegrationKit|EIK|IOnlineSubsystem|OnlineSubsystemEOS|OnlineServices|EOS_[A-Za-z0-9_]+)\b|OnlineSubsystem' Plugins/CkFoundation/Source Plugins/CkFoundation/Script Source Script Config -g '*.h' -g '*.cpp' -g '*.as' -g '*.cs' -g '*.ini' -g '!**/Intermediate/**' -g '!**/Binaries/**'
rg -n 'EOSIntegrationKit|EIK|OnlineSubsystem' .gitmodules CkPlugins.uproject Config Plugins -g '*.Build.cs' -g '*.uplugin' -g '*.ini' -g '!**/Intermediate/**' -g '!**/Binaries/**'
git ls-files Plugins/CkApplication Plugins/EIK
Test-Path Plugins/EIK
```

## Registry ownership, travel, and handles

| Existing mechanism | Live evidence | Consequence for online services |
|---|---|---|
| Gameplay registry is owned by `UCk_EcsWorld_Subsystem_UE` | `Source/CkEcs/Public/CkEcs/Subsystem/CkEcsWorld_Subsystem.h:148,295-304`; `.cpp:134-157` allocates the registry/slot, transient root, and weak world ref | Map/world ownership is unsuitable as the only lifetime for persistent authentication and operations |
| Gameplay processors tick in `ACk_EcsWorld_Actor_UE : AInfo` | subsystem `.h:91`; `.cpp:69-91,609-658` builds the global graph and spawns one scheduler Actor per non-empty partition | A feature with no actor entity can still use the gameplay registry, but that infrastructure itself uses scheduler Actors; a strict non-Actor persistent service pump needs its own owner |
| World end-play drains entity teardown before freeing the registry | subsystem `.cpp:287-319,355-424` requests destruction of every non-transient entity, pumps at zero time, frees slot before registry reset | Outstanding provider callbacks must not depend on old world fragments or cached references |
| `ck::FEcsWorld` is registry/slot RAII only | `Source/CkEcs/Public/CkEcs/World/CkEcsWorld.h:13-39`; `.cpp:7-29` | It has no world pointer, tick, scheduler, graph, or graceful feature cleanup routine |
| Registry handles use slot + generation | `Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp:179-254,258-302,308-325` | Slot free advances generation; an old handle cannot silently reach the replacement registry. This protects registry dereference, not an external callback's captured owner/payload lifetime |
| World resolution walks the lifetime chain | `Source/CkEcs/Public/CkEcs/EntityLifetime/CkEntityLifetime_Utils.cpp:301-327` | A bare FEcsWorld service entity is not a valid input to arbitrary world-dependent features; its transient root has no world ref |
| Lifetime dependent collections assume same-registry children | lifetime utils `.cpp:646-656` | Do not establish persistent service ownership by adding world handles to service `LifetimeDependents`; use explicit operation IDs and weak projection links that are recreated after travel |

`Request_CreateEntity(Registry)` can create a service-domain entity without an Actor (`CkEntityLifetime_Utils.cpp:466-491`). Creating through the owner overload also installs lifetime/context inheritance (`:437-461,551-656`). The private GameSession host uses the registry overload. A dedicated service registry must deliberately define which entities are owned by its root and how per-user and per-operation children terminate; relying on registry reset alone skips feature cancellation/release semantics.

`ContextOwner` and `LifetimeOwner` remain different contracts: context resolves configuration/service scope; lifetime determines destruction. World lookup assumes a world-backed lifetime chain, so online configuration/provider resolution should not be smuggled through world lookup.

## Best proven neighboring modules

### CkGameSession: persistent signal-host ownership

`Source/CkGameSession/Public/CkGameSession/Subsystem/CkGameSession_Subsystem.h:53` derives from `UGameInstanceSubsystem`; `:117-124` owns `TUniquePtr<ck::FEcsWorld>`, one signal handle, and external delegate handles. Initialize allocates FEcsWorld and creates the signal entity (`.cpp:27-31`). Deinitialize removes both global delegates, clears the handle, then resets the world (`:39-46`). Player login/logout broadcast Ck signals (`:76-102`); bind/unbind functions occupy `:105-159`.

This is the closest lifetime/signal precedent for a GameInstance online service host. Do not conflate its player admission events with remote online authentication/session admission. Its `Claude.md:3` describes lobby/loading/results/session settings, but this inspected subsystem implements only UE player login/logout forwarding and a controller list. No online-session API, lobby operation processor, or service scheduler exists in the inspected module.

The handlers subscribe to process-global `FGameModeEvents`, and their bodies do not filter `InGameModeBase` to this GameInstance (`.cpp:27-28,76-102`). Reusing the shape requires explicit instance/world routing for multi-PIE and travel; the current neighbor should not be copied mechanically.

### CkLoadingScreen: a non-Actor GameInstance tick owner

`Source/CkLoadingScreen/Public/CkLoadingScreen/Subsystem/CkLoadingScreen_Subsystem.h:42-65` combines `UGameInstanceSubsystem` and `FTickableGameObject`. Initialize registers map delegates and enables ticking; Deinitialize removes delegates, resets retained assets, and disables ticking (`.cpp:108-137`). This proves the repository uses non-Actor GameInstance ticking. Its predicates depend on a viewport and exclude dedicated servers (`.cpp:139-183`), so those policies must **not** be carried into server-capable online services. Map filtering and tick behavior while no world is available need explicit online rules and verification.

### CkTimer: ECS public surface and deferred completion

Use the small Timer quartet for reflected Specs/requests, typesafe handle, fragment state, request enqueue, processor mutation, signal APIs, and completion. `Source/CkTimer/Public/CkTimer/CkTimer_Processor.cpp:48-66` copies/resets requests before visiting and removes the request marker only after detecting whether callbacks re-enqueued. `DEVELOPMENT.md:349-403` defines generic deferred completion and per-operation result payload coexistence. `Source/CkEcs/Public/CkEcs/Request/CkRequest_Data.h:95-109` provides `Set_CompletionDelegate` / `TryFireCompletion`.

Generic completion means `Succeeded`, `Failed`, `Failed_NotEnqueued`, or `Failed_Cancelled`; online-specific error category, retryability, identity/lobby result, and cancellation/timeout semantics remain domain data. Online asynchronous launch must not report provider success merely because the processor successfully dispatched an SDK call. Retain pending operation state until its terminal completion is processed.

Neighbor drift: Timer utility and lifetime utility retain duplicated ensure/ordinary guards in places; current `AGENTS.md` owns the required inverted-ensure shape. This audit does not fix existing drift or license reproducing it.

### CkResourceLoader: bounded pending work and rooting, with a caution

`Source/CkResourceLoader/Public/CkResourceLoader/CkResourceLoader_Fragment_Data.h:155-181` exposes a rooted batch with readiness/failure polling; the shared streamable handle roots loaded assets. `CkResourceLoader_Processor.cpp:348-355` completes pending queued requests as cancelled at EndPlay.

The old delegate API captures raw processor `this` using `FStreamableDelegate::CreateRaw` (`CkResourceLoader_Processor.cpp:96-110`) and callbacks directly read/mutate entity fragments (`:213-236,241-298`). This is **not proof of a lifetime-safe arbitrary backend callback bridge**. Reuse completion/rooting/cancellation concepts; do not copy the raw callback capture into online code. A service callback adapter must survive immediate callbacks, late callbacks, entity destruction, provider shutdown, and map travel without touching dead or foreign registries.

### CkVoiceChat / ActorRelay: enqueue boundary, not online provider reuse

Voice RPC bodies validate sender/talker and enqueue server/receive inboxes (`Source/CkVoiceChat/Public/CkVoiceChat/Net/CkVoiceChatRelay_Actor.cpp:20-56,63-92`). Control RPC appends to a transient-entity control inbox (`CkVoiceChatControlRelay_Actor.cpp:15-35`). These are useful examples of a transport delivering bounded input for processors to consume.

They require Actor RPC endpoints; `CkVoiceChatRelay_Actor.h:31,39` uses unreliable RPCs and the control relay uses reliable RPC at `CkVoiceChatControlRelay_Actor.h:30`. VoiceChat's own module doc explicitly states zero coupling to OSS/sessions/external services (`Source/CkVoiceChat/Claude.md:8-18`). It is not EOS RTC or party/lobby voice. ActorRelay is world-scoped (`Source/CkActorRelay/Public/CkActorRelay/CkActorRelay_Subsystem.h:14`) and its relay is an Actor (`CkActorRelay_Actor.h:14`). The public API is actually `Request_AcquireChannel` / `_ForPlayer` (`CkActorRelay_Utils.h:59,68`), not the simplified Acquire/Broadcast API advertised in its module description.

## Can the existing processor machinery host a service-only registry?

**Source-level feasibility: yes; existing proven deployment: not found.**

- `TProcessorBase` takes an `FCk_Registry` and resolves its transient entity from registry context (`Source/CkEcs/Public/CkEcs/Processor/CkProcessor.h:298-304`). The typed processor iterates that registry and builds typed handles (`:699-708`). A UObject/world is not required by the base constructor.
- `FProcessorGraphBuilder::Build` accepts an explicit descriptor list, registry, transient, unresolved-reference policy, and Runtime/Editor context (`Source/CkEcs/Public/CkEcs/Scheduler/CkProcessorGraph.h:129-134`). It instantiates processors using that registry (`CkProcessorGraph.cpp:137-153`).
- `FProcessorScheduler` owns a partition with processor instances; `Tick` receives time, registry, and Full/LoadKernel scope (`CkProcessorScheduler.h:43-48`; `CkProcessorGraph.h:66,84-102`). A non-Actor subsystem can own and call it.
- `BuildDescriptor<T>` and the factory shape are existing APIs (`CkProcessorTraits.inl.h:258-264`; `CkProcessorDescriptor.h:98`). Global static registration is optional for assembling a local explicit descriptor list; using `CK_REGISTER_PROCESSOR` would also expose the service processor to normal global gameplay graphs (`CkProcessorRegistration.h:19-25,88-94`). The prototype must establish intended registration ownership rather than creating accidental double execution.
- Graph context only distinguishes Runtime/Editor, not Gameplay/Online. Net-mode requirements consult the transient's CkNet tags (`CkProcessorGraph.cpp:194-200`; `CkProcessor_NetModePolicy.cpp:16-40`). A bare FEcsWorld has no automatically installed net metadata. A service graph should not classify provider operations by Actor role or pretend to be a listen server just to satisfy those predicates.
- `LoadKernel` is the snapshot rebuild scope, not a service kernel (`CkProcessorScheduler.h:13-18`; `CkProcessorDescriptor.h:67-78`). Do not use it as the online service selection mechanism.

**Lifetime kernel is the main prototype risk.** Deferred destroy requires EndPlay, Teardown, Await, Finalize, then real registry destroy (`Source/CkEcs/Public/CkEcs/EntityLifetime/CkEntityLifetime_Processor.h:36-158`; `.cpp:38-145`). These processors themselves can act on actorless entities, but their declared groups/edges reference the broader graph: lifecycle follows replication, teardown follows EndPlay, Finalize follows OwningActor_Destroy (`CkProcessorGroups.h:135-151`; lifetime processor `.h:97`). Missing group/RunAfter references trigger ensures even in Permissive mode (`CkProcessorGraph.cpp:291-300,344-354`). Therefore a naive handful of copied global descriptors is insufficient. The spike must construct a dependency-complete small graph, explicitly rebind curated kernel descriptor groups/edges or use dedicated service ordering, and prove cancellation/release happens before final destroy. This is new integration, not evidence for a broad CkEcs scheduler rewrite.

Retain existing pump semantics: reactive processors consume their dirty marker; non-idempotent time integration does not replay through the pump. The scheduler already precomputes pump eligibility (`CkProcessorScheduler.cpp:84-102`) and caps iterations (`CkProcessorScheduler.h:87`). A manual loop that calls arbitrary processors until requests appear empty would lose ordering, completion, marker, and re-entry guarantees. Shutdown must disable new admission and callbacks, terminate operations, drain required lifecycle work, destroy scheduler instances, and only then free the registry slot; FEcsWorld destructor itself only invalidates/reset storage.

## Callback, signal, and GC contracts

`registry_table::Allocate/Free` assert game-thread execution (`Source/CkEcs/Public/CkEcs/Registry/CkRegistry_SlotTable.cpp:181-182,260-261`). Resolve/TryResolve permit reads on scheduler workers only under a documented parallel region; mutation is guarded against active parallel iteration (`CkRegistry_SlotTable.h:33-47`). This is not a general concurrent queue/registry API. A backend worker callback must copy value data into an appropriately synchronized bridge inbox; registry admission and application run on the owning game thread.

Signals retain payload and synchronously publish subscribers (`Source/CkEcs/Public/CkEcs/Signal/CkSignal_Utils.inl.h:149-173`). Bind can immediately replay this-frame/last payload (`:196-210`). Thus provider callback handling must account for re-entrant public calls, teardown, and a listener queuing new operations. Use operation correlation/generation; do not capture a fragment reference across callbacks/frames. Retained signal arguments prohibit raw pointer/reference arguments (`CkSignal_Fragment.h:43-44`) and validate retained InstancedStruct GC independence (`CkSignal_Utils.inl.h:38-64`).

Signal delegate connections have explicit teardown constraints: the connection token is non-owning; destroying the signal host/registry must not try to release a token into an already destroyed sigh. Removing only the delegate fragment from a live entity must release first (`CkSignal_Fragment.h:146-154`; `CkSignal_Utils.inl.h:568-589`). Use existing Unbind APIs for live-host removal; do not add destructor-driven unbinding blindly.

**Current doctrine supersedes older skill guidance:** `DEVELOPMENT.md:410-439` requires weak refs in ECS fragments, feature-created UObjects pinned by ObjectPooling, and asset batches rooted by streamable handles. Observed actors/worlds use `TWeakObjectPtr`; reflected subsystem-owned `UPROPERTY TObjectPtr` is a traced root. No new raw pointer storage, fragment-only strong holder, AddToRoot, or bare NewObject+strong fragment is a sanctioned new pattern. Live ObjectPooling pins objects in UPROPERTY storage (`Source/CkCore/Public/CkCore/ObjectPooling/CkObjectPooling_Subsystem.h:51-78,103-104`). Older `ckecs-domain-reference` says owning fragments should use TStrongObjectPtr; follow current DEVELOPMENT instead.

Provider SDK interfaces/handles are not necessarily UObjects. Their native ownership and callback-unregistration rules need provider-specific documentation; no existing Ck wrapper was discovered. A GI subsystem may own native managed provider state while ECS fragments retain correlation IDs/value state and weak UObject context. This remains a design seam, not a declared existing API.

## Module direction and reflected AS API

The topology requires same/lower-tier dependencies and forbids runtime-to-editor dependencies (`Source/ARCHITECTURE.md:135-138,267,299`). CkEcs depends on Core/Log/Memory/Profile/Settings/ThirdParty; actual `CkEcs.Build.cs:24-41` also lists Iris/IrisCore/NetCore. It has no OSS dependency. ResourceLoader is a small low-tier Core/Ecs feature (`Source/ARCHITECTURE.md:174`), ActorRelay includes EcsExt/Label (`:190`), and GameSession includes Label/Record (`:214`).

A provider-neutral runtime online module can depend on Core/Ecs/settings and selected composable features; provider implementations can depend on that online contract and UE/EOS interfaces. Reversing that direction would force EOS/OSS into CkEcs or the neutral public header surface. A feature/provider module split is a proposal for the lead's final design, not a checked-in module/API. `CkProvider` is existing data-asset value-provider machinery, not an online-provider registration interface (`DEVELOPMENT.md:109`).

Public feature APIs must be C++/Blueprint/AS-consumable (`DEVELOPMENT.md:62-65`). Use reflected Specs, result/error USTRUCTs/enums, typed handles, utils getters and Request methods, and generated signal Bind/Unbind functions. `ScriptMixin` is demonstrated by `CkNet_Utils.h:70` and Timer utils. Generated AS wrappers show `namespace utils_timer` and mixin forwarding plus generic completion default (`Script/Generated/utils_timer.as:5,91-94`). The generator discovers native BlueprintFunctionLibraries (`Source/CkAngelscriptGenerator/CkAngelscriptWrapperGenerator.cpp:102-107`); the AS guide prescribes `utils_*` wrappers and explains mixin-only static binding (`Script/ARCHITECTURE.md:123-130`).

AS is optional: CkModuleRules detects the fork's Angelscript plugin and sets `WITH_ANGELSCRIPT_CK=1/0` (`Source/CkBuildConfig/CkBuildConfig.Build.cs:13-34,53-60`). New runtime domain types must not require editor generation modules or provider SDK native objects in BP/AS payloads. Dynamic AS handles are a separate registry/generation path; start with native typesafe handles unless a demonstrated consumer needs dynamic compositions. Generated files are boot/tool-owned; this audit wrote none.

## Existing versus missing infrastructure

| Existing, source verified | Missing / must be designed and verified |
|---|---|
| Generational registries/handles; actorless entity creation | Online account and product-user identity domain, local-user scope, provider interface/capability model |
| FEcsWorld private registry RAII; GI signal-host precedent | Service registry tick owner and service-only graph/lifecycle integration |
| Deferred requests, generic completion/cancellation, synchronous Ck signals | Async operation correlation, deadlines, cancellation/late-completion policy, typed service errors, immutable SDK completion ingress |
| Settings and reflected utils/mixin/wrapper patterns | Auth/lobby/session/presence/friends/invite/matchmaking/cloud/stats/achievement APIs; provider capability census not performed here |
| Existing CkNet roles + Iris replicated-fragment path | Online connectivity/identity/provider readiness distinct from gameplay net roles |
| ActorRelay RPC endpoints and VoiceChat inbox transport | Direct non-Actor provider callback bridge and SDK notification ownership |
| World teardown and ObjectPooling rooted ownership | Persistent session-to-world projection, travel intent/admission, multi-PIE instance routing, provider shutdown quiescence |

## Recommended evidence gates for the next design stage

This audit recommends a narrow architecture spike before broad features: a dedicated GI-owned FEcsWorld, two fake-provider operations, explicit service-only descriptor graph, value completion inbox, and an AS/BP/C++ public facade. Demonstrate launch, immediate callback, queued callback, re-entrant listener, rejection before enqueue, timeout, cancellation, duplicate/late callback, account/provider generation change, travel with no usable world, multi-PIE isolation, and GI shutdown with pending notifications. Assert no downstream callback/mutation after rejection and no partially active registration on failure. Exercise actorless lifecycle cancellation/release before registry free and prove no global gameplay processor injection.

No build/runtime validation was performed, and no claim of travel safety, dedicated-server tick support, provider SDK callback behavior, runtime cost, or EIK feature parity is established. The lead owns provider selection, entity topology, graph integration policy, feature scope, and final architecture recommendation.
