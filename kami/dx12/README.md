# KAMI DirectX 12 renderer migration

## Target

Windows-only and DirectX 12-only rendering, with no OpenGL compatibility
wrapper or GL fallback in the **finished viewer**. Until complete feature
parity and functioning DX12 world drawing, the older OpenGL-based build must
remain usable; turning it off now would make Kami unusable.

## Implemented: DX12-01 renderer foundation

The native D3D12 C++ implementation in this directory provides:

- DXGI factory, discrete GPU preference and test-only WARP fallback
- D3D12 device, hardware graphics queue, command allocators and command list
- Double-buffered DXGI flip-model swap chain, RTV descriptors and resize
- GPU/CPU fences and synchronization that doesn't block every rendered frame
- Native HLSL vertex and pixel shader, D3D12 graphics pipeline and rasterization
- Resource barriers, clears, triangle rendering, presentation
- Offscreen GPU rendering and pixel readback verifying the actual shader output
- A separate MSVC Windows CI path, without destabilizing the present viewer

WARP is Microsoft's D3D12 software rasterizer, not OpenGL or a translation
layer. GPU acceleration and FPS still require hardware testing.

## DX12-01b: compiled into the actual Kami Windows executable

The native D3D12 renderer's C++ implementation is now also compiled into
`KAMIViewer.exe` by `indra/newview/CMakeLists.txt`, with native Windows SDK
D3D12/DXGI/D3DCompiler linkage. This avoids any separate installation script
or loose external renderer DLL.

Run these diagnostics on the finished, packaged Windows viewer:

    KAMIViewer.exe --kami-dx12-warp-test
    KAMIViewer.exe --kami-dx12-hardware-test

Exit status 0 means an actual DirectX 12 pixel shader drew an offscreen
triangle, its pixels were read back and validated, and the D3D12 resources were
released. Nonzero means the D3D12 initialization or rasterizer test failed.
The WARP option uses Windows' built-in D3D12 CPU rasterizer, while the hardware
option selects a DX12-capable discrete/integrated GPU.

These flags do not start a Second Life session. Normal viewer startup is not
slowed by probing or initializing a second graphics device. The current viewer
still uses the existing OpenGL renderer for its actual world and UI until every
rendering subsystem is migrated and verified under D3D12.

## DX12-02 prototype: actual indexed world-geometry rendering

A native D3D12 indexed mesh renderer has been added in `KamiDx12World.cpp`.
This path draws arbitrary CPU-supplied triangle meshes with per-mesh
world-view-projection matrices, per-vertex color, a D32 depth buffer, index
validation, a bounded upload memory budget and per-frame GPU-fence-protected
resource lifetimes. The native viewport/RTV can display these meshes, and
the WARP/MSVC test renders overlapping meshes, reads actual BGRA pixels and
verifies the nearest surface wins the hardware depth test.

The `KAMIViewer.exe --kami-dx12-world-warp-test` diagnostic runs this renderer
from inside the real viewer executable without booting the existing viewer.
No OpenGL translation layer is involved. This is a genuine step toward
world rendering, but it does **not** pull Second Life regions, terrain,
avatars or material textures from the live viewer. World/rigged mesh scene
extraction, UVs and textures, draw batching, texture residency, GPU default
heap caching, PBR and UI are not yet implemented. The prototype caps a frame
at 256 small meshes and 32 MiB of per-frame upload memory; do not call it
production-performance-ready.

## NOT implemented yet

The standalone smoke renderer **does not render Second Life**. The OpenGL
viewer has NOT been converted. Missing components include:

1. DX12 render interface and scene graph / material / GPU upload abstraction
2. Region, terrain, prim, mesh, rigged mesh, avatar and attachment rendering
3. Texture uploads/streaming, VRAM budgeting, culling and LOD handling
4. SL PBR ON and true PBR OFF, EEP, lighting, shadows and reflections
5. UI/HUD/2D composition and dynamic texture rendering
6. Depth/motion vectors, DLSS/FSR/XeSS, frame generation, DXR capability modes
7. Cinematic effects, injector integration, depth hooks and photo capture
8. Perf modes, FPS limiter, resolution scaling, telemetry and creator preview
9. All viewer-specific SL networking, inventory, physics and RLV interactions
10. Windows packaging as the complete, tested Kami DX12-only executable

No item above should be marked done without actual D3D12 source, Windows
compilation and in-world tests. Existing avatar physics, Follow Avatar,
Smart Sort, login, uploader and other SL features must not regress.

## Build and test on Windows 11 with Visual Studio 2022 and Windows SDK

    cmake -S kami/dx12 -B build/kami-dx12 -G "Visual Studio 17 2022" -A x64
    cmake --build build/kami-dx12 --config Release --parallel 2
    ctest --test-dir build/kami-dx12 -C Release --output-on-failure

Optionally exercise a DXGI native window and live present:

    .\build\kami-dx12\Release\kami_dx12_smoke.exe --warp --window

Hardware adapter diagnostic (requires a hardware DX12 GPU):

    .\build\kami-dx12\Release\kami_dx12_smoke.exe --hardware --offscreen

## Milestones to a true DX12-only Kami

DX12-01: native device, shaders, rasterizer, swap chain, GPU fence and CI test.
DX12-02: GPU resource model, descriptor management and typed world render API.
DX12-03: world/terrain/avatars/attachments and SL UI all draw via DX12.
DX12-04: lighting, shadows, reflection probes, EEP and PBR ON/OFF parity.
DX12-05: native post-processing, upscalers, frame generation, DXR options.
DX12-06: reproducible Windows packaging, stability and GPU FPS regression
        coverage, then **remove all OpenGL dependencies from final executable**.

A successful foundation test proves a genuine DX12 draw, not a completed viewer,
not an FPS gain, and not successful in-world behavior.
