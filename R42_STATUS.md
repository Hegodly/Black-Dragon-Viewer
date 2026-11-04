# KAMI R42 — Mesh Discovery and Full Object ExtraParams Audit

This is a focused native DX12 compatibility build. Not an Unreal migration.

## Tested source changes
- The compressed ObjectUpdate decoder now skips the **86-byte legacy
  particle-system payload** in the exact position specified by the official
  Second Life viewer, instead of declining the entire volume before reading
  mesh ExtraParams. This does not render particles.
- Malformed and truncated legacy particle updates are still rejected without
  searching for UUID-shaped bytes or inventing assets.
- The full ObjectUpdate path now also counts actual ExtraParams, sculpt
  parameters, and mesh-type sculpt parameters, separately from compressed
  updates. Previously F9 showed only the compressed counts, making it
  impossible to see full-update mesh metadata.
- The F9 indicator includes FullExtra, FullMesh, and Particle counters.
- The R42 release label identifies the active code revision.
- The existing authenticated mesh capability, native DX12 renderer,
  caching and object revision FPS improvements are unchanged.

## In-world test
1. Extract to a new folder, start KAMIViewer.exe.
2. Log in and verify F2 says 'EventQueue + mesh granted'.
3. Enter the world on F2, then open F9.
4. Send a screenshot with MeshID, CAP, FullExtra, FullMesh, Particle,
   Sculpt, and Shapes.
5. If MeshID still stays 0, compare FullExtra/FullMesh to separate
   missing simulation content from a compressed packet layout mismatch.
Do not upload signed capability URLs, credentials or private chats.

## Remaining limits
Real world mesh ID acquisition and actual mesh rendering are not yet
confirmed. Textures, Bento avatar, full inventory, and IM are incomplete.
A passing Windows build is not proof of correct online world rendering.
