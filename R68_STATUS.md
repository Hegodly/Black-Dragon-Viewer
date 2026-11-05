# KAMI R68 — 4,096-entry bounded missing-object queue and parent-first recovery

Based on the real-world R67 test: **Root 14 / Linked 301 / Missing 1,024/1,024 / Requested 3,136 / Parent 43 / Mesh DRAW 174 / Near 230 / Undrawn 56**. Mesh LOD decoding and four concurrent downloads improved the scene, but missing verified object anchors were still blocking full linkset reconstruction. The request queue was completely full.

## Changes

1. **Increase retained pending recovery IDs from 1,024 to 4,096**; do not raise the maximum 4,096 root-object retention, 8,192 linked-object retention, 2,048 Quality primitive-draw budget, or DX12 4-million vertex cap.
2. **Prioritize linked-object parent/root recovery** ahead of ordinary ObjectUpdateCached IDs, ahead of speculative unknown terse-movement updates. Compressed and full SL parent references are recorded as highest-priority entries. A speculative or cached ID already in the map can be upgraded in-place if later verified as a missing parent.
3. On an exhausted 4,096-ID queue, attempt to reclaim completed five-attempt entries, then allow a verified missing parent to displace a speculative or ordinary cached ID rather than being silently dropped. A cached-only ID can displace speculative entries, but never a parent entry.
4. Keep the exact existing outgoing RequestMultipleObjects format, one 16-ID batch per 400ms, per-ID backoff and five-attempt/90-second reclamation. Enlarging a memory queue does **not** increase packet rate. Keep deterministic priority + retry ordering and bounded CPU memory.
5. The Seven-in-One panel now labels R68 and displays missing queue pressure as Missing N/4096; retain R66 Near/NoLOD/Parent/Far/BadPose/Undrawn counters and all R67 linked-transform-aware mesh, texture and PBR asset streaming improvements.

## Regression coverage

- Compile-time check: missing-object queue exactly 4,096, no smaller than root-object retention; four mesh workers stay within the scheduler's limit.
- Verify missing linked-object parent requests sort ahead of cached and speculative requests, even if an ordinary cached request has fewer attempts.
- Keep R65 priority/backoff tests, exact bounded simulator wire templates, Windows CTest, and forced Microsoft WARP DX12/PBR pixel tests.
- GitHub Action artifact: **KAMI-R68-4096-Missing-Parent-Recovery-DX12**.

## Important limitations

This is a queue-capacity and prioritization experiment, **not** proof that missing buildings will load. The simulator must actually send valid root updates; unsupported Second Life prim/sculpt/complex volume shapes, unresolved transforms, 56 previously eligible but undrawn meshes, and texture/PBR acquisition remain independent causes of incomplete rendering. After the R68 test, compare Root, Linked, Parent, Missing and Mesh DRAW counters after a fixed amount of in-world time against R67. Keep the 2,048-primitive Quality draw limit until evidence warrants a change.
