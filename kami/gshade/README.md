# KAMI Beyond Reality - External GShade/ReShade support

KAMI's built-in GShade compatibility and private MXAO controls were retired
after in-world testing. KAMI does not inject, bundle, load or manage a
GShade or ReShade runtime. The 20 original, MIT-licensed KAMI-FX techniques
and FireGarden/Cinematic/Clean4K presets are still included as optional assets.

## Install GShade separately on Windows

1. Exit KAMI.
2. Obtain Windows GShade from its official distributor, or use a trusted
   compatible ReShade installation. Do not stack multiple OpenGL wrappers
   (opengl32.dll) in the viewer folder.
3. Select KAMIViewer.exe as the application and OpenGL as the graphics API.
   KAMI is an OpenGL Second Life viewer, including with external DX12 modules.
4. Complete setup using the external installer's instructions. Restart KAMI,
   then configure shader paths and depth handling in the external overlay.
   No specific GShade version has been verified on every KAMI system.
5. Optionally run Install-GShade-Assets.ps1 with the viewer closed to copy
   original KAMI-FX.fx into an existing installed shaders folder:

   powershell -ExecutionPolicy Bypass -File ./Install-GShade-Assets.ps1 -ViewerDirectory . -ShaderDirectory ./gshade-shaders/Shaders

   This helper never installs an injector, overwrites existing shaders or
   presets, or modifies ReShade.ini.
6. Choose your KAMI preset from KAMI-FX-Presets in the external overlay,
   or import your own GShade-compatible ini, fx and fxh files manually.
   GShade-exclusive extensions may not function on an alternative runtime.

## Bundled assets

- KAMI-FX/Shaders/KAMI-FX.fx: 20 original effects
- KAMI-FX/FireGarden.ini: starter preset, all effects OFF
- KAMI-FX/Cinematic.ini and Clean4K.ini: example presets
- Install-GShade-Assets.ps1: optional external shader asset helper
- KAMI-GSHADE-EXTERNAL-SETUP.md: these instructions

Not bundled: GShade binaries, ReShade binaries, proprietary MXAO shaders,
in-viewer GShade switches, internal runtime hooks or proprietary FX packs.
Native SSAO/HBAO, Contact Shading, Volumetric Lighting and Render Scale
remain independent.

If an external shader overlay malfunctions, use that runtime's official
uninstall procedure for KAMIViewer.exe. Preserve the viewer's own binary,
data and user settings. Compatibility and GPU performance still require
direct in-world testing.
