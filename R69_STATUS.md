# KAMI R69 — precise in-world geometry failure classification

R68 is the first independently observed major live-world loading breakthrough: 546 authenticated roots, 4,453 linked objects, 1,536 mesh instances drawn, 619,416 real triangles, 1,653 eligible mesh instances, and 112 of 4,096 queued object requests. Earlier R67 drew 174 meshes with the queue saturated at 1,024/1,024. Preserve the complete R68 recovery path and Quality's 2,048-primitive cap.

## Why a diagnostic-only R69

The R68 panel's `Parent 584` count was misleading: it increments for **any** linked mesh whose hierarchy does not resolve, including genuinely missing parent objects, avatar attachments, hierarchy cycles, invalid poses, and chains exceeding 16 links. Those do NOT all represent separate requests for missing roots. Likewise, `Undrawn 117` includes triangles deliberately skipped for genuine alpha=0, malformed/zero-area geometry, and a genuinely exhausted triangle/vertex budget. It is unsafe to infer a need for further network queue increases or shader budget changes from those aggregate counts alone.

## Changes

- Add explicit classified mesh parent counters: **M**=missing parent/root; **A**=avatar-attached linkset (not a house); **C**=hierarchy cycle; **I**=invalid transform/quaternion; **D**=too-deep chain.
- Add exclusive non-draw categories: **Alpha**=all triangles invisible (face alpha zero); **Geom**=all triangles degenerate/invalid; **Budget**=mesh instance deferred by max-triangle budget. Cases are not artificially converted to visible geometry; original material and vertex output are unchanged.
- Show these causes in the Seven-in-One F9 panel's existing Security and Performance section second line, leaving Root / Linked / Mesh DRAW / Missing queue and the main Near / NoLOD / Parent / Undrawn / Tris line intact.
- Reset the counters on region clear; keep the same DX12 vertex/triangle, texture atlas, cache, download, and packet-request limits.
- No extra SL network packets, no FPS cap changes, no new shader or texture allocations.

## Tests

Unit regressions check avatar parent classification, a truly absent parent, a parent self-cycle, fully transparent mesh faces, degenerate mesh triangles, and a mesh instance deferred solely by zero triangle budget. CTest also retains the all-R68 authenticated LLUDP, mesh LOD, PBR, linkset and WARP DX12 tests.

## What to inspect next

Compare the screenshot's R68 `Parent 584` with R69's **M/A/C/I/D** counters, and R68's `Undrawn 117` with **Alpha/Geom/Budget**. This identifies whether to fix object parent recovery, avatar attachments, broken hierarchy parsing, missing alpha support, geometry degeneracy, or budget pressure. A large **A** cannot be fixed by increasing the missing object queue; many fully invisible **Alpha** objects should not be falsely rendered. The GPU texture atlas already shows **512/512** and needs its own residency/paging investigation, not an unbounded slice increase.

R69 is a diagnostic test build, not proof that the whole house now renders.
