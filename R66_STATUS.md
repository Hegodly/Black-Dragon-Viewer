# KAMI R66 — mesh visibility diagnostics (no renderer budget changes)

R65 increased accepted real SL world roots from 26 to 213, linked objects from 427 to 1,780, eliminated the full 256-entry missing-object queue, decoded 303 distinct authenticated mesh LOD assets, and recovered binary PBR materials. Yet only 36 mesh objects were drawn. That is a real gap between retained object references, decoded unique mesh assets and actual in-range geometry.

R66 does NOT alter the R65 2,048-primitive Quality cap, DX12 mesh-first vertex allocation, network traffic or GPU texture limits. It instruments every authenticated mesh object with a mesh ID, placing it into a deterministic loading reason:
- Near: resolved, finite, within render range and mesh LOD loaded (eligible to draw).
- NoLOD: nearby valid object/linked mesh anchor but no decoded mesh LOD in CPU cache.
- Parent: unresolved verified hierarchy for a linked mesh.
- Far: valid pose but outside the live render range (144m horizontal, 96m vertical).
- BadPose: invalid position/quaternion.
- Undrawn: in-range ready mesh instances that emitted no triangles, including GPU triangle-budget exhaustion, fully alpha-invisible geometry or invalid/degenerate triangles. Combine with Tris count to narrow the diagnosis.

Each otherwise eligible mesh instance is classified; root and linked instances are treated uniformly. Children now check unresolved parent pose before decoded-asset availability in the diagnostic path; no new geometry is fabricated.

The seven-in-one panel's overlong mesh details line has been replaced by: MeshID | LOD | Near | NoLOD | Parent | Far | BadPose | Undrawn | Tris, ensuring screenshots show the missing-information cause instead of truncating it. All counters reset on live-world clearing.

New unit tests exercise nearby decoded mesh, missing LOD, out of range, invalid quaternion, unresolved child root and triangle-budget deferral.

Windows CI must compile and run all native CTest tests and WARP DX12 PBR smoke. In-world house tests are still required for conclusions about scene completeness.
