# KAMI Beyond Reality — Original Core

KAMI is a new, independently authored Windows application. This project does not import Black Dragon, Firestorm or official Second Life viewer source, assets, skins, shaders, or build scripts.

## Current status
Original native Windows shell, Direct2D/DirectWrite interface, user-selectable light/dark appearances, keyboard navigation, and separate protocol boundary. This is NOT YET a working Second Life viewer: authentication, world rendering, inventory, avatar and messaging are not implemented.

## World rendering policy
KAMI's 3D rendering backend is **DirectX 12 exclusively**. The new original `src/world/device.cpp` uses DXGI to select a hardware D3D12 device and creates a direct command queue. An optional DX12 WARP path is provided for diagnostics where hardware DX12 is unavailable. This is only a foundation: it does not yet create a swap chain, draw geometry, or render a Second Life scene. No OpenGL or D3D11 world renderer is included.

The existing **Direct2D/DirectWrite** rendering is strictly for the two-dimensional Windows app interface and does not replace the required DirectX 12 world renderer.

## Build
Requires Windows, Visual Studio 2022 Desktop development with C++, Windows SDK, and CMake.
Run these commands in the project root:
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure
Then start build/Release/KAMIViewer.exe.

F1-F6 changes screens; T toggles light/dark and saves the preference in the Windows user registry.

This clean source tree lives on a separate branch in the owner's existing GitHub repository. The old repository name and earlier branches remain until a new repository is created or the existing one renamed. No inherited viewer files are included in this branch's source tree. The user has not yet selected a license for their newly authored code.


## Original DX12 scene milestone
The World tab now hosts a **DirectX 12 rendering viewport**. It renders a locally generated terrain mesh and a primitive anonymous mannequin using vertex-colored materials, directional HLSL lighting, depth testing, and a movable camera. Use F2 to open World; A/D or arrow keys orbit; W/S or wheel zoom; Up/Down adjusts height. This is offline demonstration data, not an actual Second Life region or avatar. Native Windows 2D UI remains Direct2D; no OpenGL world rendering.
