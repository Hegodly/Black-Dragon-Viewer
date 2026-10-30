# KAMI native visual quality tiers (experimental)

## Problem
The original Low, Medium, High, Ultra and Cinematic selector primarily changed
geometry budgets, lights, texture limits, draw distances and maximum avatar
counts. A close camera shot often looked nearly identical even when measured FPS
differed. GShade could mask subtle SSAO/lighting changes.

## Visual difference introduced
The five preset levels now also select distinct PBR specular quality response
inside Kami's **existing** deferred lighting shader. Low and Medium make glossy
PBR materials look less reflective; Ultra and Cinematic retain much sharper
authored highlights and more probe contribution.

| Preset | PBR min roughness | Reflection-probe scale |
| --- | ---: | ---: |
| Low | 0.34 | 0.25 |
| Medium | 0.22 | 0.55 |
| High | 0.13 | 0.85 |
| Ultra | 0.07 | 1.00 |
| Cinematic | 0.02 | 1.10 |

This is a deliberate visual quality approximation, **not** a faster BRDF
implementation or proof of better FPS. No extra render pass, no new GPU buffer.
GShade files, gamma, tone mapping, user's actual world resolution, frame cap,
and uploaded SL materials are not modified. Legacy non-PBR avatar skin will
not see the same direct effect.

When the graphics preset is Custom, Potato/Performance, Auto Tune or Crowd
Mode is active, native material fidelity is bypassed to avoid conflicting
quality managers. The user-facing status describes the **last selected**
preset, not an FPS benchmark or proof that every renderer setting stayed fixed.

## Validation
- Compile the PBR helper with glslangValidator on Linux.
- Run the full existing native source contract tests after integration.
- Run the Windows viewer build to validate shader and C++ integration.
- Compare **the same camera shot**, with GShade disabled first and then enabled:
  PBR jewelry, glossy metal, normal-mapped objects, shadows/SSAO, nearby avatars,
  and textured terrain. Observe glints/reflections under changing light.
- Measure GPU frametime, not just FPS: native shader quality is only one of
  several render costs and may have negligible speed difference.

Not yet validated live on every GPU. If a shader compiles but the scene
looks unchanged, inspect the in-world material's PBR status and the
selected preset, and ensure no automatic quality controller is active.
