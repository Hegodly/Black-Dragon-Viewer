#!/usr/bin/env python3
"""Verify KAMI local GPU detection policy and post-patch source wiring."""
import argparse
import py_compile
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
MODELS = ("rtx 5070", "rtx 5080", "rtx 5090", "rtx 4070", "rtx 4080",
          "rtx 4090", "rtx 3070", "rtx 3080", "rtx 3090", "rx 6800",
          "rx 6900", "rx 6950", "rx 7700", "rx 7800", "rx 7900", "rx 9070")

def recommend(gpu):
    return 2560 if any(s in gpu.lower() for s in MODELS) else 0

def select(checked, untouched, old_gpu, gpu, old_auto, selected):
    eligible = (not checked and untouched) or (
        checked and old_gpu != gpu and selected == old_auto)
    return recommend(gpu) if eligible else selected

def get_field(parent, name):
    children = list(parent)
    matches = [children[i + 1] for i, k in enumerate(children[:-1])
               if k.tag == "key" and k.text == name]
    assert len(matches) == 1, name
    return matches[0]

def integrated():
    s = ET.parse(R / "indra/newview/app_settings/settings_blackdragon.xml").getroot().find("map")
    assert get_field(get_field(s, "KombatantRenderWidth"), "Value").text == "2560"
    for name in ("KAMIHardwareResolutionInitialized", "KAMIHardwareLastRenderer",
                 "KAMIHardwareAutoLastWidth", "KAMIHardwareDetectionStatus"):
        get_field(s, name)
    p = (R / "indra/newview/pipeline.cpp").read_text(encoding="utf-8")
    assert "glGetString(GL_RENDERER)" in p
    assert "render_control->isDefault()" in p
    assert "const bool first_auto = !checked && fresh_default;" in p
    assert "const bool gpu_changed_auto = checked && previous != renderer && selected == previous_auto;" in p
    pos = p.index("void LLPipeline::createGLBuffers()")
    assert pos < p.index("kamiDetectWorldRendererOnce();", pos) < p.index("allocateScreenBuffer(resX, resY);", pos)
    assert "KAMIHardwareDetectionStatus" in (R / "indra/newview/llviewerdisplay.cpp").read_text()
    ui = ET.parse(R / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
    box = [n for n in ui.iter() if n.get("name") == "kd_resolution"]
    assert len(box) == 1
    assert {n.get("value") for n in box[0] if n.tag == "combo_box.item"} == {"0","2560","3840","7680"}
    print("PASS post-integrated settings, C++ and UI contracts")

def main():
    a = argparse.ArgumentParser()
    a.add_argument("--integrated", action="store_true")
    args = a.parse_args()
    py_compile.compile(str(R / "kami/renderer/add-gpu-detection.py"), doraise=True)
    high = "NVIDIA GeForce RTX 5070/PCIe/SSE2"
    low = "Intel UHD Graphics 770"
    assert recommend(high) == 2560
    assert recommend("AMD Radeon RX 7800 XT") == 2560
    assert recommend(low) == 0
    assert recommend("AMD Radeon Graphics") == 0
    assert recommend("NVIDIA GeForce RTX 3050") == 0
    assert recommend("Unknown GPU") == 0
    assert select(False, True, "", high, 2560, 2560) == 2560
    assert select(False, True, "", low, 2560, 2560) == 0
    assert select(False, False, "", low, 2560, 3840) == 3840
    assert select(False, False, "", low, 2560, 2560) == 2560
    assert select(True, False, high, low, 2560, 2560) == 0
    assert select(True, False, low, high, 0, 0) == 2560
    assert select(True, False, high, low, 2560, 0) == 0
    assert select(True, False, low, high, 0, 7680) == 7680
    assert select(True, False, high, high, 2560, 0) == 0
    print("PASS GPU tier selection and manual-setting protection")
    if args.integrated:
        integrated()
    print("Static tests only; Windows rendering tests still required.")

if __name__ == "__main__":
    main()
