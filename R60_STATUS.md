# KAMI R60: standard SL material assets

Fixes material asset loading before glTF decoding: the official LLSD serializer emits `<? LLSD/Binary ?>` with spaces and uppercase letters, while R58/R59 only recognized the compact lowercase header. R60 accepts the official case-insensitive, trimmed declaration, compact headers, and unframed binary maps. Bounded envelope validation, type/version validation, UUID-only image references, and transactional decode behavior remain enforced.

F9 now separates material HTTP/transport failures from decoding failures, and shows the last object material HTTP status, response size and decoding stage. Expected legacy-terrain material probes do not overwrite object diagnostics.

Validation includes an explicit standard-header regression (fails against R59), whitespace/case variants, unsupported formats, malformed/truncated assets, version/type/glTF errors, unchanged output on failure, and an asset-to-production-DX12 pixel check. R59 PBR/IBL/terrain and streaming budget improvements remain included.

Live verification: after logging in, inspect F9 PBR assets and faces. A scene containing glTF materials should progress beyond zero; HTTP and decoding diagnostics identify any remaining failure. This is an offline-validated loading fix, not a claim that the user's authenticated assets have been tested.

Known limits remain: one active reflection probe, native KAMI sky rather than EEP, no multi-probe blending or planar mirrors; full avatar and other viewer systems remain incomplete.
