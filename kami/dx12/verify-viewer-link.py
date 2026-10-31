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
check("opt-in DX12 GPU test compiled into startup", '#include "KamiDx12Renderer.h"' in startup
      and 'nativeDx12.renderOffscreenSmokeTest(128, 128)' in startup
      and 'kamiDx12WarpTest || kamiDx12HardwareTest' in startup)
check("no diagnostic GPU load on normal viewer start",
      "if (kamiDx12WarpTest || kamiDx12HardwareTest ||" in startup and
      "kamiWorldWarpTest || kamiWorldHardwareTest)" in startup and
      "if (!nativeDx12.initialize(kamiDx12WarpTest || kamiWorldWarpTest))" in startup)
check("Windows build executes embedded DX12 rasterization",
      'Execute real native D3D12 GPU test inside staged KAMIViewer' in workflow
      and '--kami-dx12-warp-test' in workflow)
check("native DX12 world depth & indexed test in staged viewer",
      "nativeDx12.renderWorldOffscreenTest(128, 128)" in startup and
      "--kami-dx12-world-warp-test" in workflow)
print("KAMI DX12 embedded build wiring: PASS. SL world still uses existing renderer.")
