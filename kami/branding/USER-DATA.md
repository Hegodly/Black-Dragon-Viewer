# KAMIVIEWER user data and diagnostics

New Windows builds use `%APPDATA%\KAMIVIEWER` for their application data.
Daily logs live in `%APPDATA%\KAMIVIEWER\logs` and are named
`KAMIVIEWER_<date>.log`. Crash/exit markers and diagnostic fallback filenames
use the same branding. The default cache is `%LOCALAPPDATA%\KAMIVIEWER` unless
an explicit supported cache override is supplied.

The crash reporter uses the new data root and prefers the exact active log
recorded in viewer crash metadata. Legacy server metadata field names remain
compatible. Daily-log cleanup derives the date offset from the new prefix
length and skips malformed date filenames.

Existing `%APPDATA%\KombatantViewer` or `%APPDATA%\BlackDragon` logs/settings are not moved, deleted,
or automatically copied. A new KAMIVIEWER data directory starts with fresh
settings. Custom settings can be migrated separately after the startup problem
is diagnosed. Builds already downloaded still write to their original paths.

The build workflow applies and verifies this naming change before compilation.
Source/integration checks do not establish successful login-screen startup.
This rename does not identify or fix the separately reported startup exit.
