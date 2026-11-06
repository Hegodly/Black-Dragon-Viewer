# KAMI R87: persistent world geometry migration

Status: Phase 1 — architecture contract and resource accounting. **Not yet a complete renderer replacement.**

## Verified problems in the current renderer
- `Renderer::update_live_world` constructs a combined `SceneMesh` from terrain, proxies, decoded meshes and primitive shapes.
- `build_live_mesh_geometry` imposes a 1,000,000-triangle limit, and `kLiveSceneVertexLimit` is 4,000,000 vertices.
- `make_geometry()` uploads the combined geometry again whenever the scene changes (unchanged scenes are already skipped).
- `decoded_mesh_assets()` exposes decoded mesh LOD data, but that does not imply persistent per-asset GPU allocations.
- The existing R86 Global counter measures geometry selection exclusions; it does **not** prove every missing building is caused by the triangle budget.

## Replacement contract
1. **Asset cache:** decoded asset UUID + LOD revision -> immutable GPU vertex/index buffers, with explicit residency and upload status. Never duplicate a mesh asset's GPU data merely because multiple scene instances reference it.
2. **Instance table:** simulator object/local ID, parent ID, asset UUID, transform, material, visibility, selected LOD, and last-seen region generation. Maintain distinct root/child instances.
3. **LOD policy:** use actual available SL LODs; never synthesize building geometry. When capacity is tight, downgrade suitable objects before dropping them.
4. **Residency manager:** budget GPU bytes and staging bytes separately; evict unused assets by recency and scene visibility, never while GPU work references them.
5. **Renderer:** submit visible instances against cached asset buffers in one DX12 pipeline; terrain, water, and avatars remain independent resources. Keep the old renderer available behind a development flag until parity tests pass.
6. **Texture residency:** allocate/evict by texture identity and material usage, not by one permanently saturated fixed layer table.
7. **Evidence:** report simulator-discovered instances, asset requests, decoded assets, resident assets, GPU-upload failures, culled instances, missing parent/LOD, and render submissions independently.

## Acceptance tests
- Two instances of one asset upload one immutable geometry buffer.
- Parent/child transforms and nonuniform scaling match the reference viewer.
- A dense quarter-region scene retains buildings after furniture/foliage arrivals.
- LOD downgrade preserves object visibility under a fixed byte budget.
- Teleport/region change invalidates stale instance IDs and retains only safe reusable asset data.
- Texture and alpha surfaces remain visible, including cutout foliage and glass.
- WARP DX12 smoke test and real SL scene capture both pass.

No claim of full scene completeness or FPS parity is valid until these tests run against actual in-world data.
