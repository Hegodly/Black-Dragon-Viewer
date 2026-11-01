#!/usr/bin/env python3
"""Verify DX12-01 is a real native DirectX foundation, not an OpenGL wrapper.

These are source-level contracts only; successful GPU execution is confirmed
separately by the Windows MSVC/WARP CTest GPU pixel-readback test.
"""
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
header = (HERE / "KamiDx12Renderer.h").read_text(encoding="utf-8")
world = (HERE / "KamiDx12World.cpp").read_text(encoding="utf-8")
core = (HERE / "KamiDx12Renderer.cpp").read_text(encoding="utf-8")
smoke = (HERE / "KamiDx12Smoke.cpp").read_text(encoding="utf-8")
cmake = (HERE / "CMakeLists.txt").read_text(encoding="utf-8")


def expect(label, condition):
    if not condition:
        raise RuntimeError("Kami DX12 source contract failed: " + label)
    print("PASS", label)


expect("Windows-only native DX12 includes", all(
    name in header for name in ("<d3d12.h>", "<dxgi1_6.h>", "#if !defined(_WIN32)")
))
expect("no OpenGL API or translation calls", not re.search(
    r"(?:#\s*include\s*[<\"]GL[/\\]|\b(?:gl[A-Z]\w+|wgl[A-Z]\w+)\s*\()",
    header + core + world + smoke + cmake
))
expect("no test adapter automatically replaces hardware",
       "if (warpForTesting)" in core
       and "DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE" in core
       and "No hardware DirectX 12 device" in core)
for token in (
    "D3D12CreateDevice", "CreateCommandQueue", "CreateCommandAllocator",
    "CreateCommandList", "D3DCompile", "D3D12SerializeRootSignature",
    "CreateGraphicsPipelineState", "CreateSwapChainForHwnd", "ResizeBuffers",
    "DXGI_SWAP_EFFECT_FLIP_DISCARD", "ResourceBarrier", "DrawInstanced",
    "ExecuteCommandLists", "SetEventOnCompletion", "Present",
    "GetCopyableFootprints", "CopyTextureRegion", "readback->Map",
):
    expect("renderer uses " + token, token in core)
expect("per-backbuffer fences avoid forced global GPU stalls each frame",
       "waitForFrame(index)" in core and "frameFenceValues_[index] = value;" in core
       and "if (!flush()) return false;" not in core.split("bool Renderer::renderFrame")[1].split(
           "bool Renderer::resize")[0])
expect("offscreen GPU test validates both geometry and clear",
       "centerColored" in core and "cornerBlack" in core)
expect("CTests actually execute GPU path",
       "kami_dx12_smoke --warp --offscreen" in cmake
       and "renderOffscreenSmokeTest" in smoke)
expect("native D3D12 SDK libs, no GL libraries",
       "d3d12 dxgi d3dcompiler" in cmake
       and not re.search(r"\b(?:OpenGL::GL|opengl32|glew|glfw)\b", cmake, re.I))

expect("indexed world scene GPU pipeline exists",
       all(token in world for token in (
           "D3D12_DEPTH_WRITE_MASK_ALL", "D3D12_COMPARISON_FUNC_LESS",
           "DXGI_FORMAT_D32_FLOAT", "IASetVertexBuffers", "IASetIndexBuffer",
           "DrawIndexedInstanced", "SetGraphicsRoot32BitConstants", "CreateCommittedResource")))
expect("per-backbuffer GPU resource lifetime preserved",
       "frameUploads_[index].clear()" in world and
       "frameFenceValues_[index] = fence;" in world)
expect("world pass runs in native Windows CTest",
       "KamiDx12World.cpp" in cmake and
       "renderer.renderWorldOffscreenTest" in smoke)
expect("world GPU readback detects depth and indexed mesh",
       "redOccludesBlue" in world and "blackClear" in world)
print("Kami DX12-01+02 native indexed world pass source contracts PASS")
