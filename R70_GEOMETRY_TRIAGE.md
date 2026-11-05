# KAMI R70 — live geometry pipeline triage

Base: R69 commit `6780eb532238fe056713a84df1561535d5d2c281`.
Status: **diagnostic specification only — implementation and Windows build not yet complete**.

## Live R69 screenshot (2026-11-05)
- Root 24; Linked 403; Solid 242; Mesh DRAW 217; Missing 4081/4096; Requested 5904.
- MeshID 373; LOD 90; Near 293; NoLOD 11; Parent 79; Far 0; BadPose 0; Undrawn 76; Tris 398574.
- Parent classes: M 51, A 28, C 0, I 0, D 0.
- Undrawn: Alpha 76, Geom 0, Budget 0.
- Texture HTTP 200; failed HTTP 0; decode 0. PBR assets 1; PBR terrain 0.
- GPU atlas 117/512 in this screenshot, **not** 512/512. Do not infer atlas saturation from this capture.

## Implementation targets
1. Distinguish object-request queue occupancy, unique missing IDs, retry attempts, request completions, timeouts, and object IDs actually resolved. Do not interpret 4081/4096 as 4081 permanently missing objects without checking queue semantics.
2. Track mesh asset state by UUID: discovered, queued, HTTP status, bytes received, decoded LOD count, selected LOD, CPU vertices, GPU-uploaded vertices, and final draw disposition. Expose per-stage counters without leaking account/session secrets.
3. For parent class M=51, record bounded anonymized root/local-ID relationships, first-seen age and recovery request count. Exclude A=28 avatar attachments from house-parent retries.
4. For LOD=90, Near=293, NoLOD=11, distinguish asset not fetched, unsupported payload, LOD absent, and invalid bounds; select a valid available fallback LOD only when geometrically appropriate.
5. Preserve Alpha=76 fully transparent surfaces as invisible. Geom=0 and Budget=0 are not the observed cause in this capture.
6. Verify texture-atlas residency and eviction at actual observed occupancy (117/512), not presumed saturation. Track allocations and failed uploads.
7. Add regression tests for parent recovery, retry deduplication, mesh fetch/decode failures, LOD fallback, and DX12 upload disposition. Require Windows build, CTest and forced Microsoft WARP DX12/PBR pixel smoke.
8. Compare R69 and R70 **in the same live SL region** after 30/60/120 seconds: resolved unique objects, drawn mesh instances, triangles, parent M/A, missing mesh IDs, and queue depth. Synthetic WARP pass does not prove the user's house renders.

## Guardrails
Preserve R68 recovery behavior, current rendering budgets, original alpha semantics, existing PBR output and no FPS cap. Do not increase request limits or memory budgets without a demonstrated bottleneck. No fabricated in-world validation.
