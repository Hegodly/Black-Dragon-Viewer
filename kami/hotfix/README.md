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
6. For a patch that changes KAMIViewer.exe, native DLLs, renderer code or
   other loaded binaries, **a full process restart is unavoidable**. Queue
   it and clearly label **Restart required**. Never promise an in-session
   binary hotfix or silently log the user out. The user chooses when to exit.
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
- **Binary patch**: downloadable deltas can be staged in Home while connected,
  but replace EXE/DLL files only via a separate trusted updater *after the
  viewer process exits*. Requires a user-approved restart and reconnection.

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
- No live update feed, install code, forced logout, external updater,
  "Update Required" modal or full-screen Home shell yet
- Source/integration tests to avoid presenting a nonfunctional updater as real

## Remaining for the full release
- Actual connected-session, full-screen Home shell and Return to World flow
- Safe handling for unsaved edits and active dialogs while entering Home
- Signed feed, update controller, safe reload and staged binary broker
- Deferred restart messaging for binary patches, not an in-session DLL swap
- History, rollback, release-channel selection and failure-injection tests
- Long-running in-world tests for timeouts, idle disconnections, chat/voice
  visibility, avatar state and real streaming/decode frame times

Do not advertise this prototype as a deployed hotfix service.
