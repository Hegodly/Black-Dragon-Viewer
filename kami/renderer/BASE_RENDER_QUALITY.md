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

- World buffers keep `RenderResolutionDivisor=1`, `RenderResolutionMultiplier=1.0`; `KombatantRenderWidth=2560` selects actual 1440-high source render buffers (not temporal or spatial upscaling). The UI stays at its window resolution.
- **True QHD is now the default source resolution** for new profiles: 1440 real GPU framebuffer rows. At 16:9 this is exactly 2560x1440, including when displaying the image downsampled onto a 1080p monitor. At other aspect ratios, maintain 1440 rows and compute width from the live world viewport.
- Do not enable experimental automatic framebuffer resizing; prior in-world testing reported viewport crop/camera shake. The true QHD selection is a fixed, nonadaptive world-buffer dimension and does not change camera projection or the OS desktop mode.
- Do not silently enable VSync or impose an FPS cap. Preserve user-selected resolution, max FPS, and quality settings. If GL rejects the 1440p framebuffer, revert to window-native rendering and display the reason, rather than silently pretending to output true QHD.
- Neutral-mode optimization skips a full-screen identity glow/color composite and glow clear when glow, greyscale, sepia, and posterization are all off; the original shader is used whenever one of those effects is enabled. Default CAS sharpening is zero, so QHD pixels are not sharpened in an additional pass. These do *not* guarantee higher FPS: measure median/p99 frame times and GPU memory.

## Existing installs

These settings are stored per-user, so shipping default changes **does not override** an existing profile. To try the neutral baseline without resetting unrelated preferences, adjust only:

| Debug setting | Value |
| --- | --- |
| `RenderTonemapType` | `0` (Khronos Neutral) |
| `RenderTonemapMix` | `1.0` |
| `RenderGlow` | `FALSE` |
| `RenderDeferredSSAO` | `FALSE` |
| `RenderReflectionProbeCount` | `128` |
| `KombatantRenderWidth` | `2560` (true 1440p world target) |
| `RenderCASSharpness` | `0.0` |

Leave `RenderResolutionMultiplier` and `RenderResolutionDivisor` at `1.0` and `1` respectively for native QHD tests. Choose `KombatantRenderWidth=0` only for an explicit lower-cost window-native fallback; on weaker GPUs this may be needed for usable FPS. Users may turn optional effects back on at any time. Never erase their saved preferences.

## Release validation gates

1. Run `python kami/renderer/verify-neutral-baseline.py` and `python kami/renderer/verify-true-1440p.py` on checkout; after build-time patches, run the QHD verifier with `--integrated`.
2. Compile Windows and validate shader compilation at launch.
3. In the **same Second Life scene**, compare 1920x1080 native-window vs true-QHD 2560x1440 on a 1080p and a 1440p monitor; confirm the actual FBO readout is 2560x1440 in 16:9, no stretching, no crop, and correct 1440 rows in non-16:9 windows.
4. Test materials: PBR metals, dark skin and light skin, fabrics, emissives, dark rooms, sunset, water, and reflective furniture. Check clipping, skin hue, shadow detail, and brightness.
5. Turn optional SSAO, bloom, shadows, SSR, and other effects on *individually*. Reject doubled tone mapping, saturation spikes, shimmer, or more passes than necessary.
6. Validate texture loading and live resolution switching, stable camera movement, correct UI sizing, low-end GPU fallback messaging, reflection-probe allocation, and settings persistence. Verify neutral fast path does not alter glow/sepia/greyscale when enabled.

Only merge after a clean build and in-world QA; this file documents design and static-test targets, not measured FPS results.
