# Replacing EOSIntegrationKit with CkFoundation online features

**Research completed: 2026-10-02. Architecture status: proposed, awaiting product/architecture decisions. Implementation status: not started.**

## Recommendation

Build Ck-owned entity features for identity, social/invitations, lobbies, text chat and game connection, with **Steam and Direct Network profiles first** and an optional EOS adapter later. Keep the online domain independent of Actors, and retain Unreal's NetDriver/travel/replication boundary where the engine requires it.

The best starting architecture is a GameInstance-owned persistent ECS service world with a small non-Actor processor host. `CkGameSession` already demonstrates private `FEcsWorld` ownership, and existing processor graph APIs accept a registry. The missing piece is a correctly scoped service graph with lifecycle, async cancellation and shutdown—not a new networking framework from scratch.

Before broad implementation, prove that persistent service host and the actual Steam/Direct gameplay transport paths. These are the two most consequential unknowns. In particular, **a Steam-distributed executable working in Direct mode with Steam unavailable is a testable requirement, not something an OSS fallback setting proves**.

## Read the investigation

| Document | Contents |
|---|---|
| [EIK source audit](EIK_SOURCE_AUDIT.md) | Supplied version/modules; 30+ capability families; detailed login, lobby, session, invite and voice traces; Actor dependencies; static caveats |
| [CkFoundation source audit](CK_FOUNDATION_AUDIT.md) | Current dependency scan; existing registry/processor/request/signal/GC/replication architecture; reference modules and missing infrastructure |
| [Steam and Direct Network research](STEAM_AND_DIRECT_NETWORK_RESEARCH.md) | Official Valve/Epic/Tailscale sources; login/invite/transport flows; direct-network discovery and setup limits |
| [EOS service boundaries](EOS_SERVICE_BOUNDARIES.md) | EIK vs EOS vs EGS; Auth/Connect/accounts; social/overlay and future cross-store implications |
| [Architecture proposal](ARCHITECTURE_PROPOSAL.md) | Module boundaries, entity/fragment model, provider contracts, callback/completion lifecycle, gameplay handoff, Direct protocol and turn-key recipe |
| [Migration and validation](MIGRATION_AND_VALIDATION.md) | MVP scope, consumer ledger, decisions, staged delivery and concrete acceptance matrix |

The source audits are evidence snapshots. The architecture and migration documents are proposals, not current APIs or approved work orders. Read the recommendation here first, then the architecture; use the audits to challenge its premises.

## What the supplied EIK actually offers

The archive at `F:\EOSIntegrationKit-main (1)\EOSIntegrationKit-main` identifies itself as **EIK 4.85, targeting UE 5.6**, with nine plugin modules. It is the OnlineSubsystem-based implementation. Current EIK web documentation describes newer `EIKCore` APIs absent from this archive; those were not attributed to the inspected code.

Its implemented integration spans:

- Epic Auth/Connect, several credential types, device identity, persistent login, refresh/logout and account-link primitives;
- EOS lobbies and sessions, filtered discovery, member/lobby metadata, invitations, ownership changes and lobby voice;
- Epic friends, friendship requests, presence, user information and social-overlay entry;
- EOS P2P sockets and Unreal NetDriver/connection adaptation;
- RTC voice, raw RTC data and P2P packet wrappers;
- achievements, stats, leaderboards, player/title storage, Epic commerce, reports, sanctions and anti-cheat wrappers;
- selected HTTP APIs, Google/Discord login helpers and editor/setup tools.

The detailed matrix distinguishes actual provider calls, helper conventions, explicit stubs and unverified behavior. EIK does not host these backend services itself.

### Important limits

1. **Full text chat is not supplied by its generic OSS interface.** Chat/Message getters return null, although RTC data and P2P packets provide ingredients for custom chat. Lobby text, history, delivery semantics and UI remain a product feature to build.
2. **“Host migration” is lobby owner handling and address glue.** No authoritative game-state transfer/recovery path was found in the inspected migration code. It cannot be treated as seamless gameplay migration.
3. **Login success, lobby join and game connection are different milestones.** Some high-level join helpers report success immediately after requesting `ClientTravel`; that does not verify admission or world readiness.
4. **Core EIK services are already mostly plain C++/subsystems/UObjects.** Actors enter chiefly through UE travel/networking, beacon ping, anti-cheat connection helpers and positional voice. A Ck design should remove Actor ownership from online feature state while keeping necessary engine bridges small.
5. **The archive is incomplete as a build package.** Its main EOS SDK payload is absent, so its exact SDK version and build compatibility are unknown. The README says MIT while the LICENSE points at Unreal EULA; resolve permissions before any source copying.

These are source observations. Static concerns in the detailed audit were not reproduced at runtime and are not a blanket quality verdict on EIK.

