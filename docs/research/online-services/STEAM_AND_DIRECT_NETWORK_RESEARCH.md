# Steam and direct-network research

Retrieved: **2026-10-02**. Scope: official Valve, Epic, and Tailscale documentation for a proposed CkFoundation multiplayer integration. This is research and design guidance, not an implementation or runtime compatibility claim. No code, build, engine inspection, repository checkout, or Git operation was performed. EOS research and the final framework architecture belong to the main investigation.

## Decisive boundaries

1. A lobby/session service organizes players and advertises a destination; the gameplay connection is a separate operation. Valve explicitly separates lobbies from networking. Unreal likewise separates a successful session join from resolving a connection string and travelling. [Valve multiplayer](https://partner.steamgames.com/doc/features/multiplayer?language=english), [Epic session interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-session-interface-in-unreal-engine).
2. Steam lobby ownership automatically transfers when its owner leaves. This is **not evidence of gameplay authority migration**, world reconstruction, or connection continuity. [Valve ISteamMatchmaking, GetLobbyOwner](https://partner.steamgames.com/doc/api/ISteamMatchmaking?l=english#GetLobbyOwner).
3. Tailscale supplies encrypted network connectivity and access policy. A host endpoint becomes reachable; the game still owns room state, player admission, chat, discovery, and any account authentication. Its official game-server guide still ends with the player entering an address and game port. [Tailscale private game server](https://tailscale.com/docs/use-cases/personal-or-at-home-use/share-private-game-server).
4. Prefer an OSS Steam adapter as the initial Unreal-facing Steam integration candidate, with targeted native Steamworks extensions when coverage is missing. Treat newer Online Services compatibility as a measured spike: its UE 5.7 overview is still Beta and retains the shipping caution. A `Steam` provider identifier is not proof of complete implemented interface coverage. [Epic Online Services overview, UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-online-services-in-unreal-engine?application_version=5.7).

## Capability matrix

“Game-owned” means proposed framework behavior, not a feature supplied by Tailscale or raw sockets. Exact OSS Steam coverage in the project's 5.7.4 fork remains unverified.

| Capability | Native Steamworks | Unreal OSS Steam integration candidate | Direct IP / LAN / Tailscale profile |
|---|---|---|---|
| Local identity | Signed-in Steam account / SteamID | OSS identity; validate actual login behavior | Local guest/profile by default; separate verified account adapter if needed |
| Lobby and member state | Backend lobby, metadata, membership | Lobby-backed session settings; confirm mapping | Game-owned room on listen host or dedicated server |
| Discovery / matchmaking | Filtered Steam lobby search; game-server APIs | OSS search/find/join | Address entry, bookmarks, configured unicast candidates; optional separately hosted directory |
| Friends / blocked users | Steam social graph | Friends interfaces / external UI; scope by implemented capabilities | Local bookmarks/contact labels; overlay peers are devices, not a game social graph |
| Presence / joinability | Steam rich presence and friend game info | Presence/session adapter; verify publishing | Probe configured game hosts; no global game presence service |
| Invite | Steam lobby or rich-presence invite | OSS accepted-invite delegate; verify cold launch | Copy descriptor/link; game/OS launch routing must be implemented |
| Lobby text | Native lobby text/binary messages | Native extension if OSS chat lacks required coverage | Reliable game-owned room messages after connection |
| Gameplay text | Game protocol or networking messages | Game protocol over selected UE driver | Same game protocol |
| Voice | Steam capture/compression/decompression; transport still required | Advertised voice interface; prove channel lifecycle and transmission | Optional separate voice implementation; Tailscale does not supply voice rooms |
| Gameplay transport | Modern Steam Networking Sockets/Messages, SDR where eligible | Steam Sockets plugin/driver candidate | UE IP transport candidate over reachable unicast endpoint |
| Player authentication at authority | Session tickets / server-side validation | SteamAuth packet handler candidate | Guest admission or separate credential verification |
| Cross-store / console reach | SDR access outside Steam has additional conditions | Steam Sockets docs limit supported cross-platform devices | Transport interoperability requires compatible game builds and platform network permissions |
| Lobby leader transfer | Automatic / explicit ownership transfer | Verify mapped events and refresh | Game-owned policy |
| Active authority migration | Game-owned | Game-owned | Game-owned |

Native capability evidence: [Steam lobbies](https://partner.steamgames.com/doc/features/multiplayer/matchmaking?l=english), [Steam Friends](https://partner.steamgames.com/doc/api/ISteamFriends?l=english), [Steam Voice](https://partner.steamgames.com/doc/features/voice?l=english), [Steam Networking](https://partner.steamgames.com/doc/features/multiplayer/networking?l=english). Unreal advertised Steam interfaces: [OSS Steam, UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine?application_version=5.7). Direct-network column is proposed product scope grounded in the connectivity boundaries below.

## Steam integration layers and setup

### Native Steamworks and Unreal wrappers

The native SDK is the reference capability surface. An Unreal wrapper exposes a subset through engine interfaces; existence of an abstract getter does not prove its provider returns a usable implementation. For example, an `IOnlineChat` API declaration alone is insufficient evidence that OSS Steam implements the required lobby-chat workflow. The initial spike must check returned interfaces and behavior; native lobby chat is the documented fallback candidate. [Epic IOnlineSubsystem API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/OnlineSubsystem/IOnlineSubsystem?lang=en-US).

Newer Online Services intends to replace OSS eventually. The documented OSS Adapter wraps Online Subsystem implementations through the newer interfaces. Current public adapter API lists sessions, social, presence, auth, and other classes; it does not establish a full Steam-native lobbies or chat implementation. Avoid assuming that selecting `DefaultServices=Steam` provides parity. Exact plugin availability, adapter configuration, and capability coverage require inspection of the selected engine and a live spike. [Epic subsystem/service overview](https://dev.epicgames.com/documentation/unreal-engine/online-subsystems-and-services-in-unreal-engine), [OSS Adapter plugin](https://dev.epicgames.com/documentation/unreal-engine/API/PluginIndex/OnlineServicesOSSAdapter?lang=en-US), [OSS Adapter API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/OnlineServicesOSSAdapter?lang=en-US). The API pages currently default to 5.8; they are orientation, not proof of this fork's contents.

### Credentials, clients, and application identity

Steam initialization requires a running Steam client, known AppID, matching OS user context, account ownership/license, and adequately configured app/packages. Development may use `steam_appid.txt`; it must not ship in depots. Steam launch supplies the AppID. `SteamAPI_RestartAppIfNecessary` may relaunch the installed Steam build, so it must be considered carefully in debugging and shipping flows. In an OSS-based integration let the engine/provider own SDK lifecycle rather than initializing another competing Steam API owner. [Valve API initialization](https://partner.steamgames.com/doc/sdk/api?l=english).

Epic documents AppID 480 for development testing, `DefaultPlatformService=Steam`, `bEnabled=true`, and `SteamDevAppId`. For session-based integration it documents `bInitServerOnClient=true`; its lobby path does not require this setting. SteamAuth is enabled separately using the `OnlineSubsystemSteam.SteamAuthComponentModuleInterface` packet handler. Enabling it performs Steam authentication for joining players; default failure behavior kicks them. Binding its failure override suspends that default and makes enforcement the application's responsibility. [Epic OSS Steam setup/authentication](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine?application_version=5.7).

Recommendation: use a project AppID and two separately licensed Steam accounts on two machines for acceptance testing; 480 is an initialization smoke test, not evidence of the project's isolated backend configuration. For unreleased external testing, Valve documents release override keys and Steam Playtest. A Playtest uses an associated but distinct AppID and has the main game's technical Steamworks features. [Valve testing on Steam](https://partner.steamgames.com/doc/store/testing?l=english).

Engine SDK version, redistributable staging, net-driver classes, and packaging behavior must be verified against 5.7.4 before emitting turnkey config. Do not blindly copy an old SDK path/version or unvalidated driver fallback from public examples.

### Gameplay transport and crossplay

Valve's preferred modern APIs are `ISteamNetworkingSockets` and `ISteamNetworkingMessages`; legacy `ISteamNetworking` is deprecated. Modern APIs offer relay/direct options; SDR can conceal public endpoint addresses. A SteamID/FakeIP transport destination is not a general routable direct IP, and a lobby ID is not a gameplay socket address. [Valve networking overview](https://partner.steamgames.com/doc/features/multiplayer/networking?l=english).

Epic's UE 5.7 Steam Sockets documentation says the plugin uses its own net driver, only connects to other Steam Sockets builds, and supports Windows/Mac/Linux rather than all devices. It also warns that non-Steam PC-store builds require appropriate configuration. Therefore do not promise a Steam Sockets client can transparently join an IP/Tailscale host, or swap drivers inside an active connection. [Epic Steam Sockets, UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-steam-sockets-in-unreal-engine?application_version=5.7).

**Product requirement / unresolved spike:** the same Steam-distributed build should offer an explicit Direct profile when Steam is absent or not active. Prove the selected profile's actual driver before creating a gameplay connection. An OSS Null fallback does not prove that the network driver became an IP driver; a Steam-first driver and initialization fallback also do not prove intentional Direct behavior. Determine whether driver selection can safely occur before each new connection in one process, or whether switching profiles requires relaunch/startup selection in this fork. Until measured, document restart behavior as unresolved and expose a provider/profile error rather than silently selecting a fallback driver. No cited official page establishes that no restart is required.

Epic also documents a Shipping subscription check that shuts down on failure. Direct startup must therefore be measured before committing to Steam-required initialization/relaunch; this fork's exact startup policy remains unresolved. [Epic OSS Steam Shipping behavior](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine?application_version=5.7#shippingbuilds).

Valve supports some non-Steam SDR scenarios, but its stated conditions include a Steam-shipping game, timely SDK/security updates, a matchmaking coordinator that can issue credentials, and contacting Valve for SDKs/terms. Access is not guaranteed for every partner/game; the open-source transport library alone does not include access to Valve's relay network. This is a materially different deployment commitment from ordinary Steam-to-Steam P2P. [Valve SDR requirements](https://partner.steamgames.com/doc/features/multiplayer/steamdatagramrelay?l=english#cross_platform).

## Login, host, invite, and join sequences

The following are **recommended orchestrator sequences**, not an implementation claim. Represent completion of service membership, transport establishment, admission/authentication, and world readiness separately. Do not announce “connected” merely because a lobby/session operation succeeded.

### Steam startup and identity

1. Register owned join-intent handlers early enough to capture startup and platform UI events. Buffer intent until local player, provider, and menu/travel coordinator exist.
2. Initialize the selected OSS Steam provider, check local identity, and report a meaningful unavailable state when initialization fails. Never silently reinterpret a Steam identity as a local guest.
3. Native evidence: `GetSteamID` identifies the currently signed-in account; `BLoggedOn` indicates live Steam-server connectivity. Treat temporary connectivity loss as a service-state change rather than automatically disabling every Steam feature. [Valve ISteamUser](https://partner.steamgames.com/doc/api/ISteamUser?l=english#BLoggedOn).
4. Once identity is ready, resolve one queued intent. Preserve provider-specific IDs as typed data, not concatenated travel strings. Cancel or serialize overlapping host/search/join operations.

### Host and search

1. Create a lobby-backed OSS session with explicit visibility, capacity, invites/joinability, compatible build/protocol, and transport profile. Wait for completion before exposing invitations.
2. Publish compact room metadata and member-ready state; keep high-frequency gameplay data off the lobby service.
3. Host world/listen endpoint readiness must precede advertising a ready gameplay destination. Keep room state separate from destination readiness.
4. Search using compatible build/protocol/game-mode filters, then validate the selected result again before joining. Native Steam lobby search is backend discovery, not a measured network-ping or skill/ranking system supplied automatically by the game framework. [Valve lobby workflow](https://partner.steamgames.com/doc/features/multiplayer/matchmaking?l=english).

### Invites: distinct warm and cold paths

| Steam operation | Already-running recipient | Recipient starts from invite |
|---|---|---|
| `InviteUserToLobby` / overlay lobby invite | `GameLobbyJoinRequested_t` with lobby SteamID | `+connect_lobby <64-bit lobby Steam ID>` |
| `InviteUserToGame` / rich-presence game join | `GameRichPresenceJoinRequested_t` with connect string | Connect string supplied through launch path |

Valve distinguishes these callbacks explicitly. Rich presence `connect` enables Join Game; `steam_display` controls localized rich-presence display. A friend being online does not establish a valid game destination. [Valve Friends API](https://partner.steamgames.com/doc/api/ISteamFriends?l=english#GameLobbyJoinRequested_t), [enhanced rich presence](https://partner.steamgames.com/doc/features/enhancedrichpresence?l=english).

Implement `ISteamApps::GetLaunchCommandLine` where required and configure Installation > General > Use launch command line for the safer rich-presence route documented by Valve. `NewUrlLaunchParameters_t` allows new URL parameters to be queried while running. Parse supported tokens into a validated join intent; never execute arbitrary launch text as a console command. The exact OSS handling of these paths is an engine compatibility spike, particularly cold startup before delegates are registered. [Valve launch parameter API](https://partner.steamgames.com/doc/api/ISteamApps?l=english#GetLaunchCommandLine).

### Resolve membership, travel, and admission

1. OSS accepted invite: consume `OnSessionUserInviteAccepted`'s search result. Native lobby extension: consume the lobby ID, request fresh data when needed, and join via the chosen integration owner. Do not let native and OSS paths independently create two joins.
2. Call `IOnlineSession::JoinSession`; wait for `OnJoinSessionComplete`. On success resolve `GetResolvedConnectString`, then use client travel to the intended server. A successful join call is not a successful world connection. [Epic OSS session lifecycle](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-session-interface-in-unreal-engine), [resolved connection API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/OnlineSubsystem/IOnlineSession/GetResolvedConnectString).
3. Admission verifies room generation, compatible protocol/build, capacity, and selected credential policy before making a player active. Model transport failure and travel failure independently, rolling back provisional membership where appropriate.
4. For native ticket auth, create a session ticket, send it to the authority, run `BeginAuthSession`, and wait for successful `ValidateAuthTicketResponse_t`; immediate ticket validation is not final identity verification. Use fresh tickets for each verifier and clean up with `CancelAuthTicket` / `EndAuthSession`. Dedicated servers use the corresponding server API. [Valve authentication flow](https://partner.steamgames.com/doc/features/auth?l=english), [Valve ticket API](https://partner.steamgames.com/doc/api/ISteamUser?l=english#BeginAuthSession).
5. Source nuance: Valve says a Steam lobby member is authenticated to the Steam backend. That does not remove the authority's obligation to validate a separately arriving game connection and admission policy. [Valve lobby authentication](https://partner.steamgames.com/doc/features/multiplayer/matchmaking?l=english#authentication).

### Text and voice

Native lobby text/binary messages use `SendLobbyChatMsg` and `LobbyChatMsg_t` / `GetLobbyChatEntry`. Payloads are limited to 4 KiB and backend bandwidth is limited; voice/game data belongs on networking. Treat message bytes as untrusted and apply game-owned length/rate/type rules. [Valve lobby chat API](https://partner.steamgames.com/doc/api/ISteamMatchmaking?l=english#SendLobbyChatMsg).

Steam Voice provides recording, compressed data retrieval, and decompression. It does not transmit audio. A complete voice product still needs transport/channel lifecycle, playback, devices, push-to-talk, mute/block, privilege checks, and teardown. Do not infer room voice from native lobby membership or an advertised OSS Voice interface. [Valve Steam Voice](https://partner.steamgames.com/doc/features/voice?l=english).

### Lobby leader versus gameplay host

Steam guarantees one lobby owner and may choose a replacement automatically; explicit transfer uses `SetLobbyOwner` and members receive a lobby data update. Refresh ownership from the service rather than inferring it from roster order. [Valve lobby ownership](https://partner.steamgames.com/doc/api/ISteamMatchmaking?l=english#GetLobbyOwner).

Recommendation: support automatic **room leader** change initially. Default active listen-host loss to a clear disconnect/return-to-room policy. Gameplay migration is a separate feature requiring election, authoritative checkpoint/state transfer, new endpoint advertisement, reconnect/resume, sequence/epoch handling, and a policy preventing two authorities. Existing replication or seamless map travel is not evidence of migration: Epic describes server travel as the current server moving connected clients to a new map. [Epic multiplayer travel](https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine?application_version=5.7).

## Tailscale: what is actually minimal configuration

### Connectivity and naming

Tailscale's normal setup is installing its client, signing in through an identity provider, and adding/authorizing devices. There is still out-of-game setup and policy ownership. A “zero game-backend configuration” claim may be valid for direct networking; “no player setup” is not. [Tailscale quickstart](https://tailscale.com/docs/how-to/quickstart).

MagicDNS registers device names/FQDNs and lets players use a host name instead of its Tailscale IP. It is naming, not game-service discovery. For ordinary host/client nodes both running Tailscale, no subnet router or exit node is needed. Subnet routers are for reaching devices that do not run the client; they add route advertisement, administrative approval/access policy, and platform route handling. Their default SNAT can obscure the original endpoint source, reinforcing that an IP is not an account identity. [MagicDNS](https://tailscale.com/docs/features/magicdns), [subnet routers](https://tailscale.com/docs/features/subnet-routers).

### Broadcast and multicast

Do not depend on ordinary LAN broadcast or mDNS discovery across a tailnet. Official current client metrics document multicast packets being dropped; Tailscale's own Wake-on-LAN explanation establishes its Layer 3 boundary and explains why Layer 2 wake packets do not cross even with subnet routing. This supports choosing explicit unicast discovery for the framework. A dedicated tunnel/bridge or protocol reflector would be additional infrastructure, not minimal Tailscale setup. These sources do not constitute a runtime test of Unreal's particular LAN beacon implementation. [Tailscale dropped-packet metrics](https://tailscale.com/docs/reference/tailscale-client-metrics), [Tailscale Layer 3 explanation](https://tailscale.com/blog/wake-on-lan-tailscale-upsnap).

### NAT, UDP, relay, and firewalls

Current Tailscale connection docs describe three paths: direct UDP, optional tailnet Peer Relay, and DERP. Connections begin relayed and attempt upgrades; blocked UDP or hard NAT can prevent direct connectivity. All paths retain end-to-end WireGuard encryption. DERP can carry traffic when HTTPS is available, but has lower throughput and weaker latency/QoS characteristics than a direct path. Measure the actual pair using `tailscale status` / `tailscale ping`; do not advertise guaranteed direct or low-latency gameplay. [Tailscale connection types](https://tailscale.com/docs/reference/connection-types).

The outer Tailscale tunnel's port is distinct from the game's inner UDP port. Tailscale documents typical outbound TCP 443 for control/DERP, UDP source port 41641 for direct tunnels, and UDP 3478 for STUN. Most deployments do not need manual public-port changes, but difficult environments may. The game host still must bind a reachable interface and its OS firewall and tailnet policy must permit the game protocol/port. [Tailscale firewall requirements](https://tailscale.com/docs/reference/faq/firewall-ports).

### Access control and identity

Policy mechanics are default-deny for traffic not allowed by rules; newly created tailnets are initialized with an allow-all policy. Do not collapse these into the inaccurate blanket claim “new tailnets deny peers.” Grants are the recommended modern policy format. [Tailscale ACL examples](https://tailscale.com/docs/reference/examples/acls), [Tailscale grants examples](https://tailscale.com/docs/reference/examples/grants).

A Tailscale node share allows a recipient to reach the shared host without joining all of the owner's tailnet. Friends still need Tailscale accounts/client setup and must accept the share. Shared nodes are quarantined and cannot freely initiate connections into recipient tailnets. This is useful for host-authoritative direct games, but must not be mistaken for peer-to-peer full mesh access. [Tailscale game server sharing](https://tailscale.com/docs/use-cases/personal-or-at-home-use/share-private-game-server).

**Design inference:** network membership authenticates/authorizes devices and routes at the overlay layer. It does not prove the claimed in-game player ID, game ownership, bans, inventory ownership, or who is using an already-authorized computer. Direct mode should expose a clear guest/session identity posture unless a separate application credential is verified. Do not elevate display names, IPs, MagicDNS names, or locally enumerated peer users into verified game account IDs.

## Recommended direct-mode product and exact join sequence

These are proposed alternatives; none assumes a backend Tailscale game service.

| Option | What player sees | Required work / limitation |
|---|---|---|
| Address / name + port | Join a host directly | Lowest configuration; no global roster/discovery |
| Copy/paste join descriptor | Host shares a structured string | Self-contained address/port/room token needs no resolver backend; validation/versioning required; bearer secrets need expiry/redaction |
| OS game link | Clicking opens game and queues descriptor | Scheme registration/installer/platform integration; no auto-launch from Tailscale itself |
| Bookmarked hosts | Saved hosts with live room status | Unicast probe; offline/stale addresses remain normal |
| LAN discovery | Nearby LAN rooms | Restricted to supported local interfaces; tailnet discovery uses another mechanism |
| Seeded unicast discovery | Rooms on configured hosts | Probe only bounded candidates; no address-space sweeping |
| Directory / rendezvous | Searchable room list and short codes | Separate hosted service, accounts/policy if desired; breaks “no backend” premise |
| Optional tailnet peer integration | Reachable device candidates | Local integration/permissions and platform support spike; a candidate is not an active game or friend |

1. Player selects **Direct**. Create a local session identity, or explicitly authenticate an independent account provider. No implied Steam/EOS login.
2. Host starts a room/listen world with the direct-compatible driver profile. Confirm selected bind address and configured game/control ports are ready.
3. Host exposes a join descriptor containing schema version, direct transport type, address or FQDN, port, compatibility version, room identifier/generation, and optional short-lived admission token. A self-contained token can be encoded and decoded locally without a directory. If it carries admission rights, the host must verify it (for example against its stored random admission secret), enforce expiry/revocation policy, and avoid treating possession as a named account identity. Avoid map names or arbitrary console/travel options supplied by remote input.
4. Recipient establishes the needed network first: same LAN, reachable public server, or authenticated Tailscale node/share. A tailnet grant and OS firewall must admit the chosen protocol/port.
5. Recipient enters/pastes/clicks the descriptor. Queue it if the game is starting. Parse a bounded allowlisted schema, resolve DNS, and probe a game-owned endpoint for room/build compatibility and current readiness. A short code cannot resolve itself without either bundled mapping, a directory, or host address information.
6. Connect through the IP-compatible game transport and perform admission. Make explicit whether the authority accepted a guest or verified a credential. Only then publish joined/ready state.
7. Pre-game room and match text use reliable bounded game messages, routed through the host with sender identity assigned by the authority. These channels exist while connected; offline direct messages and cross-room social chat require another service.
8. On disconnect/host loss, release provisional room membership and show an actionable reason. A bookmark may retain the address; it does not preserve a vanished listen-host world.

Use the same normalized intent/state-machine semantics for Steam and direct mode, but preserve transport/account distinctions. Unreal's client travel connects to a server; server travel is server-owned map travel. Initial direct connectivity should be proven on an ordinary local network before introducing the overlay variable. [Epic travel semantics](https://dev.epicgames.com/documentation/en-us/unreal-engine/travelling-in-multiplayer-in-unreal-engine?application_version=5.7).

## Required compatibility spikes before claiming turnkey behavior

1. **Engine inventory:** selected 5.7.4 fork's OSS Steam, Steam Sockets, Online Services / OSS Adapter implementation and config; actual SDK version; returned interface availability. Public docs include older examples and pages defaulting to 5.8.
2. **Steam two-account path:** project AppID, licensed accounts, create/search/invite, successful transport/admission and map travel. Verify warm lobby invite, cold lobby invite, warm rich-presence invite, cold launch parameters, duplicated/expired intent, and invite during an active match.
3. **Native extension ownership:** prove exactly one SDK lifecycle/callback owner and one join executor when native lobby chat/invites are combined with OSS sessions.
4. **Driver profiles:** Steam-to-Steam modern transport, IP-to-IP direct transport, mismatched-driver rejection, and explicit actual driver selection before connection. Test the Steam-distributed executable in Direct mode with Steam closed/unavailable; prove IP driver selection and successful connection rather than merely an OSS Null login or fallback log. Measure whether a profile change needs process restart. Do not claim same-process runtime driver switching until tested; never silently fallback between incompatible transport profiles.
5. **Auth policy:** malformed/expired/replayed ticket, account mismatch, revoked/failed auth, no partial player admission, and cleanup. Distinguish backend lobby membership authentication from authority admission.
6. **Room leader loss:** service owner changes; no falsely reported gameplay migration. Confirm the chosen return-to-room/disconnect behavior on active listen-host termination.
7. **Direct path:** guest versus verified identity UI, schema validation, IPv4/FQDN/desired IPv6 support, wrong port, denied route, OS firewall denial, stale bookmark, room generation mismatch, full room, unreachable host, and text rate limits.
8. **Tailscale two-machine test:** same-tailnet and node-share cases; direct versus forced DERP; optional peer relay if claimed; game UDP behavior under latency/loss; no reliance on multicast/broadcast discovery. Record connection path alongside game RTT and observed failures.
9. **Voice separately:** capture/playback, transport, mute/block, device changes, privilege restrictions, lobby-to-match transitions, and teardown on every supported provider/platform.
10. **Crossplay claims separately:** compatible protocol/build, non-Steam store configuration, console/platform constraints, authentication mapping, and any Valve SDR permission/coordinator requirement. Steam friends do not automatically become a unified cross-store social graph.

## Evidence quality and unresolved questions

- Verified: current official documentation and primary API behavior as cited; UE 5.7 versions used where available.
- Design recommendations: proposed facade/state machine, direct room protocol, typed join descriptors, guest posture, and initial migration policy. These require approval in the final architecture and later implementation proof.
- Unverified: exact engine fork support and regressions, current project Steam app setup, plugin interfaces at runtime, generated packaging config, performance, startup/delegate ordering, chat mapping, voice integration, mixed-provider/driver coexistence, and console coverage.
- The fetched UE Online Services overview still contains UE 5.1-era shipping guidance while labeled UE 5.7. Preserve that provenance; do not reinterpret it as fresh certification. Steam identifier support and API-complete interface definitions do not certify Steam backend feature parity.
- Official sources directly establish multicast dropping and the Layer 3 boundary. They do not provide a test result for the engine's LAN discovery. The direct-mode recommendation therefore chooses bounded unicast discovery and makes LAN/tailnet distinction explicit.

All URLs above were retrieved on **2026-10-02**. No community tutorials, forum bug reports, or third-party plugins were used as factual authority.
