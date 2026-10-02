# CkFoundation online services: continuation on another machine

Prepared 2026-10-02. Read this file completely, then the linked research documents, before changing source, configuration, submodule pointers or Git history. This handoff is self-contained with respect to the conversation; it does not bundle the original machine's source archives, engine, credentials, local guidance edits or runtime setup.

## 1. One-line summary

Continue the source-grounded design of CkFoundation entity features that replace EOSIntegrationKit for Steam and Direct/Tailscale login, lobbies, invitations and chat, with EGS later; research is complete, architecture is proposed, and implementation has not been approved or started.

The user's goal, in their own words:

> To provide a turn-key featureset in CkFoundation to allow any game made with CkFoundation to be able to work on Steam (and EGS too but that's not urgent) AND networks like TailScale (i.e. without Steam) to create lobbies, join lobbies, invite friends, chat, all without requiring much (or any) configuration.

They want new fragments/Utils/processors added to Entities, conformity with CkFoundation architecture/coding standards, and reduced or eliminated Unreal Actor dependence for this integration. The present recommendation is Steam-native and Direct profiles first, with optional EOS later, and an Actor-free online domain that retains necessary Unreal gameplay networking/travel bridges.

The latest authorized work was to publish these notes on a new branch and prepare this cross-machine continuation. That does **not** turn every proposed module or phase into an approved implementation order. Begin with source/environment reconciliation and a concrete first-stage recommendation.

## 2. Repo state

### Transfer identity

| Item | Value |
|---|---|
| Repository | `https://github.com/chainkemists/CkPlugins.git` |
| Transfer branch | `feature/online-services-research` |
| Integration branch | `origin/dev` |
| Fetched publication base | `c14c30816a4f77b52fbef7ff0d83a7cae609509d` |
| Research documents commit | `0de0b94a2d9fe1032ea6ece34110188e7436f0a7` |
| Continuation commit | The separate commit introducing this file, immediately after the research commit; its own SHA cannot be embedded without changing that SHA. Resolve it from the branch history. |
| Expected transfer delta | Eight Markdown files under `docs/research/online-services/`, in exactly two temporary docs-only commits |
| Publication proof | Verify the remote branch tip and its two commits with Git; a prepared handoff file alone is not proof that a push succeeded |
| Runtime evidence | Zero Unreal builds/editor boots/packaging runs/live Steam, EOS or Tailscale sessions |

Both temporary commit subjects contain `[DROP BEFORE MERGE]`; both bodies contain `Drop-Before-Merge: true`. The first holds all seven research documents together so their links remain coherent. The second holds this continuation prompt. No `.gitignore`, source, runtime config, generated script, binary, asset, SDK payload or gitlink is included.

The original active checkout remains on local `dev` at `c5fc5f7f05a8d693ea12caa4b13d0bb9487a75b8`. At publication preparation it diverged from fetched `origin/dev` by **21 local-only / 12 remote-only commits**. A temporary Git index based on the fetched remote tip was used to form this independent docs branch. The normal index and working files were not switched, stashed or reset, and those 21 unrelated local commits were not included. No other repository checkout was created. There were no existing docs-branch commits requiring a merge/rebase onto the base; do not mistake this for permission to publish or rewrite the divergent local history.

### Source revisions are not interchangeable

| Source | Revision |
|---|---|
| CkPlugins root examined in the investigation | `c5fc5f7f05a8d693ea12caa4b13d0bb9487a75b8` |
| Foundation gitlink recorded by that root | `3a52cf6bc8c947db23a43e1041c255a2405fc098` |
| Foundation checkout actually inspected, with local dirt | `6d7019e53d6e79a087c747ecddce8e9074b87d57` |
| Foundation gitlink retained from the publication base | `3d8660c27e33c92c0d6ec5c6b827b5eafc3317a8` |
| CkAuto gitlink retained from the publication base | `55fcdb6e0b5d1a77a5cbdc8b5001834580898d56` |
| CkTests gitlink retained from the publication base | `9dad9c1b6c7ae045e8be3f0c97f144c0f5c4b664` |

A normal checkout of this transfer branch therefore does **not** reproduce the code or all instructions examined during research. Do not silently bump gitlinks or assume the cited line numbers match. Establish the intended implementation baseline with the maintainer, then reread the relevant code there. Source citations remain dated evidence until reconciled.

### Explicitly preserved unrelated dirt

On the original machine, root changes included `.agents/skills/build-test/SKILL.md`, `AGENTS.md`, `CLAUDE.md`, `Config/DefaultGameplayTags.ini`, `Script/Generated/CkPlugins_EntitySpawnParams.as`, Foundation/CkTests submodule state and CkGameplayDebugger untracked content. Root untracked work included the `.agents/skills/ck-*/` wrappers, `CONTINUATION_PROMPT_CrowdAvoidanceVolumePathPlanning.md`, `ReleasePreparation/` and `_scratch/`. Other sessions may continue changing these; this is an ownership warning, not a freeze of their state.

Foundation had modified `.gitignore`, `CLAUDE.md`, `Source/CLAUDE.md`, `Script/ARCHITECTURE.md`, `Source/CkCore/Public/CkCore/Ensure/CkEnsure.h` and five skill/reference files. Its `AGENTS.md`, `DEVELOPMENT.md`, `Script/AGENTS.md`, `Script/CLAUDE.md`, `Source/AGENTS.md` and `Source/ARCHITECTURE.md` were untracked. None was included in these commits. The inspected ensure-header dirt was a comment correction, not a changed macro expansion.

In particular, the newer `DEVELOPMENT.md` used by the audit may not exist after fetching this branch. Essential current rules are restated in section 9; do not resurrect conflicting older guidance merely because it is the only tracked guide on the destination machine.

### Dropping the temporary commits later

Keep all future implementation and permanent project documentation in independently reviewable commits. Before the eventual implementation merge:

1. Identify the exact two temporary commits by their subjects, bodies and paths; resolve the second commit with the history of this file. Do not treat every future documentation commit as disposable.
2. Preserve any accepted architecture decisions needed by the product in permanent documentation first. Preserve this handoff locally if it is still useful.
3. Review the full commit sequence. On the appropriate feature branch, selectively drop only these temporary docs-only commits during the agreed rebase/cherry-pick delivery process. Confirm that retained implementation does not depend on these files.
4. Verify retained code/config/gitlinks and required tests are unchanged by that selection. For the research-only branch, dropping both commits leaves the publication base and no feature implementation.
5. Do not reset the branch to the pre-doc base, broadly delete `docs/`, delete other branches, or force-push shared history automatically. Coordinate any later published-history rewrite under then-current delivery authority.

This section records a future cleanup requirement; it does not authorize doing that cleanup during the handoff.

## 3. Active bugs or open questions

No replacement code exists to debug. Static EIK concerns in the audit were not reproduced and must not be reported as confirmed runtime defects. The open product/architecture questions are:

- **"Invite friends" without Steam:** are shareable joins/bookmarks enough initially, or does the game require a persistent in-game friend graph and offline invitation inbox? The latter needs an additional service.
- **"Chat":** text first is recommended, but the user has not answered whether voice is required at launch. Existing Ck gameplay voice is not a proven pre-game party voice feature.
- Which first consuming game, player capacity, operating systems, listen/dedicated topology and reconnect behavior define acceptance? Windows/listen-server co-op is a proposed first envelope, not an approved constraint.
- Is lobby ownership recovery plus rehosting enough on host loss, or is seamless authoritative gameplay migration required?
- When should EOS/EGS cross-store play begin, and which account/data continuity requirements already exist?
- Can a narrow persistent Ck processor host reuse the current lifecycle graph safely without broad scheduler changes?
- Can the intended Steam-distributed executable use the Direct/IP profile with Steam unavailable, and can profiles switch in-process or require relaunch?
- Which source baseline and newer coding-guide changes will the maintainer make authoritative on the destination machine?

Two optional questions about Direct social scope and text/voice were raised during research; no answers were recorded. Keep their recommendations provisional.

## 4. Why prior fixes or investigations were insufficient

There were no attempted replacement fixes. The completed investigation answered capability and design questions through source inspection and primary documentation; it intentionally did not claim runtime proof.

- The supplied archive is EIK **4.85 / UE 5.6**, built around OSS. Current EIK web documentation describes newer `EIKCore` APIs that are absent from it.
- The main EOS SDK payload is missing. No SDK version, build compatibility, credentials, portal configuration or packaged startup was proven.
- EIK's generic OSS Chat/Message getters return null, although RTC data/P2P APIs provide transport primitives. This is not a complete text-chat product.
- EIK's inspected host migration handles lobby ownership/address information, not authoritative world-state transfer. A helper reporting success after requesting `ClientTravel` is not proof of admission or ready gameplay.
- `CkGameSession` owns a private GameInstance registry for UE player-login/logout signals. Its broader description does not establish a lobby system or persistent processor pump.
- EIK is disabled in the inspected host descriptor. No direct Foundation consumers were found in the documented text scan, but binary assets, the associated engine, other games and unpopulated submodule contents were excluded. No global-removal claim follows.
- The destination checkout will use different recorded source revisions and may lack the locally revised guides. That difference must be reconciled before applying proposed APIs or line references.

## 5. Available diagnostics and the first evidence to collect

Start with [README.md](README.md), [ARCHITECTURE_PROPOSAL.md](ARCHITECTURE_PROPOSAL.md), then [MIGRATION_AND_VALIDATION.md](MIGRATION_AND_VALIDATION.md). The remaining audits contain concrete file/line references and primary-source links retrieved on 2026-10-02. Refresh time-sensitive SDK, service and store claims before implementation/release decisions.

On the destination machine, record the absolute existing checkout path; branch/HEAD; clean or owned dirty status; root Foundation/CkAuto/CkTests gitlinks; actual submodule HEADs; engine association and actual engine fork/version; available build/tooling and current instructions. Use credential-safe remote identification: do not dump a remote URL containing a token into logs or notes. Read-only examples are `git rev-parse HEAD`, `git status --short`, `git ls-tree HEAD Plugins/CkFoundation`, and `git -C Plugins/CkFoundation rev-parse HEAD`.

Use an already provisioned, explicitly selected checkout. This transfer does not authorize creating a new clone/worktree/mirror/repository copy or initializing a different repository root. If no checkout exists on the new machine, get the exact intended path and provisioning authorization first; do not manufacture another checkout as a convenience.

The original EIK source was outside Git at `F:\EOSIntegrationKit-main (1)\EOSIntegrationKit-main`. On the destination, map `EIK_ROOT` to matching user-supplied material if available. No archive or SDK is bundled, and no retrieval source for an identical full archive was proven. These SHA-256 checks identify three inspected files, not the entire archive:

| File relative to EIK_ROOT | SHA-256 |
|---|---|
| `EOSIntegrationKit.uplugin` | `DBCFF772BB8FEB60D15D8E3E4AD9F72F05462BAEED0631E484489411330F9608` |
| `LICENSE` | `5A2C3FFC3DFE7831FE8D4246A914348F54BAF1F01DEAFA7B0F29224F8D133C99` |
| `README.md` | `43AA33E34F5D77D687DFDC8791A053A7B451CCCADC36810D7DF32294DC4966BC` |

If it is unavailable, the branch still provides the written investigation; label source-reproduction gaps explicitly and continue Ck/provider planning that does not depend on unavailable files. Do not substitute an arbitrary latest EIK release. The archive's README says MIT while LICENSE points to the Unreal EULA; prefer independently authored adapters and resolve permissions before copying implementation.

No engine installation, SDK payload, App ID, Steam/Epic credentials, portal settings, Tailscale account/device state or build artifact is transported by this branch. Do not invent those values. The original host was described as the UE 5.7.4 AngelScript fork; engine source was not inspected, and that description is not proof of the destination engine.

## 6. Likely symptoms, causes and files

Paths in this table are relative to the destination root; source paths beginning `Source/` are within `Plugins/CkFoundation` unless otherwise stated.

| Symptom or failed assumption | Likely cause to check | Evidence / files |
|---|---|---|
| Cited guide is absent or line references differ | Local-only guides or different Foundation revision | Section 2 revision table; `DEVELOPMENT.md`, `Source/ARCHITECTURE.md`; Foundation audit |
| Lobby/user state disappears during travel | Online state placed in the map-owned registry | `Source/CkEcs`, `Source/CkGameSession`; architecture persistent-world section |
| Service requests never run, or run twice | No pump, wrong graph selection, global processor registration in both domains | `FProcessorGraphBuilder`, `FProcessorScheduler`, `CK_REGISTER_PROCESSOR`; Foundation audit |
| Missing-dependency ensures in a supposedly small service graph | Lifecycle groups depend on replication/Actor destruction groups | Foundation audit's processor/lifecycle evidence; architecture hosting spike |
| Shutdown inside a callback causes stale access or duplicate completion | Synchronous signals, execute-before-unbind completion, active scheduler stack | `CkRequest_Data.cpp`, Ck signal implementation, scheduler; architecture failure/lifetime rules |
| Lobby join reports success but gameplay fails | Membership was confused with travel, transport/auth admission or readiness | EIK join-helper audit; `CkOnlineConnection` proposal |
| Cold invitation is lost or joins twice | Notifications installed too late, missing intent buffer/deduplication, two provider owners | Steam/Direct research; architecture invitation pipeline |
| Direct mode still needs Steam or connects with the wrong driver | Startup relaunch/subscription policy or persistent Steam driver config | Selected engine's OSS/SteamSockets/IP implementation; transport gate C |
| Tailscale peers are reachable but LAN discovery is empty | Broadcast/multicast assumption across a routed overlay | Steam/Direct research; explicit endpoint/unicast design |
| Host promotion does not restore a running match | Lobby ownership transfer was mistaken for game authority migration | EIK migration source audit; connection epochs/rehost proposal |
| A fresh machine appears to lack EIK usage entirely | Scope excluded binary assets/engine/other games or uses a different version | Migration consumer ledger; root descriptor and actual game assets/config |

## 7. Critical files and their roles

| File | Role |
|---|---|
| [README.md](README.md) | Entry point, recommendation, evidence boundaries and publication/source mismatch |
| [ARCHITECTURE_PROPOSAL.md](ARCHITECTURE_PROPOSAL.md) | Entity/module/provider boundaries; lifetime/completion; login/invites/chat/connection |
| [MIGRATION_AND_VALIDATION.md](MIGRATION_AND_VALIDATION.md) | Proposed scope, phases A-H, consumer ledger and observable gates |
| [EIK_SOURCE_AUDIT.md](EIK_SOURCE_AUDIT.md) | Supplied archive capability matrix and concrete login/lobby/invite/voice traces |
| [CK_FOUNDATION_AUDIT.md](CK_FOUNDATION_AUDIT.md) | Reusable CK mechanisms, source citations and missing infrastructure |
| [STEAM_AND_DIRECT_NETWORK_RESEARCH.md](STEAM_AND_DIRECT_NETWORK_RESEARCH.md) | Official platform/network sources, constraints and test cases |
| [EOS_SERVICE_BOUNDARIES.md](EOS_SERVICE_BOUNDARIES.md) | EIK/EOS/EGS distinction, account domains and future cross-store implications |
| `Plugins/CkFoundation/Source/CkGameSession` | Existing GI-owned private registry precedent; not a lobby implementation |
| `Plugins/CkFoundation/Source/CkEcs` | Registry, processors, requests, signals, destruction and gameplay networking contracts |
| `Plugins/CkFoundation/Source/CkTimer` | Small reference feature quartet and request-drain/completion patterns |

Use the exact relative file names in the source audits to locate particular declarations; use `rg` to reconcile changed layouts. Do not create replacement stubs just because a cited file moved.

## 8. Things ruled out

- No reason was established to reproduce every EIK SDK wrapper before delivering the requested game flow.
- No evidence supports calling CkMessaging a network chat system or CkGameSession a completed online lobby service.
- The new online domain does not need an Actor per user, friend, lobby, invite or chat channel. Eliminating all Unreal gameplay Actors/replication is a different project.
- Tailscale is not a lobby directory, friend graph or offline message backend. Steam lobbies are not themselves gameplay connections.
- A provider enum or abstract UE interface does not establish that a concrete Steam implementation supports every operation.
- No evidence supports silent Steam-to-Direct fallback, unconditional user-index-zero assumptions, or automatic replacement PUID creation when an existing account may need linking.
- No source finding establishes a complete cold-start join intent pipeline in the supplied EIK helpers; that is a scoped absence of identified code, not a reproduced platform failure.
- No runtime, performance, release readiness or repository-wide regression claim has been established.

## 9. Architecture notes and gotchas

### Proposed design to assess

Separate identity/social, lobby/control, and gameplay connection. Proposed boundaries are `CkOnline`, `CkOnlineIdentity`, `CkOnlineSocial`, `CkLobby`, `CkChat`, `CkOnlineConnection`, provider adapters for Steam/Direct/later EOS, and optional UI/editor tooling. These are responsibility boundaries and illustrative names; do not create empty modules for the entire list.

A thin GameInstance subsystem owns a persistent `FEcsWorld`, managed native adapters and a narrow non-Actor pump. Service entities survive map changes; gameplay projections are recreated using provider-qualified IDs and generations. `FEcsWorld` ownership alone does not tick processors. Curate a dependency-complete graph: the existing lifecycle kernel includes gameplay/Actor dependencies, `LoadKernel` is snapshot scope, and global processor registration can cause wrong-registry/double execution.

Callbacks copy bounded value payloads into owned queues; only owning-thread processors mutate registry state. Put a canonical pending operation in storage before issuing provider work that can complete synchronously. Claim/remove its terminal ownership before invoking listeners; copied delegates and execute-before-unbind behavior do not provide a global exactly-once guarantee. Logical shutdown closes admission and invalidates generations immediately; physical registry/scheduler/provider destruction waits for active ticks and synchronous dispatch to unwind. Late remote create/join success needs reconciliation/cleanup, even after local cancellation.

Capture launch/warm invite events as soon as the provider exists and buffer them independently of friends queries. Normalize all entry points through one correlated login/resolve/membership/connect/admit/ready flow. Advertise game endpoints only after the listener is ready, with epoch/version protection. Define joining a party separately from joining its running game.

Prefer stock UE OSS Steam behind narrow CK contracts; use native Steamworks only for demonstrated gaps, with a single membership/cache owner. Keep Direct's host-owned lobby/control protocol independent of the gameplay map, and select a reliable framed channel from existing facilities after a protocol spike. Tailscale discovery should not depend on LAN broadcast. Shareable endpoint descriptors need no global directory; globally resolvable short codes do. Profile/NetDriver compatibility and real build optionality are explicit requirements.

### Portable coding and delivery rules from this conversation

These essentials travel with the handoff because some original guides do not. Read destination instructions too; surface substantive conflicts before choosing a new policy.

- Read a complete neighboring feature before writing. Use the fragment-data/fragment/processor/Utils quartet, typed handles, reflected Specs/results, CK property/macro conventions, CkModuleRules, module-tier direction and Ck logging. No direct registry/fragment mutation from game-facing callers. Support C++, Blueprint and AngelScript, including AS-disabled builds where applicable.
- Use the actual local naming/formatting conventions: Allman braces, four spaces, UHT-compatible reflected declarations, trailing return types for appropriate non-UFUNCTION declarations/definitions, appropriate CK includes and generated header order. `FCk_[Feature]_Spec` is the newer construction payload terminology. Use `FCk_Time` and a monotonic source for online deadlines, not dilated world time. Confirm exact conventions on the accepted baseline.
- Utils enqueue deferred requests; processors drain a copy/reset the live queue so re-entrant requests remain for a later pass. Do not apply the Timer scope-exit completion guard merely around launching an asynchronous SDK operation. Generic completion and richer result signals must retain clear, consistent semantics.
- `CK_ENSURE_IF_NOT` owns the inverted `if`: make prerequisite checks total, then put rejection/recovery directly in its body. Never use an empty ensure followed by an ordinary duplicate `if`. Intended Shipping policy skips predicate and body when checks are disabled; the inspected flags retained checks. Verify actual destination flags and do not change them to evade that policy. External protocol parsing/admission uses ordinary functional result-producing checks, not assertions as an authentication boundary.
- Reject required multi-step admission/composition/configuration atomically. Test malformed/default/stale inputs for rejection, no downstream mutation/callback, and no partial success. Expected network/login failures need typed terminal outcomes and bounded recovery, not hidden log-and-continue behavior.
- ECS fragments observe UObjects weakly; feature-created objects use pooled/traced ownership and asset batches use established rooting. No new raw pointers without explicit maintainer approval, no ad hoc fragment-only strong roots, and no unsafe references captured by delayed provider callbacks. Keep provider SDK pointers/credentials out of public reflected data.
- Respect lifetime ownership and registry boundaries: do not attach a persistent service entity to a world entity's destruction chain. No process-global mutable current user/lobby. Multiple PIE instances require explicit context routing.
- Treat waited-on resources as leases with reconciliation and bounded escape; scope cleanup through established lifecycles/state machines. Do not add unexplained delays, unbounded retries, silent fallback or split-brain caches.
- Never create linked worktrees, clones, mirrors, repository copies or alternate checkouts as a workaround. Work in the explicitly selected existing checkout; preserve unrelated dirt and stage exact paths only. Never reset, clean, stash others' work, rewrite shared history or modify other sessions' submodules casually. If safe in-place work is blocked, report the exact blocker.
- Never clear/recreate Binaries, Intermediate, DDC or existing outputs for a clean rebuild without explicit approval of that destructive scope. Do not build in another existing checkout to evade ownership or locks.
- Use semantic Git messages with concise explanatory bodies. Never put assistant/model/vendor attribution or AI co-author trailers into Git/hosting metadata. Rebase future implementation delivery onto the appropriate freshly fetched integration branch; do not merge divergent histories or publish unrelated local commits. Do not merge/publish future work merely because this docs transfer was authorized.
- The original main conversation retained Astra Ultra by user request; bounded research/review was delegated to Sol 6.1. Follow the user's destination-session selection, perform the required model/effort check there, and keep architecture/authority/final integration with the lead. Use available lower-cost Sol agents for clearly bounded work, with explicit ownership and verification.

### Unreal verification rules

Use `CkAuto/UnrealToolbox.exe` with **absolute `--project=<actual-destination-root>`**, never raw Build.bat, UnrealBuildTool or automation editor commands. Resolve the destination Toolbox/engine setup first. Before launching, state the planned editor-boot count and why. For a localized change normally plan one red and one final green boot; architecture phases need an explicit proportional gate plan.

Launch Toolbox detached through PowerShell `Start-Process -FilePath <absolute-Toolbox-path> -ArgumentList <validated-arguments> -PassThru -WindowStyle Hidden`. Hidden applies to the parent console; retain the Toolbox progress window. **Never pass `--no-progress-window`.** Request managed-shell GUI/escalated permission as required, monitor the returned process and its `--output` log to completion, and do not launch a duplicate after timeout. Use `--config=` only with `--build`; AS-only tests reuse current binaries. Inspect fresh logs for ensures/script errors and verify actual process exit/artifacts before claiming success.

Write any multiline PowerShell the user must run into a workspace `.ps1`; do not hand them a pasted multiline shell recipe. Cooking, packaging, staging and archiving are build-machine work; Shipping requires explicit permission. Interactive/editor-only evidence remains a separate observed/manual gate, and fake-provider tests cannot prove real overlay, cold-launch, relay, transport, audio or installed-build behavior.

## 10. Concrete diagnostic and verification flow

1. **Establish destination state.** Resolve an authorized existing checkout and read current instructions. Verify the transfer branch, both temporary commit markers and eight-file delta against its publication base. Inventory dirty ownership, recorded/actual submodule revisions, available guides and engine/Toolbox association. Do not switch a dirty checkout or initialize a new repository root blindly.
2. **Reconcile source and doctrine.** Compare the accepted destination baseline with the three Foundation revisions in section 2. Identify which cited APIs/rules differ. Confirm missing local guide updates rather than assuming the branch transported them. If matching EIK source is available, check descriptor/version and targeted evidence; otherwise keep that limitation explicit.
3. **Restate the product decisions.** Confirm the first consuming game, Direct invitation scope, text/voice, player/topology/platform envelope, host-loss behavior and EOS timing. Keep the provisional defaults separate from accepted requirements.
4. **Prepare the first stage for approval.** Recommend the smallest architecture spike and its acceptance observations using migration phases A-C. Publish a concrete scope/files/dependencies/evidence plan before broad implementation; continuation alone is not authorization to implement every proposed module.
5. **When implementation is authorized, prove hosting first.** Fake operations must complete through Utils/processors/signals in a persistent registry, survive map teardown, reject stale/duplicate callbacks and cancel exactly once. Test listener-triggered shutdown/profile switch/destruction, safe physical teardown, dependency-complete lifecycle order and no scheduler Actor/double execution in this domain. C++/BP/AS results must agree.
6. **Prove the real transports early.** Use the selected engine's actual driver/OSS implementation. Test two licensed Steam accounts/machines with the game's App ID and Direct/IP clients over LAN. Prove the intended Steam-distributed executable supports Direct with Steam unavailable; establish restart/profile policy. Record membership, connection, admission and readiness separately.
7. **Deliver one vertical slice.** After hosting/transport gates pass, create/join/leave a Direct lobby, ready up, exchange text, enter gameplay and return. Exercise timeouts, malformed packets, revoked/expired invites, late success, host loss and bounded cleanup. Then add Steam social/warm/cold invites and test Tailscale connectivity paths. Follow phases D-F for reusable UI/config validation and a second consuming game.
8. **Remove EIK only from inventoried consumers.** Audit code, Blueprint assets, config, engine plugins, SDK staging and identity/cloud-data continuity. Migrate every actually consumed capability before removal. Source scans alone cannot prove serialized-reference or installed-build cleanup; phase G requires those separate observations.
9. **Keep release and future scope explicit.** EOS/EGS needs a shared cross-store population and compatible transport/account flow, not two isolated providers. Voice, dedicated allocation and seamless gameplay migration each need their own acceptance. Refresh official platform/store requirements when relevant.
10. **Report evidence and retain removable commits.** Distinguish inspected/proposed/runtime-verified facts, record commands/logs/results and remaining gaps, and keep implementation commits separate from the two `[DROP BEFORE MERGE]` docs commits. Update the living handoff if work continues; do not leave an obsolete status document that claims success.

The immediate continuation can complete steps 1-4 without an Unreal build. Do not spend an editor boot merely to reread research documents.

## 11. Suggested first message

Use the launcher below after making the transfer branch available in the authorized destination checkout. The absolute path is the original machine's convention; if the destination root differs, replace `D:\Repos\CkPlugins` in the launcher with the actual absolute root before sending it. Reading this file does not require the original `F:` archive or unpublished guides; reproducing their evidence does.

```text
I'm continuing the CkFoundation online-services replacement research. Read this continuation prompt fully before doing anything: D:\Repos\CkPlugins\docs\research\online-services\CONTINUATION_PROMPT_OnlineServices.md
Use feature/online-services-research in an existing authorized checkout, remapping the absolute path if needed, and first verify source/submodule revisions and available guides.
Then propose the smallest implementation gate without starting implementation, preserving unrelated work and keeping the two [DROP BEFORE MERGE] documentation commits separate from future implementation.
```
