# KAMI Beyond Reality — GShade compatibility, built-in FX and MXAO

Includes **20 original KAMI FX effects** (MIT licensed) and two demonstration
presets. The default FireGarden.ini activates *no* effects to avoid reducing
FPS or interfering with the built-in tone mapping.

KAMI supports ReShade FX-compatible GShade presets (.ini/.fx/.fxh), but not
necessarily GShade-exclusive runtime extensions. We do **not** distribute
the GShade runtime or its non-redistributable shader packages.

## Enable effects on Windows

Preferences > Display > GShade Preset Compatibility > Activate included KAMI
FX pack (20 effects). Close KAMI before completing the activation script,
then restart, enable KAMI FX and press Home for effect/parameter selection.
Or select Import GShade preset and shader folders, choosing your existing
.ini and Shaders directory. These two options share the *same* optional
ReShade FX runtime; there is never a second graphics injector.

## MXAO

Actual iMMERSE MXAO is included as a **ready-to-install option** in Display
under Private MXAO, with strength, radius, quality and debug controls. Its
author, Pascal Gilcher, prohibits public redistribution, so the actual
MXAO shader cannot lawfully be embedded in KAMI's public ZIP. The installer
fetches it directly from the author's own pinned source for a **personal**
installation. It runs only if you enable MXAO, is off by default, and requires
in-world depth-buffer verification. MXAO personal installation and KAMI FX
share a single runtime: select one preset configuration per portable folder.

## Performance and compatibility

All optional FX and MXAO are OFF by default. No FPS cap is added, no extra
shader DLL hook is installed, and Fire Garden UI skins all toggles. Some
effects may significantly reduce performance, particularly at native 4K;
depth processing, optical flow and screenshot behavior need GPU validation.
The tool does not change native 4K resolution or tone mapping.
