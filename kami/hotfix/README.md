# KAMI Hotfix Home — NBA 2K-style update flow

## Product behavior (full-release target)
1. The viewer checks an HTTPS release feed periodically and on login. It may
   *display* an available update while in Second Life, but it must not replace
   executables or forcibly disconnect an active resident.
2. Non-disruptive notification: **UPDATE REQUIRED — return to Kami Home to apply**.
   Use a non-blocking notice until the resident chooses to finish their session.
3. When the resident explicitly chooses **Return to Home**, safely save local
   preferences, prompt about unsaved work, and end the session normally.
4. The trusted updater downloads only changed files to a staging directory,
   verifies a pinned publisher signature (not just a hash from the same server),
   checks exact allowed paths, and atomically applies the patch when the viewer
   executable is no longer running.
5. Show progress in the Home/login experience. Restart or relaunch only when
   native code/DLLs change; supported data-only hotfixes may be reloaded safely.
6. On health-check failure, restore the previous version automatically and show
   a useful rollback report. Preserve user settings, textures and account data.
7. Provide **Hotfix History**, a manual **Check for Updates** action, and
   per-channel controls: Stable, Beta and Experimental. Honor explicit opt-in
   for Beta and Experimental.

## Patch formats
- **Live config hotfix:** signed, schema-validated setting/feature flag or
  shader parameter allowlist. Only change reloadable values; do not override
  user graphics preferences or claim to increase FPS without benchmarks.
- **Asset hotfix:** verified changed files. Apply at Home with no active world
  session; refresh viewer caches only if necessary.
- **Native renderer hotfix:** verified DLL/EXE replacements after full exit,
  followed by restart and health check. No in-place changes to loaded binaries.

## Required manifest fields
`schema_version`, `channel`, `from_build`, `to_build`, `min_os`,
`requires_restart`, `release_notes`, `sequence`, `files` (relative path,
compressed size, final size, SHA-256) and `signature` (key ID + detached
publisher signature over canonical manifest bytes). Reject absent or unknown
signatures and downgrade/replay attempts. Never embed signing *private* keys
in a viewer or repository. Key rotation requires independent authorization.

## Security invariants
- HTTPS with proper trust validation; pinned application-layer signing keys
- No arbitrary script execution, updater shell commands or unsigned plugins
- Allowlisted relative paths only, no traversal, symlink/reparse-point escapes,
  user-data overwrite, or writes into grid/inventory/Second Life caches
- Bounded patch size, disk-space preflight, atomic rename and rollbacks
- Process/session lock: the in-world viewer may download/cache packages, but
  cannot modify its own EXE, shaders in use or loaded libraries
- Explicit safety valve to disable auto-download/checks and roll back
- No server-controlled changes to security/privacy/anti-RLV permissions

## Implemented in this branch
- Native KAMI Fire Garden **Hotfix Center (Preview)** accessible in the
  Workspaces menu, compiled into the Windows viewer after theme integration
- A real Close action; no non-functional Apply or Return to Home button
- Feature kept **off**: no network check, unverified patch installer, auto
  logout, forced update, or changes to in-world performance
- Source and integrated XUI/C++ preflight tests to avoid packaging a
  misleading update experience

## Still required before 1.0
- Signed feed and trusted delivery backend
- Native update controller and secure Windows off-process patch broker
- Safe logout-to-Home transition and unsaved-work handling
- Download, verify, stage, install, restart and automatic rollback
- Patch history, detailed error reporting and beta cohort testing
- Failure injection tests: disconnect, disk full, corrupt files, mismatched
  signatures, malicious filenames, viewer crash, half-applied patch

Do not enable UPDATE REQUIRED until all these mechanisms are tested.
Do not merge this prototype into an existing beta release just because its
compile succeeds.
