# Actual MXAO: optional personal runtime integration

This adapter runs the author's actual `MartysMods_MXAO.fx` through ReShade FX.
It does not rename native SSAO/HBAO to MXAO. No proprietary shader source or
private modifications are committed here or included in public CI artifacts.

The optional ReShade runtime is built from revision
`7bf9de8b33bcc76c3177007e65d73c72dd0f34c0`, with its BSD license/notices staged.
It is loaded before GL context creation ONLY when a personal-install marker
exists. Existing external `opengl32.dll` injection is respected, and hooked DLLs
are never unloaded while GL is active. `KAMI_DISABLE_MXAO=1` bypasses the runtime
for recovery; delete `KAMI_MXAO_PRIVATE.enabled` while the viewer is closed to
return to the native-only startup path.

## Personal setup on Windows

In the next KAMI Windows build, click **Install MXAO from author (optional)**
inside Preferences > Display > Private MXAO. A separate PowerShell installer
starts only after the button is clicked. Close the viewer when prompted,
then restart after setup. The MXAO shader is not pre-bundled.

Manual command-line installation is still supported:

After a successful compatible Windows build, close the viewer and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Install-Private-MXAO.ps1 -ViewerDirectory 'C:\Path\To\KAMI'
```

Git is not required. The script downloads a pinned ZIP from the author’s official iMMERSE repo
at `f57d3afa1ebe5d1fd6152d4f6fb9a2e75bd1d1cb`, keeps its license, and selects its
existing pixel-shader fallback to reduce reliance on compute passes. The runtime’s
OpenGL version requirements still apply and must be checked on each target GPU. It does
not overwrite an existing ReShade installation/configuration. Restart, open
Preferences > Display > Private MXAO, disable native AO, and enable MXAO.

The adapter controls actual MXAO technique state, strength, sample radius and
quality through the ReShade API. Missing technique/uniforms disable processing
with diagnostics. Native AO, native frame generation and Performance Mode pause
MXAO. It starts OFF each session. No normal-startup shader preload is added;
a personal MXAO installation can compile its shader on initialization.

## Validation / limitations

Local checks passed for ReShade FX preprocessing/parsing and GLSL generation of
the official MXAO pixel fallback. Its vertex shader and all five fragment shader
entry points compiled on a headless Mesa OpenGL context. Settings/XUI binding,
layout bounds and runtime hook initialization order checks passed. These checks
do not validate the Windows API adapter or real scene depth capture.

Requires successful Windows runtime + viewer build and in-world validation.
Shader compilation alone does not prove correct depth capture. Verify the depth
buffer selection/orientation/linearization and image output with Home diagnostics.
Resize, snapshots, frame caps, UI bleed and fallback must be checked on hardware.
ReShade processes the final presentation, so screen-space effects may bleed over
UI, and ordinary viewer snapshots may not include them. Performance is unmeasured.

The author permits private modifications; distribution/reuse requires permission.
The installer is for personal direct download, not preparing redistributed MXAO
packages. Public builds contain only this generic adapter and the open runtime.
