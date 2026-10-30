# KAMI Viewer GPU-first renderer: design objective

## Definition of "90% GPU / 10% CPU"
The project owner wants the **graphics rendering pipeline to be GPU-first**. "90/10" is a design philosophy, **not an achievable universal utilization ratio or a guaranteed FPS result**. CPU and GPU usage percentages are not comparable workload shares; a single saturated CPU thread can bottleneck the viewer while overall CPU usage looks low.

**Performance target:** aim for 100 rendered FPS (10 ms frame budget) in named repeatable scenarios and on specified supported hardware, never promise 100 FPS on every GPU or in every Second Life region.

## Constraints
- Current Second Life/Black Dragon lineage uses OpenGL; Vulkan/DX12 migration is a separate major renderer port, not something achieved by changing a setting.
- Server-synchronized scene updates, network protocol, inventory/UI, content decoding, scene graph maintenance, and other non-rendering tasks still require CPU work. Do not promise to shift those wholesale onto the GPU.
- Untrusted creator-supplied mesh, texture, particle, and avatar workloads vary widely. GPU VRAM and CPU time limitations exist.
- Maintain a compatibility path for older or unsupported integrated GPUs. Never assume compute shader or multi-draw-indirect support without capability probing.

## Staged implementation

### Stage A — Instrument before changing render architecture
- Record per-frame **CPU main-thread submission time**, **GPU pipeline elapsed time** via timestamp queries where supported, frame time, draw calls, state changes, vertex/index counts, and VRAM usage.
- Show a **CPU-bound / GPU-bound / mixed** diagnosis, but do not infer this from GPU utilization alone.
- Publish privacy-respecting, opt-in benchmark templates: no upload of account, avatar identities or region coordinates.
- Record median FPS, 1% low FPS, and frame-time p95/p99 on 1080p fixed scenes.
- Add 100-FPS adaptive quality target that avoids oscillation and never replaces texture-detail preferences silently.

### Stage B — Low risk, broadly compatible
- Profile and batch like-material opaque geometry and reduce redundant OpenGL state changes/draw calls.
- Cache static world geometry on GPU; avoid repeated per-frame CPU regeneration of unchanged buffers.
- Offload appropriate CPU jobs to background worker queues without making OpenGL calls outside safe contexts.
- Distance-aware, preference-respecting avatar impostors, mesh LOD and update frequency; prioritize close faces/hair/outfits.
- Efficient native shaders; no in-process GShade/ReShade dependency or CPU-readback post-processing loops.

### Stage C — GPU-driven path for supported hardware
- Capability gate for compute shaders, shader storage buffers and **multi-draw indirect**.
- Feed object bounding spheres/transforms and draw metadata into GPU resident buffers.
- Run GPU visibility/frustum-culling compute; produce indirect draw commands; use MDI for safe opaque geometry batches.
- Avoid GPU-to-CPU readback between visibility and draw submission; use correct memory barriers, synchronization, and GPU fence timing.
- Gradual optimization for particles, eligible static objects, and avatar skinning **only if profiling proves gains**.
- Leave unsupported materials, transparent sorting, complex rigging, HUDs, and legacy capabilities on the existing working path until migrated and verified.
- Include force-disable switch, logging, and fallback to native legacy drawing if device initialization or validation fails.

### Stage D — Research rather than promise
- Evaluate Vulkan as an eventual alternative backend **only after** the existing viewer is reliable. It is a large multi-year-scale engineering scope, not a toggle.
- Experimental SSGI, skin subsurface scattering, ray tracing, GPU occlusion and frame generation are quality features; they must never be necessary for Performance Mode.

## Acceptance metrics
1. No startup regressions, missing textures, or crashes in quiet and crowded Second Life scenes.
2. GPU-driven path reduces **measured CPU frame cost** and/or draw submission overhead over legacy in an identical scene.
3. Maintain or improve frame-time p99, not only peak FPS.
4. Preserve correct world geometry, avatar materials, alpha effects, selection, screenshots, and deferred rendering.
5. Disabling GPU-driven mode restores the existing renderer without losing user settings.
6. Test across older integrated GPU, old GTX, recent RTX, and AMD devices with driver versions documented.

References:
- Khronos OpenGL indirect rendering documentation: https://wikis.khronos.org/opengl/Vertex_Rendering
- Khronos multi-draw-indirect sample and CPU comparison: https://docs.vulkan.org/samples/latest/samples/performance/multi_draw_indirect/README.html
