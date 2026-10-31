#!/usr/bin/env python3
"""KAMI native QHD contract checks.

Default preflight verifies the pure 1440-row size policy and script syntax.
--integrated additionally validates the REAL post-patch C++ and UI contents.
Static checks do not prove OpenGL hardware behavior or live FPS.
"""
from pathlib import Path
import argparse
import py_compile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "indra/newview/app_settings/settings_blackdragon.xml"
PIPELINE = ROOT / "indra/newview/pipeline.cpp"
DISPLAY = ROOT / "indra/newview/llviewerdisplay.cpp"
PANEL = ROOT / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
SCRIPT = ROOT / "kami/renderer/enable-true-1440p.py"


def dimensions(view_width: int, view_height: int, target: int) -> tuple[int, int]:
    """Mirror the intended C++ arithmetic for a simple pure regression test."""
    assert view_width > 0 and view_height > 0
    out_height = 1440 if target == 2560 else max(
        1, int(target * view_height / view_width + 0.5)
    )
    target_width = max(
        1, int(view_width * out_height / view_height + 0.5)
    ) if target == 2560 else target
    return target_width, out_height


def field(parent, name):
    entries = list(parent)
    result = [
        entries[i + 1] for i, node in enumerate(entries[:-1])
        if node.tag == "key" and node.text == name
    ]
    assert len(result) == 1, f"{name}: expected one LLSD key"
    return result[0]


def integrated():
    root = ET.parse(SETTINGS).getroot().find("map")
    assert root is not None
    setting = field(root, "KombatantRenderWidth")
    assert field(setting, "Value").text == "2560", "QHD must be the true default"
    assert field(setting, "Persist").text == "1", "Manual choice must survive restart"
    field(root, "KAMI1440FallbackReason")

    panel = ET.parse(PANEL).getroot()
    selectors = [n for n in panel.iter() if n.get("name") == "kd_resolution"]
    assert len(selectors) == 1
    selector = selectors[0]
    assert selector.get("control_name") == "KombatantRenderWidth"
    options = {n.get("value"): n.get("label", "") for n in selector if n.tag == "combo_box.item"}
    assert set(options) == {"0", "2560", "3840", "7680"}, options
    assert "1440" in options["2560"] and "native" in options["0"].lower()
    labels = [n for n in panel.iter() if n.get("name") == "kd_resolution_note"]
    assert len(labels) == 1 and labels[0].get("control_name") == "KAMILiveResolutionStatus"

    pipeline = PIPELINE.read_text(encoding="utf-8-sig")
    display = DISPLAY.read_text(encoding="utf-8-sig")
    assert "const U32 out_height = target == 2560U ? 1440U" in pipeline
    assert "const U32 target_width = target == 2560U" in pipeline
    assert "width = target_width;" in pipeline
    assert "target_width <= limit" in pipeline
    assert "target_width <= (U32)llmax(0, viewport_limit[0])" in pipeline
    assert 'gSavedSettings.getU32("KombatantRenderWidth") == 2560U' in pipeline
    assert "if (requested == 2560 && gpu_height != 1440)" in display
    assert "TRUE QHD 2560 x 1440 world pixels (no upscale)." in display
    assert "KAMI1440FallbackReason" in display
    assert "gPipeline.mMainRT.screen.getWidth()" in display
    assert "gPipeline.mMainRT.screen.getHeight()" in display
    assert "RenderResolutionMultiplier" in pipeline  # native render scale remains available
    assert "const bool needsGlowComposite = sRenderGlow ||" in pipeline
    assert "if (needsGlowComposite)" in pipeline
    assert "RenderNumColors > 2U" in pipeline  # 1 and 2 are neutral in shader

    general = ET.parse(ROOT / "indra/newview/app_settings/settings.xml").getroot().find("map")
    assert general is not None
    cas = field(general, "RenderCASSharpness")
    assert field(cas, "Value").text == "0.0", "Native QHD base should not sharpen twice"

    # Keep the final display at window pixel dimensions, for a full scene
    # rather than a cropped QHD viewport on smaller monitors.
    assert "gViewerWindow->getWorldViewRectRaw().getWidth()" in pipeline
    assert "gViewerWindow->getWorldViewRectRaw().getHeight()" in pipeline
    print("PASS post-integration C++, LLSD and UI true-QHD contracts")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--integrated", action="store_true")
    args = parser.parse_args()

    py_compile.compile(str(SCRIPT), doraise=True)
    assert dimensions(1920, 1080, 2560) == (2560, 1440)
    assert dimensions(1280, 720, 2560) == (2560, 1440)
    assert dimensions(3440, 1440, 2560) == (3440, 1440)
    assert dimensions(1920, 1200, 2560) == (2304, 1440)
    assert dimensions(1920, 1080, 3840) == (3840, 2160)
    assert dimensions(1920, 1080, 7680) == (7680, 4320)
    print("PASS native QHD framebuffer dimensions, aspect and 4K/8K compatibility")
    if args.integrated:
        integrated()
    print("KAMI QHD static verification passed. Real GPU/viewport QA still required.")


if __name__ == "__main__":
    main()
