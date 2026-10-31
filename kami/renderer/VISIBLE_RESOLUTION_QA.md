# KAMI Viewer — Real 1440p, 4K and 8K visible output

Status: **Experimental; Windows compile and graphics-device testing required.**

## The problem fixed in this branch

Previous builds could allocate increasingly large world framebuffers while leaving
some deferred-lighting shader VIEWPORT parameters in window resolution. The final
display also sampled a larger supersampled image with ordinary bilinear filtering,
which may fail to integrate most high-resolution detail when displaying 4K/8K
on a smaller monitor. That makes FPS drop without an obvious quality gain.

## Changed rendering path

1. KAMI continues to select **true native 1440p (2560x1440)** at a 16:9
   world viewport for first-run RTX 5070 profiles, preserving a previously
   saved manual choice. Integrated/unknown GPUs default to the existing
   **window-native** path for performance.
2. When binding the main world deferred FBO, KAMI now supplies its *actual*
   dimensions to shader viewport uniforms. Reflection probes and snapshots
   are not repurposed into fullscreen passes.
3. Once deferred lighting and SSR are finished, window-native viewport state
   is restored before the scene monitor, HUD, UI and final presentation.
4. The final world-only post shader uses a pixel-footprint **area resolve**:
   - 1x1 (ordinary bilinear) when world and output sizes are equal;
   - 2x2 sampling for modest oversampling;
   - 3x3 sampling for approximately 2x oversampling (e.g. 4K to 1080p);
   - 4x4 sampling for 2.75x+ oversampling (e.g. 8K to 1080p).
   UI/HUD are still rendered natively without smoothing.
5. The Display status reports the real allocated GPU source size,
   the final output window dimensions, and the number of SSAA taps actually
   used. The manual selector retains native/1440p/4K/8K modes.

## Quality expectations

Higher internal resolution can improve narrow edges, fine details and
subpixel reflections, but it **cannot produce more physical display pixels**.
On a 1440p desktop, native 1440p and window-native share the same pixel
count; there will be no magical difference between those two at the same
graphics settings. 4K and 8K may look smoother, especially in still scenes
and small geometry. Changes in lighting, ray tracing, shadow quality and
PBR effects are controlled independently in KAMI's other graphics settings.

**8K is an expensive photo-only mode**, especially on 12 GB GPUs. The extra
texture memory and bandwidth may dramatically reduce FPS or trigger a fallback.
The project makes no claim that every GPU can render 8K.

## Tests

- Python source/selection tests (pre-integration):
  `python kami/renderer/verify-visible-supersampling.py`
- Real Mesa EGL GPU shader compile/link/pixel readback:
  `python kami/renderer/test-supersample-glsl.py`
- Post-integration C++/GPU viewport and UI validation:
  `python kami/renderer/verify-visible-supersampling.py --integrated`
- Windows build and installation plus user QA: startup, avatar/textures,
  zoomed screenshots in identical lighting at 1440p/4K/8K, camera/HUD
  stability, frame times, 8K VRAM and fallback.

### Screenshot A/B/C protocol

Capture the same stationary avatar, location, lighting and camera at each
selected render mode. Use identical KAMI quality and anti-aliasing controls;
compare hair strands, distant edges, thin jewelry prongs and texture text.
Check the Display readout and use high-quality PNG screenshots. A scene with
very few thin edges may show only a small improvement. Do not increase contrast,
saturation or glow per resolution mode merely to force a visible difference.

The new GPU filtering is verified by a 1-of-4 thin stripe test which must
disappear with one center sample but be preserved by a proper 4x4 area resolve.
