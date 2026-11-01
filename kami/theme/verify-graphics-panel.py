#!/usr/bin/env python3
"""Preflight/verify KAMI graphics control deck; no GPU benchmark claim."""
import argparse
import ast
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
PATH=ROOT/"kami/theme/modernize-graphics-panel.py"
GRAPHICS=ROOT/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
src=PATH.read_text(encoding="utf-8-sig")
ast.parse(src)
for key in ("KAMIAliveEnabled", "PrefsViewerVisible", "PrefsDeferredVisible",
            "PrefsPostVisible", "before = bindings()", "after = bindings()",
            "section_names", "expanded >= 20"):
    assert key in src,key

if __name__ == "__main__":
    cli=argparse.ArgumentParser()
    cli.add_argument("--integrated",action="store_true")
    flags=cli.parse_args()
    if flags.integrated:
        root=ET.parse(GRAPHICS).getroot()
        assert root.get("label")=="KAMI | Graphics + Frame Pacing"
        named={n.get("name"):n for n in root.iter() if n.get("name")}
        for section in ("title_layout_panel","deferred_layout","post_layout"):
            assert section in named
        assert named["basic_layout_panel"].get("visibility_control")=="PrefsViewerVisible"
        assert named["advanced_layout_panel"].get("visibility_control")=="PrefsDeferredVisible"
        assert named["finetuning_layout_panel"].get("visibility_control")=="PrefsPostVisible"
        assert named["kami_alive_material_mode"].get("control_name")=="KAMIAliveEnabled"
        assert len([n for n in root.iter("check_box")
                    if n.get("name")=="kami_alive_material_mode"])==1
        subtitles=[(n.text or "").strip() for n in root.iter("text")]
        for expected in ("TEXTURE MEMORY & STREAMING",
                         "SCENE DETAIL & MATERIAL QUALITY",
                         "FRAME PACING & AVATAR COMPLEXITY",
                         "CONTACT SHADING & AO",
                         "LEGACY SHADER EFFECTS"):
            assert expected in subtitles,expected
        assert len([n for n in root.iter("check_box")
                    if n.get("left")=="10" and n.get("width")=="425"])>=20
        print("PASS integrated KAMI Graphics sections, names, bindings and ALIVE toggle")
    else:
        print("PASS KAMI Graphics UI script syntax and explicit bindings/section guards")
