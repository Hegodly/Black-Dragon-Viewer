#!/usr/bin/env python3
"""Validate a real SL scene-to-D3D12 presentation path in the Windows viewer.

This is an opt-in partial-scene preview, NOT the production SL renderer.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "indra/newview/kami_dx12_live_preview.cpp").read_text("utf-8-sig")
header = (root / "indra/newview/kami_dx12_live_preview.h").read_text("utf-8-sig")
display = (root / "indra/newview/llviewerdisplay.cpp").read_text("utf-8-sig")
cmake = (root / "indra/newview/CMakeLists.txt").read_text("utf-8-sig")
settings = (root / "indra/newview/app_settings/settings.xml").read_text("utf-8-sig")
world_menu = (root / "indra/newview/skins/default/xui/en/menu_viewer.xml").read_text("utf-8-sig")

def check(label, condition):
    if not condition:
        raise AssertionError(f"KAMI live DX12 preview: {label}")
    print("PASS", label)

check("native D3D12 DXGI swap chain feeds from live SL scene",
      "captureLiveVolumeWorld(camera, &stats)" in source
      and "mRenderer.renderWorldFrame(scene, false)" in source
      and "mRenderer.attachWindow(mWindow, mWidth, mHeight)" in source)
check("no GL translation, no swap chain on legacy viewer HWND",
      all(token not in source for token in (
          "wgl", "glBind", "glDraw", "glClear", "glFlush",
          "gGL.", "getNativeWindowHandle"))
      and "CreateWindowExW(" in source)
check("hardware DX12, not software render in normal viewer",
      "mRenderer.initialize(false)" in source)
check("default off with explicit control", 
      "<key>KamiDX12LivePreview</key>" in settings
      and "<integer>0</integer>" in settings[
          settings.index("<key>KamiDX12LivePreview</key>"):
          settings.index("<key>ImporterDebugVerboseLogging</key>")
      ])
check("preview frame budget protects other Kami optimizations",
      "std::chrono::milliseconds(100)" in source
      and "if (IsIconic(mWindow)) return;" in source)
check("preview shuts down after window close/disable/device error",
      "sPreviewCloseRequested" in source
      and 'gSavedSettings.setBOOL("KamiDX12LivePreview", false)' in source
      and "mRenderer.shutdown()" in source)
check("render call only after real viewer display starts",
      "kamiDX12LivePreviewTick(*viewer_cam)" in display
      and "#ifdef KAMI_HAS_NATIVE_DX12" in display
      and "if (!for_snapshot && viewer_cam)" in display)
check("World menu allows manual preview ON/OFF",
      'label="Native DX12 Scene Preview (Experimental)"' in world_menu
      and 'control="KamiDX12LivePreview"' in world_menu
      and 'function="ToggleControl"' in world_menu
      and 'parameter="KamiDX12LivePreview"' in world_menu)
check("compiled into actual Windows viewer not a batch installer",
      "kami_dx12_live_preview.cpp" in cmake
      and "kami_dx12_live_preview.h" in cmake)
renderer = (root / "kami/dx12/KamiDx12World.cpp").read_text("utf-8")
bridge = (root / "indra/newview/kami_dx12_scene_bridge.cpp").read_text("utf-8")
check("busy GPU skips capture before rebuilding scene",
      source.index("worldFrameAvailable()") < source.index("captureLiveVolumeWorld(camera, &stats)")
      and "if (!worldFrameAvailable()) return true;" in renderer)
check("nonblocking presentation still fences submitted buffer lifetime",
      "DXGI_PRESENT_DO_NOT_WAIT" in renderer
      and "DXGI_ERROR_WAS_STILL_DRAWING" in renderer
      and renderer.index("frameFenceValues_[index] = fence;") < renderer.index("if (FAILED(presentResult)"))
check("reuse completed upload buffers instead of per-mesh allocation churn",
      "cached->GetDesc().Width != bytes" in renderer
      and "result = cached;" in renderer
      and "frameUploads_[index].clear()" not in renderer)
check("capture time checked during object vertex and index traversal",
      "std::chrono::milliseconds(2)" in bridge
      and "(vertex & 255)" in bridge and "(index & 255)" in bridge)
check("slow update cools down from completion",
      "mLastFrame = std::chrono::steady_clock::now();" in source)
print("KAMI native DX12 real-scene preview source validation PASS")
