# KAMI Hotfix Home — in-session, NBA 2K-style patch experience

## Non-negotiable product behavior
**Return to Kami Home MUST NOT log the resident out of Second Life.**
Kami Home is an *in-viewer application overlay/state*, NOT the login page.
The avatar, region connection, message pumps, session tokens and normal
connection liveness remain active while Home is displayed. Returning to World
closes the Home overlay and shows the existing in-world session instantly.
Do not call any logout, disconnect, teleport, login-reset or shutdown path
while entering or exiting Home. Maintain normal network handling while Home is
open and avoid freezing the rendering/event loops.

## Release policy: only feature updates may require a restart
**Optimization hotfixes MUST be restart-free and must preserve the connected
Second Life session.** No optimization hotfix may modify KAMIViewer.exe,
native DLLs, or any currently loaded executable module. An optimization
requiring rebuilt native code, process exit, a new renderer DLL or another
non-hot-reloadable change MUST be deferred to a scheduled **feature update**
(or a separately labeled emergency maintenance release requiring consent).
Do not relabel a restart-required patch as an optimization hotfix. Feature
updates may prompt for an optional, explicitly user-approved restart; no
forced logout during the resident's active Second Life session.

## In-viewer push notification experience
- Use the viewer's **native, non-modal notification/toast framework**, not
  Windows OS notifications or modal dialog boxes.
- The future trusted update controller notifies the UI ONLY after it verifies
  a publisher-signed, restart-free optimization manifest and release channel.
- Title: **KAMI OPTIMIZATION AVAILABLE**; include a verified short version.
  Actions: **View details** (opens Kami Hotfix Center without logging out)
  and **Later** (dismisses for the moment). The resident can continue using
  Second Life normally.
- The final 1.0 flow will add **Return to Kami Home** from those details.
  Do not display a Return to Home install button until the connected-session
  Home experience and signed installer actually exist.
- Do not steal keyboard focus, teleport, disconnect, or repeatedly spam.
  Dedupe by verified update version for the current session; store a persistent
  dismiss/snooze policy in the later production controller. Keep alerts
  available through Hotfix History/notification center when dismissed.
- Native `notify` notification template and a `notifyVerifiedHotfix()`
  hook are source-integrated in this prototype, but currently **nothing
  invokes them** because no verified update feed exists.

## Desired update flow for full 1.0
1. After an opt-in update check, show a non-disruptive in-world notice:
   **UPDATE REQUIRED — Return to Kami Home to apply this optimization**.
   Do not auto-disconnect an avatar or interrupt unsaved creative work.
2. **Return to Home** opens a full-screen Fire Garden shell OVER the live
   viewer session. Display the connection state, patch notes and progress.
   World network/event processing stays alive. The avatar remains in-world;
   provide **Return to World** after a compatible hotfix completes.
3. Download only changed patch files into bounded, versioned staging.
   Verify the publisher signature, file hashes, allowed paths and quota.
4. Only **explicitly supported hot-reloadable config, shader parameters and
   safe asset changes** can be applied within this live session. If an asset
   is in use, defer replacement; never overwrite GPU/CPU-loaded assets.
5. Apply a compatible patch transactionally, verify it, then offer
   **Return to World** without logging in again. On failure, rollback and
   preserve the resident's connected session.
6. If a proposed patch requires changing KAMIViewer.exe, native DLLs,
   renderer code or other loaded binaries, it is **NOT an optimization hotfix**.
   Reclassify it as a feature release and stage it for a later explicitly
   approved restart. Never advertise it as restart-free, and never silently
   disconnect an active resident.
7. Show Hotfix History and manual Check for Updates. Channels are Stable,
   Beta and Experimental with explicit consent for preview channels.

## Patch classes
- **Live optimization**: signed, schema-validated *allowlisted* performance
  parameters, GPU feature presets and safe shader constants that the engine
  has explicit hot-reload support for. Preserve user settings and existing
  graphics preferences. Use measurable frame pacing/VRAM improvements.
- **Safe data patch**: a small, signed asset update only if the asset has
  explicit hot-reload support, references are safely invalidated, and no
  conflicting or unsaved operations exist. Otherwise stage it for the next
  restart with clear notice; stay connected while staging.
- **Feature update (separate release channel/event)**: native binaries and
  non-reloadable renderer changes may be downloaded and staged while the
  resident remains connected, but must NOT be applied as a live hotfix.
  An explicitly user-approved restart/reconnection is required to replace
  running EXE/DLLs. The resident can defer this until convenient.

## Release manifest (design)
`schema_version`, `channel`, `from_build`, `to_build`, `min_os`,
`requires_restart`, `release_notes`, `sequence`, `files` (relative
path, compressed size, final size, SHA-256) and `signature` (key ID + a
detached publisher signature over canonical manifest bytes).
Reject stale/replayed manifests, unknown signatures, and unsafe downgrades.
Private signing keys NEVER belong in the client or source.

## Safety constraints
- HTTPS and publisher-signature verification, not HTTPS alone
- Bounded downloads; verify before applying; atomic staging and rollback
- Strict path allowlist, no traversal or symlink/reparse-point escapes
- No arbitrary commands, dynamically downloaded native code while in session,
  unreviewed plugins or privileged changes to user permissions
- Never overwrite personal settings, credentials, inventory or SL cache
- Don't automatically disconnect, teleport, suppress network processing or
  change voice/chat privacy behavior to enter Home
- Connection health indicator while Home is up; if the SL session drops for
  external reasons, report the real state without pretending it remains online
- Offline/error fallback to existing live viewer; Home can always close
- Respect Second Life viewer policy, user consent and existing third-party
  licensing and notices

## Prototype actually implemented on this feature branch
- A native Fire Garden **Hotfix Center (Preview)** accessible from Workspaces
- A working Close action that does not change the user's world session
- Native viewer notification template with View details / Later buttons
- Verified-only (currently uncalled) C++ notification helper, deduped by
  release version; View details opens the Hotfix Center without logout
- No live update feed, install code, forced logout, external updater,
  functioning "Update Required" trigger or full-screen Home shell yet
- Source/integration tests to avoid presenting a nonfunctional updater as real

## Remaining for the full release
- Actual connected-session, full-screen Home shell and Return to World flow
- Safe handling for unsaved edits and active dialogs while entering Home
- Signed feed, update controller, UI-thread notification dispatch, safe
  reload and staged binary broker
- Enforce release-type policy: restart-free optimization hotfixes versus
  separately scheduled, restart-eligible feature updates
- History, rollback, release-channel selection and failure-injection tests
- Long-running in-world tests for timeouts, idle disconnections, chat/voice
  visibility, avatar state and real streaming/decode frame times

Do not advertise this prototype as a deployed hotfix service.
