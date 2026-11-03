# KAMI Beyond Reality — DX12-only 3D rendering contract

## Backend decision

**All 3D world content, including eventual real SL terrain, objects,
avatars and effects, must use the existing DirectX 12 device, graphics
queue, HLSL shaders and scene resource pipeline.**

- No OpenGL, Direct3D 11, Vulkan or mixed graphics renderer fallback.
- 2D application chrome uses Windows Direct2D/DirectWrite independently;
  these are UI libraries, not an alternative 3D world renderer.
- `Device::initialize(false)` is now the **hardware DX12 default**.
  The optional DX12 WARP software adapter may be selected **explicitly**
  by the automated smoke executable for diagnostics. This does not
  introduce another renderer backend or claim practical FPS on WARP.
- The D3D12 API supports hardware exposing feature level 11_0:
  **D3D_FEATURE_LEVEL_11_0 here is not Direct3D 11 or a D3D11 backend**.
  Advanced features must be gated by a real DX12 feature query.

## Live region ingestion bridge added

KAMI's DX12 `Renderer::set_decoded_region_geometry` can receive
an **already decoded, static** set of world triangles. It reuses the same
geometry buffer, shadow pass, material shaders, depth resources and
DXGI swap chain as the procedural offline test scene. There is no
special legacy or parallel renderer for live scene data.

Before GPU upload, the gate checks an in-world session state, a verified
data flag, triangle-list consistency, a 750,000-vertex per-batch limit,
finite positions/normal vectors/colors/UVs, valid material ranges and
the absence of unsupported rigging in this static path. Failed
updates keep the previously valid GPU geometry alive.

An in-world session in the current unit tests is **synthetic**; the
Bootstrap contract uses bool verification gates and has no production
authenticating LLUDP event adapter yet. This test is a validated
*interface contract*, not proof of authenticated SL connectivity.
`clear_decoded_region_geometry()` clears the server triangles when
the future session manager disconnects. That disconnect hook is not
wired because no actual session manager exists yet.

## Not implemented

1. Real authenticated account login and consent/MFA/ToS UX.
2. Simulator UseCircuitCode/agent handshake and verified event queue.
3. Official SL terrain, sculpt, mesh, texture, material and parcel decoders.
4. Region origin / coordinate rebasing, object-update delta application,
   frustum/LOD culling and scene graph.
5. SL skeleton and Bento skinning with actual rig matrices, animations,
   attachments and worn physics settings.
6. Live asset HTTP fetching, texture transcoding, persistent cache,
   prioritized scheduling wired to real DX12 GPU upload.
7. Live region benchmarks against other viewers.

**Do not advertise the viewer as able to render live Second Life
regions until these stages are implemented and actually tested.**
The local DX12 scene's measured frame rate is not a live SL
region performance claim.
