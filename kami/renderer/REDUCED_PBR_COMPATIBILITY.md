# KAMI Graphics: Reduced PBR / Vintage Lighting

**Status: candidate native viewer feature, pending Windows build and in-world testing.** This is a real optional graphics-workload change, not a fake FPS button or a separate batch-file installer.

## What the new Display checkbox changes

**Default (unchecked): Full PBR lighting.** KAMI maintains your existing render configuration, including modern deferred lighting, glTF PBR materials, HDR, emissive information, reflection probes, mirrors and SSR when you individually enable them.

**Reduced PBR / Vintage Lighting (checked):** KAMI snapshots your existing quality settings, then uses the normal native viewer setting handlers to disable these **optional** GPU processes:

- HDR framebuffer path (`RenderHDREnabled`)
- Separate emissive G-buffer (`RenderEnableEmissiveBuffer`)
- Advanced PBR mode (`RenderDisableVintageMode` set false)
- Reflection probe work (`RenderReflectionsEnabled`)
- Screen-space reflections (`RenderScreenSpaceReflections`)
- Mirror rendering (`RenderMirrors`)

These are real native effects and buffers; changing them triggers the existing shader/probe/FBO handlers. On returning to Full, KAMI restores all six saved values exactly, including custom configurations that were already partly disabled. Backup state is persisted between launches. World framebuffer size, textures, draw distance, avatar LOD and PBR assets are *not* implicitly reduced.

## Important limitation: this is not a 2019 legacy renderer

Modern Second Life and Black Dragon still process glTF PBR materials through a deferred scene renderer. This feature **does not remove that rendering pipeline**. It is labeled Reduced PBR rather than PBR Off so residents are not misled.

A true fully non-PBR viewer mode would require:
- Material selection and texture streaming routing to historic diffuse/normal/specular fallback textures for every glTF face, including alpha and HUD cases
- Safe mode changes and geometry invalidation without corrupting the user's materials
- PBR terrain substitutes (Second Life doesn't guarantee a legacy fallback for PBR terrain)
- Separate legacy lighting/forward rendering with correct shadows, depth, selection and compatibility
- In-world testing against content with and without creator-provided fallback textures.

Linden Lab's material documentation confirms missing/incorrect fallback textures are common and PBR terrain can lack a classic representation. We should not silently replace them with inaccurate textures.

## FPS and correctness

Lower GPU work *may* improve FPS where these effects dominate; it is not a guarantee or a preset that reduces basic image resolution. Under reduced mode, metallic reflections and emissive lighting will look different by design. Compare p50/p95/p99 CPU/GPU frame times in a fixed scene. The overall quality-locked renderer benchmark must not count this deliberate quality change as a same-quality optimization.

## Next release acceptance

- Windows compile plus current Windows driver / RTX 5070, integrated and older hardware test
- Switch Full -> Reduced -> Full multiple times; no crash, camera shake, missing UI or blank avatar
- Validate real framebuffer format changes and reflection/probe work, not merely a changed checkbox
- Exact restoration of user-customized HDR, emissive, SSR, mirrors and probe values
- Test persistence over restart
- Verify PBR-only content still displays accurately for the remaining enabled features; do not promise a full non-PBR alternative
- Verify this option is **baked into KAMIViewer.exe** and native Display settings, with no CMD, BAT or PowerShell requirements