## What already exists in CkFoundation

The source audit identified reusable pieces:

- `FEcsWorld` and generational registry/handle ownership;
- the feature quartet, typed handles, deferred requests, generic completion and Ck signals;
- `CkRecord` and normal entity composition/lifetime patterns;
- `CkGameSession`'s GameInstance-owned private signal registry;
- `CkLoadingScreen`'s non-Actor GameInstance tick-owner pattern;
- registry-based processor graph construction and scheduler tick;
- gameplay replication in `CkEcs/Net`, ActorRelay, and provider-independent `CkVoiceChat`.

There is no discovered Foundation online login/lobby/friends provider. `CkNet` is not a standalone online-services module. `CkGameSession` currently forwards UE player login/logout; its module description overstates its implemented online scope. `CkMessaging` is in-process messaging, and `CkVoiceChat` is gameplay voice, not an already-working pre-game social backend.

Normal gameplay registries die with the world. `FEcsWorld` does not run processors merely because a subsystem owns it. Existing lifecycle processor dependencies also reach into gameplay/Actor groups; the proposed service graph must handle that explicitly. Those findings motivate the early hosting spike.

## Steam, Tailscale and the real turn-key boundary

| Player path | What Ck can make turn-key | What remains external or additional scope |
|---|---|---|
| Steam-native | Platform identity, friends/presence, host/find/join, warm/cold invites, text, connection flow, reusable UI | App ID/licensing/depot setup; compatible transport and installed-build validation |
| LAN/Direct/Tailscale | Local identity, hosted lobby, readiness/text, address or shared descriptor join, game connection | Reachable/authorized network, firewall rules as needed, Tailscale enrollment/sharing; no built-in global social backend |
| EOS/EGS later | Same Ck-facing features through a tested provider; cross-store account/social flow | Epic product/deployment/policy, account decisions, compatible shared matchmaking/transport and release requirements |

