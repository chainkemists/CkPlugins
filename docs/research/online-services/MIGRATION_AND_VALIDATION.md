# Replacement scope, migration and validation

Date: 2026-10-02. Status: proposed follow-on work; **no implementation phase has started**. Architecture: [ARCHITECTURE_PROPOSAL.md](ARCHITECTURE_PROPOSAL.md).

## 1. Scope to replace first

The goal is a reusable game experience, not one-to-one parity with every EIK wrapper.

| Capability | Proposed first replacement | Later / separate commitment |
|---|---|---|
| Login | Steam identity and explicit local Direct profile; clear logout/account-change handling | EOS Auth/Connect, linking, refresh, platform-specific authentication |
| Lobbies | Create/search/join/leave, roster, metadata, ready state, permissions, host endpoint | Advanced matchmaking, persistent parties, allocation/backfill |
| Invites | Steam overlay/native invite plus warm/cold acceptance; Direct shareable descriptor | Persistent cross-provider friends and offline inbox |
| Presence | Steam joinability; Direct reachable-host status | Unified cross-store presence |
| Text | Lobby and game text with bounded history and failure handling | Durable DMs, cross-room messaging, hosted moderation/history |
| Voice | Integrate existing CkVoiceChat as a separately verified gameplay capability | Pre-game party voice/provider RTC unless launch scope requires it |
| Gameplay networking | UE Steam transport and IP transport through explicit tested profiles | New replication protocol, arbitrary mixed transports |
| Host loss | Typed disconnect and lobby/rehost flow | Seamless authoritative-game migration |
| Extra EIK services | Preserve existing consumer behavior until separately migrated | Achievements, stats, leaderboards, storage, commerce, anti-cheat, reports, sanctions, Discord/mobile |

If an actual consumer uses an item in the last row, it becomes required migration scope before that consumer can remove EIK. “Deferred for the framework MVP” does not authorize breaking it.

## 2. Actual starting point and migration inventory

Source inspection found EIK **disabled** in this host's `CkPlugins.uproject:131-132`, no initialized EIK plugin directory in the selected project, and no direct Foundation EIK/OSS implementation usage in the audited text sources. Some plugin submodule contents are unavailable, binary asset references were not inspected, and the engine installation/other game projects were not audited. Therefore this research cannot identify every production consumer or claim the dependency is already removable everywhere.

The supplied EIK 4.85 archive is reference material, not proof of the version every game ships. It declares UE 5.6; this host targets a different fork/version. Capture the installed version and settings in each actual consumer before migration.

Build a migration ledger per selected game:

| Item | Required evidence |
|---|---|
| Plugin origin/version | Project plugin, engine plugin or vendored copy; descriptor/version and SDK payload |
| Build dependencies | `.uproject`, `.uplugin`, `.Build.cs`, target/platform config and provider SDK linkage |
| C++/AS API consumers | Login, lobby, session, social, voice, stats/storage/etc. symbols and their callers |
| Blueprint/asset consumers | Referenced async node classes, EIK structures/enums, serialized defaults and object paths |
| Config/runtime behavior | Default/native OSS, NetDriver/connection classes, packet handlers, voice provider, artifact selection, launch flags |
| Startup and external entry | Auto-login, Steam/Epic startup invitations, installed URL scheme, menu readiness |
| Identity/data continuity | Existing PUID/account links, cloud data, stats/achievement ownership, environment identifiers |
| Build-machine staging | Plugin/SDK DLLs, redistributables, licenses, packaging exclusions, installer prerequisites |
| Expected user behavior | What currently works, documented failures, first-login prompts, invite/host/join acceptance |

Do not copy secrets into the ledger. Record their location/policy and whether needed; redact logs and join capabilities.

Do not initialize unavailable submodules under a new root or create a clone/worktree for this audit. Work in explicitly selected existing checkouts, preserve unrelated dirt, and coordinate any in-place build/rebase requirements separately.

## 3. Architecture decisions before implementation

| Decision | Proposed default | Why it changes implementation |
|---|---|---|
| Native Steam and Direct now; EOS later | Yes | Keeps first backend/SDK requirement small; EOS later must still meet cross-store needs |
| Non-Steam invitations | Shareable link/descriptor to a reachable host | Persistent contacts/offline invites require identity, directory and delivery infrastructure |
| Chat scope | Text first | Voice adds devices, audio routing, transport and separate acceptance |
| Gameplay topology | Listen-server co-op first | Dedicated-server allocation, trust and lifecycle are different |
| Supported launch platforms | Windows first, portable boundaries | Linux/Steam Deck/macOS need their own binaries, overlay and audio/network validation |
| Actor elimination boundary | New online domain actor-free; retain UE networking boundary | Replacing all gameplay replication is far larger scope |
| Persistent service registry | Curated non-Actor processor host | Requires lifecycle/graph spike before feature work |
| Profile switching | Explicit, disconnected boundary | Exact same-process support vs relaunch must be measured |
| Direct trust/admission | Private reachable network + expiring invitation/host approval | Public Internet support needs a stronger channel and threat model |
| Player capacity | Set by first game, measured | Avoid selecting limits from EIK/Steam marketing or inventing a benchmark |

