# R58 live glTF PBR material pipeline

This build retains R57 transparency fixes and the R55/R56 mesh and texture work.

Implemented material path:

- Preserve per-face material UUIDs from SL RenderMaterial ExtraParams in both full and compressed object updates, including explicit clears.
- Fetch authenticated ViewerAsset material_id assets on a background worker, with bounded caches and transient retries. Decode SL binary LLSD material versions 1.0/1.1 and their glTF 2.0 JSON. Image references must be UUIDs; embedded/remote URLs are not fetched.
- Consume authenticated High31 GenericStreamingMessage material override snapshots: base/emissive factors, metallic/roughness, alpha mode/cutoff, double-sided, texture replacements/clears and per-map transforms. Keep overrides received before their base material and clear stale overrides on object removal/region reset.
- Stream PBR base-color, normal, metallic/roughness, occlusion and emissive images through the existing JPEG2000/DX12 array path.
- Bind deduplicated GPU material records per face. Apply sRGB decoding to color/emissive maps; keep normal and ORM data linear. Use derivative tangent frames for normal maps, GGX lighting, AO, emissive output, unlit materials and independent map transforms.
- Render OPAQUE, MASK and BLEND correctly as distinct paths. Blended world triangles are sorted from far to near and drawn after opaque surfaces, with depth testing and no depth writes. Double-sided materials flip back-face normals. Masked materials share their alpha test with shadows.
- Honor the existing PBR lighting toggle. Diagnostics report decoded material assets, bound material faces and material failures.

Validation includes binary material/override decoding, transactional failure, face assignment/clears, GPU texture selection, and production DX12 pixel readback for maps and alpha modes. The forced-WARP workflow requires PBR pixel verification before packaging.

Rendering limits: this is KAMI's material implementation, not visual parity with the official viewer. Simulator reflection probes/IBL and EEP lighting are not implemented; the existing KAMI sun/ambient environment is used. Sorting cannot perfectly resolve intersecting transparent geometry. Real SL avatar appearance/skinning and terrain materials remain separate unfinished work. Live-region material downloads still require an in-world test.

Protocol references inspected: secondlife/viewer LLRenderMaterialParams, LLGLTFMaterial, LLGLTFMaterialList and GenericStreamingMessage template. New parser/renderer code is independently implemented. PicoJSON is vendored under its BSD license (see LICENSE.PicoJSON.txt).
