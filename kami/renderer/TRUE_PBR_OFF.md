# KAMI True PBR OFF — Legacy Material Renderer Switch

This branch supersedes the **Reduced PBR / Vintage Lighting** prototype. It does not rename reduced lighting to OFF.

## Required behavior

**ON** (default): keep Black Dragon / Second Life's normal modern glTF/PBR material shading, terrain materials, reflections, HDR and emissive effects according to the user's original graphics preferences.

**OFF** (explicit opt-in): reroute existing legacy prim/mesh faces to their original Second Life diffuse / classic material inputs. Do not draw their PBR shader passes. Disable glTF scene PBR draw calls, reflective probe work, SSR, mirror passes, HDR and PBR emissive attachments, and route terrain to the non-PBR diffuse detail shader. The setting changes rendering itself; it is not a post-processing filter or an FPS multiplier.

**Back ON:** restore all saved custom graphics preferences and rebuild draw info and terrain, returning the original material assignments without sending any simulator edits or rewriting mesh assets.

## Content compatibility that cannot be promised

The legacy materials actually supplied by the region's creators determine what OFF looks like. Many modern glTF-only scene objects and PBR-only terrain regions provide **no legacy mesh or texture counterpart**. KAMI does not manufacture missing assets. Such glTF-only scenes may not render when OFF; terrain can lose its original appearance. Texture, alpha, HUD and shadow behavior requires Windows/in-world verification before shipping.

The OFF path still uses the same modern Second Life viewer infrastructure for networking, texture fetch, avatars, non-PBR deferred lighting and sky; no unsupported claims of reverting the *entire viewer codebase* to a historic engine are made. But glTF/PBR **material shader drawing** is actually bypassed when OFF.

## Performance and quality

On lower-powered GPUs PBR OFF may save reflection, emissive and PBR material shading work. Exact FPS gains are **not measured**, vary with scene content, and must never be claimed in advance. This is an intentional visual-quality mode; do not count its FPS difference as a same-quality core-engine optimization.

## Testing gate

- Clean Windows compilation with material fallback enabled.
- Three stages of content: classic diffuse/Blinn-Phong objects, mixed legacy and PBR material objects, and pure glTF PBR-only mesh/terrain.
- Toggle ON -> OFF -> ON while wearing high-complexity rigged attachments, then test texture, alpha mask, transparent surfaces, media, HUD, avatars, shadows and camera.
- No crash or corrupted saved materials. No restarts or external batch/PowerShell scripts.
- Confirm with GPU/profiler captures that PBR draw calls are genuinely absent in OFF.
- Verify standalone default ON, restored settings survive restart, and an unsafe region is explained in UI.
- Only mark a build release-ready when it launches in Windows, validates OFF in world, and does not regress ON mode.

## GitHub implementation

The build script `kami/renderer/apply-true-pbr-off.py` patches the native viewer's material routing and UI prior to compiling. It replaces the previous `apply-reduced-pbr.py` build step. `verify-true-pbr-off.py` statically validates the real render paths. Unit/static checks do **not** substitute for real GPU QA.
