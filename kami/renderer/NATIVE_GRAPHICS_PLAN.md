# KAMI Viewer — native graphics implementation roadmap

This is a renderer engineering plan, **not a list of shipping features**. KAMI uses the existing OpenGL/PBR pipeline; the bundled GShade/ReShade runtime is removed from the native-performance branch. Frame pacing and launch stability are release gates.

## Currently integrated in source (experimental until compiled/in-world validated)
- Native **Performance Mode** with reversible stored settings (distance, avatar/object LOD, shadows, AO, SSR, volumetrics, FXAA, native resolution; disables optional D3D upscaler CPU copies).
- **Adaptive world resolution** only while Performance Mode is enabled. Samples frame rate over 3 seconds, adjusts the native 3D render scale by 0.05 increments between 0.55 and 1.0 to approach 30/45/60/90/120 target. Does not scale UI, replace upscalers, or guarantee a frame rate, especially in CPU-bound crowds.
- Native HBAO, volumetric lighting, high-quality bokeh DOF, SSR/reflection probe quality, lighting and bloom controls exposed in KAMI graphics panel (built on existing upstream shader paths).
- Optional **four-tap PBR near-field color-bounce approximation** using existing deferred GBuffer normals, albedo, depth and positions, with a custom strength uniform. This is not SSGI, does not ray march or denoise, can leak color and is disabled by default.

## Next: true depth-aware screen-space GI (SSGI)
1. Preserve an HDR lighting target **before** tone mapping and final postprocess, with depth, normal, PBR albedo and motion vectors.
2. Implement half-resolution stochastic hemisphere ray marching through a hierarchical depth pyramid; reject invalid/offscreen/sky hits, sample **HDR radiance** at successful hits.
3. Temporal accumulation with reprojection using existing velocity buffers; detect disocclusion via depth and normal rejection; include motion blur and DLSS/FSR/XeSS camera jitter consistency.
4. Bilateral edge-aware spatial denoiser and radiance clamp to prevent fireflies; composite diffuse indirect lighting once (avoid double AO).
5. Quality settings: Off / 4 rays / 8 rays / 16 rays, quarter/half resolution, max ray distance and history length. **Always OFF in Performance Mode.**
6. Gate on GPU timings, test reflective metal, interior walls, dark rooms, rapid camera motion, moving avatars, and sky boundaries.

## Next: skin-only subsurface scattering (SSS)
1. Add an explicit **skin material mask** to the GBuffer or small packed material flag; do not guess that every beige or red pixel is skin.
2. Do a screen-space **separable depth- and normal-aware diffusion** on the lit diffuse component, preserving specular highlights and texture detail, and avoiding skin/clothing/eyelash cross-contamination.
3. Give RGB channels different diffusion widths (red broader) and a physically constrained strength/radius in scene units.
4. Run at half resolution if suitable; optionally temporal stabilize; keep native PBR and default avatar rendering unchanged unless enabled.
5. Test multiple skin tones, body/head seams, alpha hair, tattoos, face closeups, and fast movement; user must be able to turn it off with one click.

## Next: more accurate AO and contact shadows
- Existing HBAO is a native fallback. For GTAO, add horizon-angle integration in view space using the depth pyramid, bent normal support, bilateral denoising and adaptive sample count.
- For small contact shadows, depth-ray march toward visible direct lights in screen space, fade gracefully offscreen. Use scene depth, not a fake dark vignette.

## Next: reflections and atmospheric rendering
- Add adaptive SSR ray budget plus probe fallback for misses. Reflection-probe update cadence and count are performance-critical; prevent stalls in crowded scenes.
- Volumetric lighting already exists in this fork. Improvement work would add jittered low-res scattering + reprojection and temporal stability; disabled in Performance Mode.
- Cinematic camera: use the existing high-quality DOF, bloom, exposure and color grading before adding new lens kernels, LUT handling or multi-pass photography.

## Performance acceptance gates
- FPS target is a **goal**, not a hard FPS limiter. VSync/CPU bottlenecks and SL avatar load may prevent hitting it.
- For every effect measure median and 1% low FPS and GPU frame times against the same camera path in empty and crowded regions, at 1080p/1440p/4K.
- Validate graphics fallback and texture loading on NVIDIA, AMD and Intel GPUs. Optional RTX/DX12 upscalers currently transfer buffers through CPU: measure separately and never enable automatically.
- Ensure snapshots preserve their requested output size and switching modes does not corrupt user settings.
- Reject a release if the Windows viewer does not compile, launch, render avatar textures, and exit without warnings.

## Third-party and attribution policy
Retain Black Dragon, Linden Lab and contributed-source licensing and notices. New native code should not require a proprietary shader injection layer.
