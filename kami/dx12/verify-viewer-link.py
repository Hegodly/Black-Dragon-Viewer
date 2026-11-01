#!/usr/bin/env python3
"""Enforce native D3D12 compiled into the Windows KAMIViewer executable.

This checks build wiring and opt-in runtime test availability, not the viewer's
3D rendering API. The legacy OpenGL renderer still draws Second Life.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
cmake = (root / "indra/newview/CMakeLists.txt").read_text(encoding="utf-8-sig")
startup = (root / "indra/newview/llappviewerwin32.cpp").read_text(encoding="utf-8-sig")
workflow = (root / ".github/workflows/kombatant-windows.yaml").read_text(encoding="utf-8-sig")

def check(name, ok):
    if not ok:
        raise AssertionError("KAMI DX12 build integration: " + name)
    print("PASS KAMI DX12: " + name)

check("compiled in same viewer target", 'target_sources(${VIEWER_BINARY_NAME} PRIVATE' in cmake
      and '${KAMI_DX12_DIR}/KamiDx12Renderer.cpp' in cmake)
check("native Windows D3D12 linked", 'target_link_libraries(${VIEWER_BINARY_NAME} d3d12 dxgi d3dcompiler)' in cmake)
check("native indexed world pass linked into viewer",
      "${KAMI_DX12_DIR}/KamiDx12World.cpp" in cmake)
check("real Second Life scene bridge compiled in viewer",
      "${CMAKE_CURRENT_SOURCE_DIR}/kami_dx12_scene_bridge.cpp" in cmake and
      "${CMAKE_CURRENT_SOURCE_DIR}/kami_dx12_scene_bridge.h" in cmake)
renderer = (root / "kami/dx12/KamiDx12Renderer.cpp").read_text(encoding="utf-8-sig")
runner = (root / "kami/dx12/KamiDx12Runner.h").read_text(encoding="utf-8-sig")
check("opt-in DX12 GPU test compiled into startup", '#include "KamiDx12Runner.h"' in startup
      and 'runEmbeddedSmokeTest(' in startup
      and "int runEmbeddedSmokeTest(bool warpForTesting, bool testWorldDepth) noexcept" in runner
      and "renderer.renderOffscreenSmokeTest(128, 128)" in renderer
      and 'kamiDx12WarpTest || kamiDx12HardwareTest' in startup)
check("no diagnostic GPU load on normal viewer start",
      "if (kamiDx12WarpTest || kamiDx12HardwareTest ||" in startup and
      "kamiWorldWarpTest || kamiWorldHardwareTest)" in startup
      and "const int diagnosticExitCode = kami::dx12::runEmbeddedSmokeTest(" in startup
      and "::ExitProcess(static_cast<UINT>(diagnosticExitCode));" in startup)
check("diagnostic success and failure bypass uninitialized viewer CRT teardown",
      "::ExitProcess(static_cast<UINT>(diagnosticExitCode));" in startup
      and "return kami::dx12::runEmbeddedSmokeTest(" not in startup
      and "return 61;" in renderer and "return 62;" in renderer
      and "return 63;" in renderer)
check("renderer construction stays in native translation unit",
      "Renderer renderer;" in renderer
      and "Renderer nativeDx12;" not in startup
      and "renderer.initialize(warpForTesting)" in renderer)
check("Windows build executes embedded DX12 rasterization",
      'Execute real native D3D12 GPU test inside staged KAMIViewer' in workflow
      and '--kami-dx12-warp-test' in workflow)
check("native DX12 world depth & indexed test in staged viewer",
      "renderer.renderWorldOffscreenTest(128, 128)" in renderer
      and "testWorldDepth &&" in renderer
      and "--kami-dx12-world-warp-test" in workflow)
print("KAMI DX12 embedded build wiring: PASS. SL world still uses existing renderer.")