Unanswered product questions remain proposals, not silently approved decisions. Research can finish without those decisions; implementation should turn accepted decisions into a short ADR and phase contracts.

## 4. Proposed delivery sequence and observable gates

No date or duration estimate is asserted. The first two spikes materially affect effort and should precede a schedule.

### A. Establish consumer scope and provider compatibility

Work: inventory the first consuming game, selected engine's actual Steam/OSS/Online Services interfaces, current driver classes and SDK versions. Check all required provider capabilities and optional-build packaging.

Gate: one capability ledger maps every currently used EIK feature to keep/replace/defer-with-consumer-agreement; one engine ledger names supported interfaces and explicit gaps. No credentials in artifacts. Resolve the archive README/license discrepancy before any source reuse; prefer independently authored Ck adapters over copying EIK implementation.

### B. Prove persistent actor-free service processing

Work: GI-owned FEcsWorld with a curated processor graph, non-Actor tick owner, fake adapter, value inbox, operation correlation, cancellation and deterministic shutdown. Reuse CkEcs graph construction where possible; do not load all gameplay descriptors into the service registry.

Gate: two fake operations complete through Utils/processors/signals, survive world teardown, reject stale callbacks, and cancel once on service shutdown; no scheduler Actor is created for this domain. Exercise a signal listener and a generic-completion listener that request shutdown/profile switch/owner destruction during dispatch: terminal ownership is claimed before notification, results do not fire twice, and physical registry/scheduler/provider destruction waits until active stacks unwind. Prove all required destruction/group dependencies are present, no `LoadKernel` misuse, no foreign world lifetime children, no global processor double execution. C++/BP/AS consumers observe consistent outcomes.

This is the architecture stop/go gate. Do not implement every feature while persistent ownership remains unproven.

### C. Prove actual gameplay transports early

Work: narrow Steam-native and Direct connection paths under the selected engine. Separate provider service success from NetDriver connectivity and admission.

Gate: two machines/two licensed Steam accounts connect with the project's App ID; two Direct clients connect over IP. The intended Steam-distributed executable connects in Direct mode with Steam unavailable, selecting the IP driver before connection. Measure whether switching requires process restart, and resolve any startup Steam relaunch/subscription policy. A SteamSockets-to-IP mismatch fails clearly rather than falling back invisibly.

Backend fake tests cannot pass this gate. Editor-only 480 tests cannot pass it either.

### D. Add the common ECS domain and Direct vertical slice

Work: user/lobby/member/chat/connection features, direct control protocol, explicit endpoints, reliable membership/text, game readiness, deadlines and structured errors. One tiny consumer recipe and basic UI path.

Gate: two running clients create/join/leave a lobby, change readiness, exchange text, enter gameplay and return to the lobby without corrupting persistent identity. Denied/expired invitations and malformed input cause no partial admission or game side effect. Host loss converges to the selected rehost/disconnect state.

Prove ordinary LAN/unicast behavior before adding Tailscale as a network variable. Include a Direct-only build/staging check with Steam/EOS/EIK absent.

### E. Add Steam services and social entry points

Work: Steam user, friends/presence, lobby search/membership, invite UI and normalization, lobby text if using native messaging; integrate selected game transport.

Gate: warm/cold lobby invites and rich-presence joins all reach the same correlated join pipeline. No duplicate OSS/native joins; cancellation during login, full lobby, account change and invite during an existing game return correct results. Two different games/configurations can use the recipe without provider-specific game code.

### F. Make setup and UI turn-key

Work: optional Ck UI, profiles, editor validator/config preview, diagnostic support view, clean provider-selection UX. Put reusable game flow in Foundation, leave map/mode and player-spawn rules to the game.

Gate: a small second consumer integrates using profile values and the public recipe. Record the actual game-specific code/config it required. Missing App ID/provider/port/map produces actionable diagnostics. Setup is deterministic and does not overwrite unrelated settings. Validate AS optional-on/off builds and Blueprint-facing use.

### G. Migrate actual consumers and remove EIK

Work: explicit feature-by-feature consumer migration, Blueprint references, config/provider replacement, SDK staging cleanup, documentation and submodule pointer work only when separately authorized for delivery.

Gate: no active EIK references in consumer source/assets/config/staging; no EIK module or SDK copy loaded/staged; expected login/invite/host/join/chat flows observed in the final build. Verify data/identity continuity where EOS data remains. Archive the evidence and rollback strategy, not another repository checkout.

The disabled host descriptor entry can be removed in the eventual scoped cleanup, but that edit alone proves nothing about downstream removal.

### H. Optional EOS/EGS and expanded scope

Work: EOS adapter, Auth/Connect separation, deliberate account linking, supported Epic social and overlay behavior, shared cross-store lobby/connection model, required store features. Add dedicated server directory/allocation, voice or seamless host migration only as independently specified workstreams.

