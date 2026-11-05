# KAMI R65 — 2,048-primitive fallback and missing-object recovery

This is a targeted response to a real in-world R64 report: **about 500 FPS with V-Sync off, but buildings remained missing**. The R64 diagnostic screenshot showed only 26 retained roots, 427 linked objects, 27 meshes drawn, a fully saturated 256-entry missing-object request queue and a material HTTP 200 response of 543 bytes rejected as "binary / trailing bytes". The scene did not approach the former 4,080-shape Quality ceiling, so changing the primitive cap alone is **not** a verified cure.

## Changes

1. Respect the requested fallback: the Quality primitive draw ceiling is **2,048** again. Preserve R64 mesh-first DX12 geometry budgeting and the larger 4,096 root-object retention limit, which is independently needed for real mesh buildings; preserve the 8,192 linked-object retention limit. Do not reduce root retention simply because fewer procedural primitives may be rendered.
2. Expand the bounded cache-miss queue from **256 to 1,024 IDs**. RequestMultipleObjects packets remain limited to **16 IDs and at most one packet per 400 ms**; authenticated SL message templates and the reliable resend budget are unchanged.
3. Prioritize explicit simulator cached-object misses and verified missing linkset parents over speculative requests originating from unknown terse movement updates. Promote a speculative request to high priority when a cached/parent miss for the same ID appears. When the queue is saturated, an important request may replace a speculative one, never an existing important request.
4. Expire exhausted object-recovery requests once their existing five-attempt/90-second reclaim rule is satisfied, even before the queue fills. This improves long-session recovery and prevents stale entries from contaminating missing counts.
5. The PBR LLSD binary decoder now accepts **up to four NUL bytes only after a completely validated root map** (a narrowly bounded terminator/padding compatibility allowance). Nonzero trailing data, extra payload, a missing map-close marker and padding greater than four NULs still fail. This may address a padded binary response; the exact bytes of the live 543-byte response are unavailable, so the failure is not claimed fixed until tested.
6. Show the pending/maximum missing-object request count in the live in-viewer diagnostic panel. Label the build R65.

## New and adjusted tests

- 4,200 eligible simple primitives, Quality cap at 2,048 with exact deferred count; Balanced remains 512.
- Root/linked retention and missing queue capacity remain bounded by compile-time checks.
- Priority comparator regression checks ensure verified building/cache requests outrank speculative terse requests.
- Binary LLSD material decoder test accepts one to four trailing NULs and rejects five, nonzero junk and a missing terminator.
- Preserve all existing R64 real-mesh-first vertex budgeting, authentic LLUDP and renderer tests.

## Limitations

This is not proof of complete live world rendering. A full SL session still has unsupported object classes, incomplete avatar appearance, a limited GPU texture atlas, and limited decoded material assets. The 543-byte material payload was not captured so the precise binary decode problem remains uncertain if the new bounded terminator allowance does not help. Evaluate real house loading, cache misses over time and PBR asset/faces counters rather than FPS alone.
