# KAMI: Official Second Life Compatibility Port Map

Upstream: [secondlife/viewer](https://github.com/secondlife/viewer),
LGPL-2.1, pinned `develop` at
`bb143726c780303e8302bfd7699d3d2d95935c39`.
This document is a factual implementation ledger, **not** a claim of full
viewer equivalence. All incorporated original Linden Lab source must
retain compatible copyright/license notices and remain available to
downstream recipients under its governing terms. Read
`third_party/secondlife-lgpl/NOTICE.md`.

## Keep the boundary clear

**Use official Second Life source for protocol, scene, material and
asset semantics. Use KAMI's own renderer for GPU work.**

- **SL wire layer → KAMI core:** verified login identity + machine ID
  bridge; LLUDP zero-code DLL; bounded official ExtraParams metadata
  DLL; LLPrimitive TextureEntry material DLL (R54); LLSD/mesh and
  JPEG2000 readers. No OpenGL context or state is imported.
- **KAMI object database → original DX12:** authenticated object roots,
  linkset poses, per-face image UUID, UV repeat/offset/rotation and
  tint; background J2C decoding; 48-layer resident texture array;
  bounded geometry and GPU copy fences.
- **KAMI UI:** application-native Windows UI and diagnostics, not
  Second Life's UI or OpenGL pipeline.

## Upstream-module audit

| Official viewer module | Current KAMI equivalent | Remaining compatibility work |
| --- | --- | --- |
| `llprimitive.cpp`/TextureEntry | R54 LGPL `KAMI_SL_TextureEntry.dll`, validated 45 faces | Full alpha modes, texgen and glTF/PBR material assignment |
| `llprimitive.cpp`/ExtraParams | Existing LGPL `KAMI_SL_ExtraParams.dll` | Physical/flexible parts, reflection probes, new material data |
| `llmessage` LLUDP zerocode | Existing LGPL `KAMI_SL_LLUdpCodec.dll` | Full template message framing/ack reliability matrix |
| `llmessage`/LLSD & caps | Original KAMI HTTPS/LLSD bridge | Broader EventQueue events, retries, permissions |
| `llprimitive`/LLVolume | KAMI procedural simple prims and decoded mesh LOD | Cut/hollow/twist/taper/sculpt shapes; complete LOD switching |
| `llvovolume`/LLFace | Per-face DX12 diffuse binding and official-compatible UV math | Texgen projections, material alpha, correct UV for complex profiles |
| `llrender`/LLViewerTexture | OpenJPEG + bounded DX12 texture array | True virtual-texture residency, mips, prioritization, full resolution |
| `llvoavatar`/LLVOAvatarAppearance | Local placeholder plus appearance metadata | Skeleton, baked textures, rigged/skinned mesh, attachments |
| `llinventory`/LLInventoryModel | Partial inventory client | Server inventory tree, creators/upload data, permissions |
| `llviewermessage` | Nearby text support | IM/group chat, transactions, online state, inventory events |
| Viewer PBR/glTF | Original KAMI HLSL shaders/settings | glTF materials, normal/ORM, alpha sorting and irradiance |

## DX12 optimization contract

Keep independent source for: D3D12 device/queue; residency/descriptor
heaps; immutable per-world vertex meshes; current 48 x 256² preview
array until texture correctness is verified; incremental per-slice
uploads; one-frame-fenced draw path; WARP software-adapter CI test.
Never reintroduce the legacy OpenGL renderer as a dependency.

## Test levels

1. Windows source-boundary + compile + automated protocol, geometry and
   official-material tests.
2. Forced Microsoft WARP virtual GPU to verify shader/material/resize,
   capture and draw path, with explicit adapter name.
3. Actual live Second Life region A/B tests on an RTX 5070; compare
   mesh-heavy prim appearance and FPS with the official viewer.

A synthetic WARP pass does not establish full Second Life compatibility
or RTX 5070 performance. KAMI continues to require real in-world testing.
