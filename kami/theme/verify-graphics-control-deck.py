#!/usr/bin/env python3
"""Check KAMI Graphics Control Deck source and generated viewer UI."""
import argparse
import ast
from collections import Counter
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
P = R / "kami/theme/modernize-graphics-control-deck.py"

def source_check():
    src = P.read_text(encoding="utf-8")
    ast.parse(src, filename=str(P))
    for phrase in ("kami_graphics_intro_card", "KAMIAliveEnabled",
                   "KAMIAliveStrength", "basic_expand", "advanced_expand",
                   "finetuning_expand", "original_controls",
                   "Pref.Default", "FRAME-TIME AWARE", "control_signature"):
        assert phrase in src, f"Missing graphics overhaul source: {phrase}"
    assert "glFinish(" not in src and "glReadPixels(" not in src
    print("PASS Graphics Control Deck source: native XUI, no rendering changes")

def integrated_check():
    panel = ET.parse(R / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
    stack = [x for x in panel.iter("layout_stack") if x.get("name") == "gfx_stack"]
    assert len(stack) == 1
    stack = stack[0]
    assert len([e for e in stack if e.get("name") == "kami_graphics_intro_card"]) == 1
    intro = [e for e in stack if e.get("name") == "kami_graphics_intro_card"][0]
    controls = {e.get("name"):e for e in intro if e.tag in ("check_box", "slider")}
    assert controls["kami_alive_enabled"].get("control_name") == "KAMIAliveEnabled"
    assert controls["kami_alive_strength"].get("control_name") == "KAMIAliveStrength"
    expected = {
        "title_layout_panel": ("basic_expand", "PrefsViewerVisible", "basic_layout_panel"),
        "deferred_layout": ("advanced_expand", "PrefsDeferredVisible", "advanced_layout_panel"),
        "post_layout": ("finetuning_expand", "PrefsPostVisible", "finetuning_layout_panel"),
    }
    for name, (button_name, control_name, panel_name) in expected.items():
        sections = [x for x in stack if x.get("name") == name]
        assert len(sections) == 1 and sections[0].get("height") == "35"
        buttons = [x for x in sections[0] if x.get("name") == button_name]
        assert len(buttons) == 1 and buttons[0].get("control_name") == control_name
        callback = buttons[0].find("button.commit_callback")
        assert callback is not None and callback.get("function") == "Pref.Tab"
        assert callback.get("parameter") == panel_name
    assert panel.get("label") == "KAMI Graphics"
    assert len([e for e in panel.iter("button") if e.get("name")=="default"
                and e.get("label")=="RESET"]) >= 6
    # Legacy graphic settings and native callbacks remain present, not replaced
    bindings = Counter(e.get("control_name") for e in panel.iter()
                       if e.tag in ("check_box", "slider", "combo_box"))
    for setting in ("RenderVSyncEnable", "RenderFarClip", "RenderAvatarLODFactor",
                    "RenderDisableVintageMode", "RenderShadowDetail",
                    "KAMIAliveEnabled", "KAMIAliveStrength"):
        assert bindings[setting] >= 1, f"Control lost: {setting}"
    print("PASS Graphics Control Deck integrated: Kami graphics header, original accordions/controls and ALIVE settings")

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--integrated", action="store_true")
    args = p.parse_args()
    source_check()
    if args.integrated:
        integrated_check()
    else:
        print("Source check only. Windows compile, UI scaling and in-world performance remain unverified.")
