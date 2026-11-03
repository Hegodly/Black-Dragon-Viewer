# KAMI advanced avatar physics — code-level comparison and integration status

Comparison reviewed on 2026-11-03 using *public source files*:

- Official Second Life viewer: [indra/newview/llphysicsmotion.cpp](https://github.com/secondlife/viewer/blob/develop/indra/newview/llphysicsmotion.cpp)
- Firestorm: [indra/newview/llphysicsmotion.cpp](https://github.com/FirestormViewer/phoenix-firestorm/blob/master/indra/newview/llphysicsmotion.cpp)
- KAMI: [src/features/body_physics.cpp](../src/features/body_physics.cpp), [src/features/rig_physics.cpp](../src/features/rig_physics.cpp), [src/features/rig.cpp](../src/features/rig.cpp), [include/kami/world/shaders.hpp](../include/kami/world/shaders.hpp)

This document is a behavioral analysis, not a transfer of licensed code.
All KAMI code is independently authored. The SL and Firestorm source files have
LGPL copyright notices and **must not** be pasted into proprietary KAMI.

## Comparison table

| Capability | Official Second Life / Firestorm | Original KAMI branch |
|---|---|---|
| Physics channels | Breast cleavage/in-out, up/down, left/right; butt up/down and left/right; belly up/down | **Six** original spring channels with independent state and bounded offsets |
| Motion driver | Derives motion from joint world position, projects along body-relative directions, evaluates velocity and smoothed acceleration | Local locomotion phase, speed changes, heading changes, jump velocity, and landing events; world-joint motion adaptation still needed |
| Wearable input | Resolves named in-world avatar visual parameters through established viewer avatar/wearable stack | Validates named parameters passed by a hypothetical decoder; **no real SL wearable asset decoding or normalization yet** |
| Output | Drives temporary avatar visual parameters and updates avatar visuals | Builds independent per-bone translations for test LEFT_PEC, RIGHT_PEC, BELLY and BUTT joints |
| Actual rigging | Real avatar appearance, joints, meshes, and fitted mesh workflows | Eight-joint **procedural test rig only**; four influence GPU linear blend skinning, neutral bind pose and per-joint hierarchy |
| Rendering | Current viewer rendering stack | DX12-only dynamic structured GPU palette, shadow and lit pass shared skin deformation |
| Safety | Has global physics enabled setting, delta guards and iteration limiting | Input finite checks, bounded influence indices and physics effects, fixed 8 ms simulation steps, malformed mesh rejection |
| Performance | Updates motion state each active frame and updates visuals when changed; Firestorm includes a small optimized iteration change | Local-avatar frame updates, CPU-to-GPU transfer of 8 bone matrices; **distance-based remote-avatar LOD budget policy only, not live remote scheduling** |
| Compatibility reporting | Render result depends on avatar and mesh physics weights | Explicit Unrigged / Partial / PhysicsReady / Invalid weight audit for visible test geometry |
| Server integration | Authenticated SL grid, inventory, avatar and animation pipeline | **Not yet integrated** |

## KAMI-specific engineering choices

1. Four normalized influences per vertex are validated, quantized into four
   UNORM8 weights with their byte sum forced to 255, and packed with
   eight-bit joint indices.
2. Rest pose offsets and joint hierarchy are solved once into a small
   per-frame palette, and weighted **position + normal** transformation
   uses the same palette in camera and shadow passes.
3. Invalid or non-finite weights, out-of-range joint indices, and bad mesh
   region declarations are rejected before GPU upload.
4. Separate profile OFF zeros the physics state, not merely the UI.
5. A policy exposes full resolution at camera distances under 12 m,
   half-rate at 12–26 m, quarter-rate at 26–44 m, and no extra simulated
   motion beyond 44 m. It is **not currently connected to live avatars**.
6. Named wearable parameter input can be injected by a future decoding
   service but **cannot yet interpret an actual SL physics wearable asset**.
   Raw SL visual parameter weights are not guaranteed to be in KAMI
   simulation units, so a versioned conversion and server-linked tests
   are still needed.

## Missing before declaring live SL physics

- Authenticated Second Life login and appearance, inventory, wearable
  asset retrieval, and correct decoding of avatar visual parameters.
- Full official SL skeleton/Bento bone list, appropriate real fitted-mesh
  inverse bind matrices, skin weights, geometry/LOD, and attachment updates.
- Animation keyframes, joint hierarchy mapping, and actual joint-relative
  acceleration drives (not just test-world locomotion).
- Multi-resident avatar instance management, attachment compatibility,
  physics visibility for third parties, and real performance measurements
  against modern viewers on identical scenes and hardware.
- Compliance/licensing review of the closed-source distribution model,
  networking protocol and official SL third-party viewer policies.

**Do not claim this branch surpasses other viewers** without cross-viewer
benchmarking, avatar body tests, and a reliable online session.
