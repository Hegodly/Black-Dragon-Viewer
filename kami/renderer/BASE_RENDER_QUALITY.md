# KAMI native base renderer: clean PBR baseline

Scope: Black Dragon / Second Life OpenGL base renderer. These are defaults for **new settings profiles**; saved user choices win, and effects remain manually available. No proprietary renderer or externally hooked GShade pass is necessary for the base image.

## Visual behavior with optional effects disabled

- **Khronos Neutral** tone mapping at full mix (0 / 1.0) prioritizes predictable skin, fabric, and metallic colors over added cinematic saturation.
- **Native PBR/deferred lighting and reflection probes remain available.** Do not disable essential PBR support just to improve FPS.
- **Bloom, SSAO, SSR, motion blur, and depth of field are OFF by default**, each still independently selectable. Do not force sharpen/saturation/contrast.
- **Reflection-probe budget defaults to 128**, versus 256 previously. Higher-quality presets may request larger budgets.
- Existing built-in **SMAA** stays enabled by default to keep edges clean.
- Reinhard tone mapping (an alternate option) now guards pure black pixels against divide-by-zero and respects exposure when exposure is active.

## Display resolution and performance

- World buffers default to the *display's native size*: `RenderResolutionDivisor=1`, `RenderResolutionMultiplier=1.0`.
- **2560x1440 is the preferred QHD target on capable displays and GPUs**, not a forced 1440p internal resolution on 1080p screens or weak GPUs.
- Do not enable experimental automatic framebuffer resizing; prior in-world testing reported viewport crop/camera shake.
- Do not silently enable VSync or impose an FPS cap. Preserve user-selected resolution, max FPS, and quality settings.
- Better defaults do *not* guarantee higher FPS: measure GPU frame time (median and p99), CPU frame time, memory, and in-world visual quality.

## Existing installs

These settings are stored per-user, so shipping default changes **does not override** an existing profile. To try the neutral baseline without resetting unrelated preferences, adjust only:

| Debug setting | Value |
| --- | --- |
| `RenderTonemapType` | `0` (Khronos Neutral) |
| `RenderTonemapMix` | `1.0` |
| `RenderGlow` | `FALSE` |
| `RenderDeferredSSAO` | `FALSE` |
| `RenderReflectionProbeCount` | `128` |

Leave `RenderResolutionMultiplier` and `RenderResolutionDivisor` at `1.0` and `1` respectively for native-resolution tests. Users may turn optional effects back on at any time. Never erase their saved preferences.

## Release validation gates

1. Run `python kami/renderer/verify-neutral-baseline.py` on checkout and after applying build-time KAMI patches.
2. Compile Windows and validate shader compilation at launch.
3. In the **same Second Life scene**, compare before/after on native 1080p and native 1440p, with optional effects off.
4. Test materials: PBR metals, dark skin and light skin, fabrics, emissives, dark rooms, sunset, water, and reflective furniture. Check clipping, skin hue, shadow detail, and brightness.
5. Turn optional SSAO, bloom, shadows, SSR, and other effects on *individually*. Reject doubled tone mapping, saturation spikes, shimmer, or more passes than necessary.
6. Validate texture loading and live resolution switching, stable camera movement, correct UI sizing, low-end GPU compatibility, and settings persistence.

Only merge after a clean build and in-world QA; this file documents design and static-test targets, not measured FPS results.