Steam lobbies and their invitations are supported by Valve services, while gameplay networking remains separate. [Valve lobby documentation](https://partner.steamgames.com/doc/features/multiplayer/matchmaking?l=english).

Tailscale makes a private game endpoint reachable and provides encrypted network connectivity. It does not supply the game's lobby database, friend graph, chat service or offline invite inbox. Its documented game-server journey still requires installation/sign-in, accepted sharing and a game address/port. [Tailscale game-server guide](https://tailscale.com/docs/use-cases/personal-or-at-home-use/share-private-game-server).

For Direct, recommend **shareable join descriptors plus recent/bookmarked hosts** initially. Do not depend on LAN broadcast discovery across Tailscale. A globally resolvable short code needs a directory; a self-contained link can encode the destination without one. Persistent friends/offline invites would require a separate service and should be a deliberate product decision.

“Almost no configuration per game” is achievable with a profile, a bootstrap recipe, reusable UI and good diagnostics. “No platform identity/network setup anywhere” is not a realistic promise.

## Core architecture contracts

- Online users/lobbies/members/invites/channels are entity features; provider SDK types stay inside adapters.
- Persistent service entities live outside the map's gameplay registry. World projections are rebuilt from stable domain IDs after travel.
- Provider callbacks copy value data into a bounded inbox. Only owning-thread processors mutate ECS state.
- Every request has one canonical operation record, a deadline and an explicit terminal outcome. Starting an SDK call is not successful completion.
- Claim/remove terminal operation ownership **before** invoking listeners; the current generic completion method executes before unbinding and can otherwise be re-entered.
- Logical shutdown can happen immediately; physical destruction waits until service ticks and callback/signal dispatch unwind. Never free the registry from inside a listener currently using it.
- Register platform invite notifications immediately when the provider exists; buffer intents through login/menu readiness independently of friends queries.
- Keep provider identity, lobby membership, connection admission and replication readiness distinct.
- Preserve the present CK ensure/GC/naming/AS contracts; do not blindly copy older neighbor patterns that conflict with current doctrine.

The proposal contains a module table, ownership diagram, request pipeline and concrete login/join sequences. Its default Actor-free claim is limited to the new online domain.

## Decisions still needed for implementation

The following are recommendations, not assumed approvals:

1. **Direct social scope:** shareable joins now, or an always-on friends/invite backend?
2. **Chat launch scope:** text first, or lobby/game voice as a launch requirement?
3. **Gameplay envelope:** first game's player count, listen vs dedicated server, supported operating systems and reconnect expectations.
4. **Host loss:** lobby/rehost recovery first, or seamless gameplay migration as an explicit feature?
5. **Cross-store timing:** keep EOS optional until needed, or bring it forward to establish a shared Steam/EGS population earlier?

The first two were raised during the investigation; this snapshot records the recommended text-first/shareable-join assumptions pending an answer. These do not prevent using the research to make a decision.

## Evidence and verification record

| Item | Result |
|---|---|
| Host source snapshot | Root HEAD `c5fc5f7f05a8d693ea12caa4b13d0bb9487a75b8`; Foundation HEAD `6d7019e53d6e79a087c747ecddce8e9074b87d57`; existing dirt preserved |
| Current EIK dependency in this host | Descriptor entry is **disabled**; no populated project EIK source found; no direct Foundation EIK/OSS consumers in the documented text scan |
| Limits of that absence claim | Binary assets, associated engine, other games and unpopulated submodule contents were not audited; no global removal claim |
| EIK source | Descriptor/build rules and concrete feature implementations inspected; detailed file/line evidence in audit |
| Primary external references | Official Valve, Epic/Betide and Tailscale sources; retrieval date 2026-10-02; version drift documented |
| Design review | Independent provider review and CK lifecycle/completion review; findings reconciled in the proposal |
| Lead spot-checks | Disabled plugin entry, GameSession/FEcsWorld ownership, request/signal/scheduler re-entry, EIK chat stubs, join completion, lobby member settings, credential-parser branches and host-promotion code |
| Document checks | Seven files; all 17 local Markdown links resolve; fenced blocks balanced; no merge markers or trailing whitespace |
| Runtime/build evidence | **None. Zero Unreal builds, editor boots, packaging runs or live provider sessions.** |
| Original investigation changes | Research documents only; no dependency/config/source removal, commit or push during the investigation. Subsequent publication packaging is described below. |
| Storage | `docs/research/online-services/` relative to the repository root. The original machine used `D:\Repos\CkPlugins`; remap this prefix on another machine. |

Most uncertain design premise: the proposed narrow persistent service graph can reuse enough existing Ck lifecycle machinery without a larger hosting refactor. Source APIs make it plausible; the first architecture spike must prove it. Separately, Steam/Direct same-executable startup and driver behavior require actual selected-engine validation.

This snapshot is complete as an investigation. Before implementation, turn the accepted decisions into a small staged plan using the observable gates in [Migration and validation](MIGRATION_AND_VALIDATION.md), refresh live source/config state, and retain the evidence boundaries above.

## Temporary documentation delivery (2026-10-02)

The requested transfer branch is `feature/online-services-research` in `chainkemists/CkPlugins`. The seven research documents form one temporary docs-only commit; the detailed `CONTINUATION_PROMPT_OnlineServices.md` beside this index forms a separate temporary docs-only commit. Both commit subjects contain **`[DROP BEFORE MERGE]`** and both bodies carry **`Drop-Before-Merge: true`**. Preserve useful accepted decisions in permanent documentation before dropping these temporary commits; keep future implementation commits separate. Do not merge this research-only branch into the integration branch as a completed implementation.

The existing `docs/` ignore rule remains unchanged. Only the eight named Markdown files are explicitly tracked on the transfer branch. The source machine's active checkout stays on its original `dev`, with its normal index and unrelated work preserved; a separate temporary Git index builds the documentation branch directly from the fetched integration tip without another checkout.

The publication base is `origin/dev` at `c14c30816a4f77b52fbef7ff0d83a7cae609509d`. At preparation, it diverged from the source machine's local `dev` by 12 remote-only and 21 local-only commits. Those 21 unrelated local commits are not part of the documentation branch. No merge or replay of that history is authorized by this handoff.

**A fresh checkout of the transfer branch does not recreate the audited source snapshot:**

| Revision | Value |
|---|---|
| Root examined during research | `c5fc5f7f05a8d693ea12caa4b13d0bb9487a75b8` |
| Foundation gitlink in that research root | `3a52cf6bc8c947db23a43e1041c255a2405fc098` |
| Foundation actually inspected, plus existing local dirt | `6d7019e53d6e79a087c747ecddce8e9074b87d57` |
| Foundation gitlink carried unchanged from the publication base | `3d8660c27e33c92c0d6ec5c6b827b5eafc3317a8` |

The source machine also has unpublished root/Foundation guidance and submodule work. This docs-only transfer deliberately does not publish it. In particular, Foundation `DEVELOPMENT.md`, `AGENTS.md`, and `Source/ARCHITECTURE.md` used by the audit were local untracked files. Read the continuation prompt's portable rules and source-reconciliation steps before implementing; do not silently move submodule pointers to make old line references match. The EIK archive on `F:` and its missing SDK are not included either.

This section records the intended commit structure and its source basis. The continuation prompt records the research commit identity; verify the final published branch tip from Git before resuming. Publication does not approve the proposed architecture or change the zero-runtime-evidence boundary.
