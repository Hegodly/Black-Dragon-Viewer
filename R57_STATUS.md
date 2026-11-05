# R57 texture sampling and invisible-surface fixes

R56 texture budgets and R55 mesh fixes are retained.

- Sampled texture alpha was ignored. Fully invisible texels now discard their color/depth fragments; the shadow pass samples the same alpha so invisible texels do not cast solid shadows.
- Primitive and static mesh generation now omits faces with a fully invisible TextureEntry tint. Opaque neighboring faces remain intact.
- The JPEG2000 decoder now supports grayscale-plus-alpha images, preserving both grayscale and coverage instead of rejecting these valid two-channel assets.
- Synthetic GPU tests upload a known magenta image through the production uploader to the highest array slot. Color readback requires actual magenta output for the opaque image and none for the invisible image. Shadow depth readback checks that only opaque geometry writes depth. A draw-success flag or texture counter alone cannot satisfy these checks.
- Regression fixtures cover primitive/mesh face invisibility, opaque neighbors, grayscale-alpha decode and transactional failure.

Limits: This patch handles fully invisible texels/faces. Partially transparent blending, per-material alpha-mask thresholds, complete glTF PBR materials, actual terrain materials and SL avatar appearance/skinning remain unfinished. Correct texture sampling in a synthetic test does not establish the cause of every gray surface in a live region. In-world comparison is still required.
