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
5. KAMI ships its original optional shader *source* files in `KAMI-FX/`,
   with no CMD/PowerShell installer. These .fx/.ini files are **not active**
   inside the native renderer. If you independently install a supported
   GShade/ReShade runtime, use its own interface to add the KAMI-FX shader
   path and select a preset. KAMI does not install or inject that runtime.

## Optional compatibility assets

- KAMI-FX/Shaders/KAMI-FX.fx: 20 original external shader techniques
- KAMI-FX/FireGarden.ini: no effects active by default
- KAMI-FX/Cinematic.ini and Clean4K.ini: example external presets
- KAMI-FX/FX-LICENSE.txt
- KAMI-GSHADE-EXTERNAL-SETUP.md

No user-facing feature-installation batch, CMD or PowerShell scripts are
shipped. KAMI's native PBR, deferred effects, real QHD/4K/8K and Workspaces
are compiled and integrated before the portable release is packaged.

Not bundled: GShade binaries, ReShade binaries, proprietary MXAO shaders,
in-viewer GShade switches, internal runtime hooks or proprietary FX packs.
Native SSAO/HBAO, Contact Shading, Volumetric Lighting and Render Scale
remain independent.

If an external shader overlay malfunctions, use that runtime's official
uninstall procedure for KAMIViewer.exe. Preserve the viewer's own binary,
data and user settings. Compatibility and GPU performance still require
direct in-world testing.
