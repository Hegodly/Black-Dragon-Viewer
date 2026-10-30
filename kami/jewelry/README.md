# KAMI Native Diamond Glints — experimental

This opt-in system adds a tight, light- and camera-responsive specular lobe
to the **existing deferred PBR lighting pass**, after normal-map shading and
reflection probes. It does not require GShade and adds no full-screen pass.

## Enable
In KAMI Preferences > Display, enable **Diamond sparkle (experimental)**.
Adjust **Sparkle strength** from 0–2. The **Subtle rainbow diamond fire** option
adds an artistic chromatic shift; it is not physically correct dispersion.
The controls are disabled by default; Potato and Performance modes bypass
them. Reflection probe cube captures do not include the glint.

## Current material selection limitations
SL PBR materials have no standardized "diamond" classification. The first
version therefore *approximates* gem selection via authored PBR attributes:
light, nearly neutral-colored, low-roughness and nonmetallic materials.
Other glossy white surfaces may qualify, and dark gemstones or materials
without PBR may not. There is no per-object whitelist in this iteration.
The real stone facet or normal map must produce variations in surface normals;
flat surfaces won't magically acquire detailed gem cuts.

The effect responds to the viewer's direct sun/moon direction and camera
orientation and fades beyond 7–24 meters. It does not add local point-light
glints, full-spectral refractive diamond optics, screen-space starbursts,
or new uploaded materials. Those are future experiments.

## Validation
CI runs a standalone GLSL fragment smoke test and asserts the build-time
wiring, visibility gates, native toggles, layout bounds, and defaults.
That is **not** proof that shader assembly, on-screen glint appearance,
frame time, or visual stability work on actual hardware.

Test a PBR pavé Cuban chain against non-gem metallic chains and white
plastic/ceramic. Compare on/off, shadows, head rotation, daylight/night,
faster camera movement, LOD changes, GPU frametime and display presets.
Do not call this FPS-neutral or production-ready before measured tests.
