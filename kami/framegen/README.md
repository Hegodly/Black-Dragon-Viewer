# Experimental native optical-flow frame generation

This is a standalone OpenGL prototype, not NVIDIA DLSS Frame Generation or AMD
FSR Frame Generation. No vendor runtime is loaded. Do not advertise this as a
validated performance feature or a guaranteed 2x FPS increase.

## Implementation

After the viewer finishes drawing world and UI, capture the backbuffer to a
ping-pong RGBA8 texture. A low-resolution RGBA16F pass estimates forward image
motion with a three-level, nine-candidate block search and nine luminance samples
per candidate. A full-resolution pass samples previous/current frames at half
that displacement. Residual-based confidence falls back to current content on
uncertain blocks. Present that intermediate image, restore the exact current
image, wait half the measured render interval, and present it. Generated frames
never enter rendering history or viewer simulation/FPS statistics.

Capture occurs on the GPU; there is no CPU image readback. CPU code schedules the
two synchronous presentations. The previous generation/presentation time is
excluded from the next cadence estimate to avoid recursive delay growth. Actual
render throughput can decrease. This is not a dedicated asynchronous presenter;
uneven pacing, VSync blocking and added input delay need Windows measurements.

The Display setting starts off every session. Bypass on snapshots, minimized or
invisible windows, Performance Mode, intervals outside 30–240 rendered FPS and
sizes above 4096 per axis. Resize and GL context teardown clear resources/history;
shader/FBO failures retain normal presentation. OpenGL state is restored after
processing, including programs, VAO, FBOs, viewport, textures, samplers, raster
mode, unpack buffer and affected enables.

## Limits / release gates

- UI is included in the optical-flow input. Static pixels are stable, but moving
  HUDs, name tags, text and cursor can distort. A separate UI composition path is
  required for a production-quality implementation.
- No geometry motion vectors, depth disocclusion, optical-flow hardware, temporal
  stabilization or global camera-cut detection. Fast motion beyond the search
  radius, transparency and scene cuts may produce bad intermediate frames.
- Added GPU work, two swaps and a synchronous sleep can reduce real FPS. Generated
  presentations do not increase simulation, input responsiveness or asset loading.
- Full Windows viewer compilation, launch, GL state checks in-world, resizing,
  focus loss, toggling, context recreation, 60/120/144 Hz VSync, integrated GPUs
  and RTX 5070 pacing/image-quality validation are required before release.

## Validation

`python kami/framegen/test-shaders.py` uses headless Mesa/EGL to compile/link and
execute the actual embedded GLSL. It verifies nonzero optical flow for a moving
edge, an intermediate image differing from the current frame, an advancing edge,
static background preservation, exact original-frame restoration and no GL errors.
This is a shader-level check, not validation of the Windows viewer or its pacing.

`apply-framegen.py` runs after all existing native graphics patches in the Windows
workflow. A clean patch-sequence application and settings/panel XML parse were
verified locally. Windows CI builds the integrated viewer; the PR remains draft.
