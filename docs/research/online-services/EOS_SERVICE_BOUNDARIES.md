# EOS, EIK and future EGS integration boundaries

Date: 2026-10-02. These notes distinguish the supplied EIK archive from the underlying Epic services and currently published guidance. All implementation claims about EIK are in [EIK_SOURCE_AUDIT.md](EIK_SOURCE_AUDIT.md). These notes are not proof of portal setup, SDK compatibility or a working release.

## 1. Removing EIK does not require removing EOS

EIK is an Unreal integration layer around several EOS interfaces, plus convenience/product helpers, third-party login integrations and editor tooling. EOS operates the underlying account, lobby, transport, voice and data services. A Ck-owned adapter can use stock Epic integration or the official SDK without EIK.

Conversely, the requested first Steam-native + Direct feature set need not use EOS at all. That is a proposed initial product profile, not a claim that Steam supplies EOS cross-store functionality. Steam/Direct functionality and a future EOS/EGS profile should be independently selectable and buildable.

Epic describes its services as modular: account/social, multiplayer, progression/data and other services can be adopted independently. Integrating the SDK still requires product setup and credentials/policy. [Epic service introduction](https://onlineservices.epicgames.com/en-US/news/introduction-to-epic-online-services-eos?lang=en-US), [Epic getting started](https://onlineservices.epicgames.com/sdk?lang=en-US).

## 2. Identity has separate domains

| Identity | Used for | Architectural consequence |
|---|---|---|
| Steam account ID | Steam friends, platform identity and associated Steam services | No automatic equivalence to an Epic account |
| Epic Account ID | Epic Account Services identity/social | Separate from a game's EOS product identity |
| EOS Product User ID (PUID) | EOS Game Services identity for the product | Preserve/link deliberately when changing external login method |
| Local Direct peer ID | Proposed local profile and session identity | No claim of Steam/Epic verification |

Epic's Connect guide describes external-provider or Epic-account login for Game Services. The Epic-account route obtains an Auth ID token and then uses Connect. It also cautions against creating duplicate PUIDs when the player previously used another identity. For a design using both EAS and Game Services, follow its documented account/proxy-account route rather than treating independent logins as interchangeable. [Epic Connect guide](https://dev.epicgames.com/docs/epic-online-services/eos-fundamentals/connect-interface/connect-guide/log-in-with-an-epic-games-account).

The supplied EIK high-level Auth flow instead copies an Auth access token into its Connect call. That is an observation of this older source, not a recommendation to copy its token handling into a new SDK integration. Pin the chosen SDK and adapter behavior during implementation.

Steam-native login can remain invisible to a player already signed into Steam. Steam→EOS Connect is an additional service login and portal configuration. Epic social/account linking can introduce consent/overlay/browser interaction. These are three distinct UX promises; the framework should expose which one a project selected.

## 3. Auth and refresh choices need explicit policy

The archive offers portal, persistent, developer, exchange-code and external login paths; its detailed fallback/refresh traces and static concerns are in the source audit. Retain their useful semantics without retaining encoded credential strings, global user-zero assumptions, or ambiguous completion.

Proposed login policy:

- Development credentials are confined to development profiles.
- Launcher-provided exchange credentials are consumed through the intended provider flow and redacted.
- Persistent login failure can request interaction when policy allows; user cancellation is terminal, not an automatic retry loop.
- Connect expiration and account revocation invalidate affected operations and social/lobby state using an account generation.
- Logout, forgetting cached credentials, unlinking accounts and deleting an identity are different requests; the default logout never silently deletes progression/account links.
- Do not create a replacement PUID automatically when an existing account may need linking.

Epic's Dev Auth Tool is explicitly a development facility, not production login. [Epic developer authentication guide](https://dev.epicgames.com/docs/epic-online-services/accounts-and-social/eos-epic-account-services/auth-interface/auth-guide/log-in-with-the-dev-auth-tool).

The supplied EIK requests Steam token type `Session`. Epic's UE 5.7 `EOSSettings` reference marks the older App and Session choices deprecated and describes WebApi alternatives. This is a concrete migration compatibility question: verify the selected EOS SDK, Steam ticket purpose/remote identity, and configured identity provider together. Do not mechanically port the archive's ticket choice. [UE 5.7 EOS settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/EOSSettings?application_version=5.7).

## 4. Friends, friendship requests and lobby invitations

A friendship request changes a social relationship. A lobby invitation asks someone to join a room. Presence can expose a joinable destination. These are distinct APIs and operations in the supplied EIK source, and should stay distinct in Ck.

Epic's friends API does not necessarily expose every friend visible in the Epic launcher; consent and application access matter. Thus an empty result is not proof that the player has no friends. Epic's published example explains application consent filtering and separate user-info queries. [Epic friends guide](https://onlineservices.epicgames.com/news/querying-for-epic-friends-and-their-status).

The EOS social overlay integrates with presence and lobby/session join destinations. A join request still requires game-owned resolution and connection handling. Maintain destination validity when the current lobby/game changes. [Epic overlay SDK integration](https://dev.epicgames.com/docs/epic-online-services/accounts-and-social/social-overlay/sdk-integration?lang=en-US).

EIK's high-level OSS path assumes received lobby invitations are displayed by the overlay, while lower-level receive/query APIs are available. Its accepted-invite path requires a known local user and builds a search result. No source-grounded complete cold-start intent queue was identified. Ck should capture intents early, retain them through login and menus, deduplicate them and surface failures. Platform runtime tests are required before declaring cold-start support absent or complete.

## 5. Lobby, session, transport and host migration

Epic's lobby service maintains synchronized membership/member data and supports integrated voice, owner changes and kicks. Sessions use explicitly maintained session state and player registration. Dedicated-server registration uses sessions; a client party lobby can still point at that server. Neither resource is itself the gameplay connection. [Epic lobbies/sessions introduction](https://dev.epicgames.com/docs/epic-online-services/multiplayer/lobbies-and-sessions/lobbies-and-sessions-introduction?lang=en-US).

EIK maps both resource kinds into UE `IOnlineSession`. That mapping is useful adapter behavior, not a reason to merge all Ck domain state into one ambiguous Session feature.

A provider can promote another lobby owner while the previous player's authoritative game process has disappeared. EIK reacts to promotion and produces a new owner/address callback; its inspected path has no game snapshot transfer or reconnect/state-recovery orchestration. Therefore first-release lobby continuity and gameplay rehost should be designed explicitly. Seamless migration is separate work.

## 6. Text, voice and data

The archive's generic OSS Chat/Message getters are null. However, it has concrete RTC data-send/receive and P2P packet wrappers. Those are transports on which a text protocol could be built; they do not establish channel UX, reliable history, moderation, offline delivery or friends DMs.

The voice layer is more complete: `IVoiceChat`/user integration, RTC room operations, lobby voice, devices, participant events, mute/volume and a custom synth output. Its positional path contains explicit Pawn/PlayerState/world coupling, detailed in the source audit. Replace that lookup with a controlled identity-to-speaker/audio-sink association if this path is adopted.

For future Ck EOS chat, select one data path and document its limits. For voice, first evaluate the existing `CkVoiceChat` feature and distinguish connected-game voice from a pre-game provider room. Avoid adding a second uncoordinated microphone owner or double-rendering a speaker through two voice stacks.

## 7. EGS later still influences today's boundaries

EGS is a storefront, EOS is a service suite, and EIK is a plugin. They are not synonyms. A future EGS release does not itself justify making an Epic account mandatory for the Steam-native profile today.

Epic's distribution agreement includes PC crossplay requirements for multiplayer products across other PC storefronts. Treat shared matchmaking/invites/transport as release planning, and recheck the applicable current rules before that release. Merely adding a separate EGS provider next to Steam would leave two disconnected populations. [Epic distribution agreement](https://cdn2.unrealengine.com/egs-distribution-agreement-831c3b1b8ec4.pdf).

EOS crossplay tooling can combine Steam/Epic social entry points through its account/overlay integration. It still needs the game's account policy, compatible lobby population, connection transport and admission model. [Epic PC crossplay tools](https://onlineservices.epicgames.com/news/epic-online-services-release-free-pc-crossplay-tools), [Epic crossplay technical overview](https://dev.epicgames.com/docs/epic-online-services/accounts-and-social/crossplay/crossplay-technical-overview).

Do not assume overlay parity across operating systems. Current Epic crossplay documentation lists platforms where the social overlay is unavailable, and its redistributable documentation describes the desktop overlay installation dependency. Keep custom Ck UI useful independently of an overlay. [Epic crossplay restrictions](https://dev.epicgames.com/docs/epic-online-services/accounts-and-social/crossplay/crossplay-restrictions), [Epic redistributable installer](https://dev.epicgames.com/docs/epic-online-services/accounts-and-social/crossplay/redistributable-installer?lang=en-US).

## 8. Evidence limits

The supplied archive declares EIK 4.85/UE 5.6 and lacks the main EOS SDK payload. Current [EIK lobby documentation](https://eik.betide.studio/multiplayer/matchmaking/lobbies) contains an `EIKCore` API absent from that archive. It is useful as version-drift evidence, not as a description of the inspected implementation.

Epic public documentation also sometimes defaults to UE 5.8 or redirects old pages; 5.7-specific references were used where available. Some EOS deep pages returned empty extraction or HTTP 429, so claims here rely on the substantive official pages that were retrieved and the supplied source, not unavailable pages. The associated 5.7.4 engine source, live service configuration and binaries were not inspected/tested in this research.

No protocol limits, capacity, latency, compliance status, live feature parity or production reliability are inferred from this review. Those belong to the implementation gates in [MIGRATION_AND_VALIDATION.md](MIGRATION_AND_VALIDATION.md).
