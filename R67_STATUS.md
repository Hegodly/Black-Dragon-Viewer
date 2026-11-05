# KAMI R67 — bounded multi-LOD mesh recovery and mesh streaming

Based on the successful R66 diagnostic viewer, preserving every R65 world-recovery fix, native DX12/PBR graphics and Quality's 2,048 primitive ceiling.

## Real-world problem

A live property can retain many Second Life mesh references and show partially decoded geometry while nearby mesh assets are still absent. In R65 the simulator retained 213 root objects and 1,780 linked objects, but the viewer drew only 36 mesh instances. This difference is not proof that the 2,048 primitive limit is being reached. R66 added explicit Near / NoLOD / Parent / Far / BadPose / Undrawn diagnostics, which remain available here.

## Changes

1. **Full four-level static LOD recovery.** The previous loader considered only medium_lod and high_lod and rejected assets with valid lower geometry. Prefer medium then high, followed by low_lod and lowest_lod. Missing/zero-sized LOD blocks are skipped. Every populated candidate is independently validated with the existing LLSD, zlib/gzip, offsets, bounds, face and vertex checks. Malformed nonzero block metadata still fails; a corrupt higher-detail data block may be skipped only when a fully validated lower-detail block is available. Never invent a proxy mesh or accept arbitrary bytes.
2. **Valid larger meshes.** Raise the per-decoded static LOD triangle ceiling from 20,000 to 60,000 while retaining the existing 8 MB decompression bound and the shared 4-million decoded-triangle session limit, 1-million-triangle Quality rendering budget and original 4-million-vertex DX12 scene cap.
3. **Four bounded concurrent mesh workers.** The previous live mesh path ran one HTTP fetch+decode future at a time, despite the authenticated scheduler supporting up to eight. Run at most four independent off-thread workers; drain their completions individually and preserve scheduler priorities, per-launch throttling, 4xx/5xx retry policy, and independent shutdown joins. No DX12 calls happen on worker threads.
4. Preserve R66 diagnostics and 2,048-primitive Quality fallback; no increases to world root retention, texture atlas or network-object recovery requests.

## Regression tests

- Low-only, lowest-only and medium-only real binary LLSD mesh fixtures now decode.
- A valid 20,001-triangle static LOD decodes; oversized 60,001-triangle LOD remains rejected and cannot mutate the last valid output.
- Existing malformed/missing medium fallback, invalid offset/deflate, UV, vertex-bound and DX12 scene tests remain intact.
- Four-worker cap is compile-time checked against the shared scheduler's eight-active upper bound.
- Native Windows CTest and Microsoft WARP forced DX12/PBR pixel checks must pass before downloading.

## Limitations

No captured live session assets were used to measure the gain. Better supported mesh LOD and streaming are necessary but not sufficient for a complete house: unsupported non-mesh sculpt geometry, unresolved parents, visibility range, alpha-masked faces, and other Second Life viewer features may still limit completeness. An in-world R67 screenshot (F9 diagnostic Near/NoLOD/Parent/Far/Undrawn) should establish the next bottleneck.