Gate: Steam and EGS clients actually share a lobby, invite/join each other and play through a compatible transport/authentication path. A Steam lobby provider next to an EOS lobby provider does not automatically unify matchmaking. The EGS distribution requirements make PC crossplay a release concern; verify the then-current requirements and full store/user journey before that release. [Epic distribution agreement](https://cdn2.unrealengine.com/egs-distribution-agreement-831c3b1b8ec4.pdf), [Epic crossplay technical overview](https://dev.epicgames.com/docs/epic-online-services/accounts-and-social/crossplay/crossplay-technical-overview).

## 5. Minimum behavior matrix

These are proposed acceptance observations, not tests already run.

| Area | Required cases | Observable success |
|---|---|---|
| Login | Success, unavailable provider, user cancels UI, expired/revoked token, account switch | Terminal result once; truthful state; no old-user callbacks applied |
| Async operation | Immediate/delayed/duplicate/late callback, timeout, cancel, re-entry | One canonical completion; no leaked remote create/join; bounded pending work |
| Registry/lifetime | World destroyed, travel, GI shutdown, owner destruction, two PIE instances | No stale/foreign registry access; notifications removed; resources released |
| Lobby mutation | Full room, denied invite, invalid metadata, simultaneous leave/join, owner loss | Authoritative membership/capacity; no successful partial transaction |
| Search | No results, stale result, superseded search, incompatible build | Empty success distinct from failure; join revalidates |
| Invitations | Warm/cold platform invite, duplicate, expired, other profile, active-game conflict | One validated intent; user-visible choice where existing game is displaced |
| Connection | Backend join succeeds but transport fails; wrong driver; rejected auth; failed travel | No false ready state; rollback and bounded recovery |
| Startup/profile | Steam open/closed/unavailable, direct selection, provider absent from build | Correct actual driver; no forced Steam dependency for promised Direct mode |
| Direct protocol | Bad length/type/version, address, room epoch, replayed capability, rate flood | Bounded parsing/queue/memory; no unauthorized mutation or admission |
| Text | New channel, mute, unauthorized sender, oversize/flood, duplicate, travel | Sender derived from membership; bounded history and honest delivery status |
| Host failure | Room owner leaves, process terminates in-game, reconnect | Distinguish room leader change from authority migration; selected recovery works |
| Tailscale | Same-tailnet, node share, denied route, DNS failure, relay fallback | Reachability errors diagnosed; no requirement for broadcast discovery |
| Public surfaces | C++, Blueprint, AS; AS disabled | Same operation semantics; no provider types exposed in public payloads |
| Packaging | Direct-only, Steam+Direct, later EOS | Correct optional dependency/staging; actual installed launch/invite path works |
| Voice, if claimed | Real two-machine audio, devices/PTT/mute, lobby/game transition | Audible expected behavior and cleanup, not merely captured buffers |

In addition, check cancellation/result consumption is visible to the consumer. Emitting a failure signal with no bound UI is not a complete user-facing failure path. Provide readable status, operation ID, provider/profile, state duration and sanitized error category; never log tickets or complete join tokens.

## 6. Verification and build discipline

This investigation used **zero editor boots and zero builds**. Only research documents were authored. Source inspection establishes implementation shape, not working network behavior.

Future C++/AS verification uses `CkAuto/UnrealToolbox.exe` with the absolute project path and detached GUI launch conventions. Each implementation stage must state the planned editor-boot count and purpose before launching. Reuse current binaries for AS-only work; C++/module work needs an incremental build. Do not create alternate checkouts or clear Binaries/Intermediate/DDC as a testing shortcut.

Runtime feature proof needs real multi-process/multi-machine sessions with the relevant accounts/network paths. Provider-contract tests with fakes are necessary for adversarial ordering and failures, but cannot verify Steam overlay, relay selection, voice, installed cold-launch or store configuration.

Cooking/packaging/staging/archiving remain build-machine work. Shipping requires explicit permission. Define release-build evidence on that machine rather than locally packaging to satisfy this document. Use final logs plus actual process exit/artifacts; inspect fresh logs for ensures, script errors, transport and provider failures.

## 7. Rollout and rollback

Migrate one complete consumer path at a time behind an explicit project profile. Keep the old integration usable until the replacement's required behavior is accepted; avoid initializing two competing providers for the same session while comparing them. Rollback means an in-place reviewed config/commit change, not silently remapping users into a new identity namespace.

Do not delete existing EOS data/accounts as part of removing EIK. EIK is an integration layer, so retaining the same EOS product/deployment and verified PUID mapping can be independent of removing its plugin. Native Steam-only operation may intentionally stop using EOS services; decide what happens to existing cloud data, achievements or account links before that consumer switches.

Only remove leftover SDK binaries/licenses/config when the ownership and absence of other users are proven. No deletion, commit, rebase, push or dependency removal occurred in this research.
