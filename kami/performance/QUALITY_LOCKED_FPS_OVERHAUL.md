# KAMI Zero-Compromise Performance Overhaul

**Owner requirement:** maximize **real rendered FPS** and frame-time consistency without silently lowering image quality or scene complexity.

This is a distinct optimization effort on a separate Git branch, based on the native 1440p / 4K / 8K work. It is not a replacement for the existing low-end / Potato / Smart mode settings. Those remain independently selectable, explicit *quality trades* for users who want them.

## Non-negotiable visual invariants

A change cannot be labeled "quality-preserving" if it silently changes any of:

- Main 3D world framebuffer resolution, supersample output, display viewport, aspect ratio, FOV, and screenshot pixels.
- Render draw distance and object/terrain/tree LOD thresholds.
- Avatar limits, mesh complexity, impostor counts, rigging or animation rate.
- Texture decode/streaming resolution, filtering and VRAM-related bias.
- Deferred shadow map settings, AO, reflections, reflection probes, PBR materials, lighting, glow, fog, camera effects, or anti-aliasing.
- Transparency/depth ordering, attachment visibility, HUDs and UI.
- FPS counts by frame generation, interpolation, forced caps or artificial latency hiding.
- Region content: same camera, time of day, visible avatars and assets.

A reversible manual quality reduction can remain a separate feature, but **does not count as success** under this performance-overhaul contract.

## Stage 0 — Build evidence before making low-level changes (committed)

- Preserve the working source/rendering baseline on its existing branch.
- Benchmark **real** frame duration, p50/p95/p99, and 1% low using the Python quality-locked benchmark and optional CPU/GPU timing samples.
- Reject baseline/candidate comparisons with mismatched resolution, scene ID, graphics settings, hardware, driver, frame generation or camera.
- Gather the same real-world scenes on the RTX 5070 12 GB: quiet region, 20+ avatars, a mesh-heavy jewelry closeup with many specular highlights, and native 1440p versus manual 4K.
- Use screenshots from **matching frames and native UI settings**, not different contrast/sharpness per quality mode, to detect regressions.

## Stage 1 — Low-risk efficiency changes with identical output

- Profile main-thread update, world cull/sort, mesh upload, OpenGL state submission, reflection probe work, shaders and present time.
- Avoid repeated **identical** shader binds, driver state calls, buffer reallocations, settings writes and CPU staging copies when proven redundant.
- Cache only immutable data or invalidate correctly when scene, material, camera or GPU context changes.
- Keep all GL work on valid context-owning threads; never blindly move GL calls onto worker threads.
- Validate each optimization with a deterministic scene and pixel comparison before keeping it.

## Stage 2 — Larger improvements, isolated and GPU-gated

- Batch compatible opaque draw calls by material and shader, without changing alpha sort order or selection overlays.
- Evaluate persistent GPU buffers and multi-draw-indirect on hardware that supports them; maintain compatibility with the legacy OpenGL path.
- Consider asynchronously preparing CPU-side geometry and textures where thread-safe; do not change texture resolution or shape.
- Tune expensive fullscreen passes using **mathematically equivalent** operations and proper color-space handling, only if matched output images and performance prove safe.

## Stage 3 — Release gate

- Build CI and shader tests pass.
- Compare median and p99 frame times in all scenarios; avoid trading huge p99 spikes for slightly higher average FPS.
- Inspect high-quality identical-camera PNGs and depth / alpha / reflection output. Reject meaningful visual differences, missing avatars, lower LODs or blurred textures.
- Verify FPS gain on more than one GPU vendor; support integrated graphics through capability fallbacks.
- In-world startup/login, asset loading, camera movement, graphics-mode toggling, HUD and photo testing.
- Do not claim that 100 FPS is guaranteed in all Second Life regions.

## Benchmark usage

Provide the same metadata fields in two JSON files and one `frame_ms` CSV per run. Optional `cpu_ms` and `gpu_ms` fields must contain real profiler timings, not utilization-based estimates. For example:

```json
{
  "scene_id": "identical-stationary-world-and-camera",
  "graphics_settings": {"RenderShadowDetail": 2, "RenderDeferredSSAO": 1, "KombatantRenderWidth": 2560},
  "render_width": 2560, "render_height": 1440,
  "display_width": 2560, "display_height": 1440,
  "hardware_id": "RTX5070-12GB", "driver_version": "record-driver",
  "camera_id": "fixed-camera", "capture_method": "real-rendered-frames",
  "frame_generation": false
}
```

```csv
frame_ms,cpu_ms,gpu_ms
10.0,4.1,8.4
10.5,4.0,8.9
```

Use at least 100 complete frames (prefer many thousands), and record measured GPU timings where supported. The example above is schema illustration, **not benchmark evidence**.

```sh
python kami/performance/benchmark_quality_locked.py \
  --baseline baseline.csv --baseline-meta baseline.json \
  --candidate candidate.csv --candidate-meta candidate.json \
  --out comparison.json
```

A negative p99 / median frame-time percentage means improvement **only if** the recorded hardware, scene and image quality were genuinely held constant. Explicitly document test variance and driver behavior. For initial branch validation, run:

```sh
python -m unittest kami/performance/test_benchmark_quality_locked.py
```
