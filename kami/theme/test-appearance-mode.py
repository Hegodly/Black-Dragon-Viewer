#!/usr/bin/env python3
"""Static post-integration validation for KAMI's native appearance mode."""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
UI = ROOT / "indra/llui"

settings = ET.parse(VIEW / "app_settings/settings.xml").getroot()
assert sum(1 for e in settings.iter("key") if e.text == "KamiLightMode") == 1
assert sum(1 for e in settings.iter("key") if e.text == "KamiGlassOpacity") == 1
menu = ET.parse(VIEW / "skins/default/xui/en/menu_viewer.xml").getroot()
workspace = ET.parse(VIEW / "skins/default/xui/en/floater_kami_workspaces.xml").getroot()
sliders = [s for s in workspace.iter("slider") if s.get("name") == "kami_glass_opacity"]
assert len(sliders) == 1 and sliders[0].get("control_name") == "KamiGlassOpacity"
items = [e for e in menu.iter("menu_item_check") if e.get("name") == "KamiLightAppearance"]
assert len(items) == 1, "missing Light/Dark appearance toggle"
item = items[0]
assert any(c.tag.endswith(".on_check") and c.get("control") == "KamiLightMode" for c in item)
assert any(c.tag.endswith(".on_click") and c.get("function") == "ToggleControl"
           and c.get("parameter") == "KamiLightMode" for c in item)
checks = {
    UI / "lluicolortable.h": ("setKAMIRuntimeColor", "clearKAMIRuntimePalette"),
    UI / "lluicolortable.cpp": ("mKAMIPaletteOriginals.try_emplace", "it->second = entry.second"),
    VIEW / "llviewercontrol.cpp": ("void kami_apply_appearance()", '"KamiLightMode"', '"KamiGlassOpacity"', "ToolTipTextColor"),
    VIEW / "llviewercontrol.h": ("void kami_apply_appearance();",),
    VIEW / "llappviewer.cpp": ("    kami_apply_appearance();",),
}
for path, needles in checks.items():
    source = path.read_text(encoding="utf-8-sig")
    for needle in needles:
        assert needle in source, f"{path}: {needle} missing"
print("PASS: KAMI palette startup, live toggle, saved Boolean, UI-only colors, XML")
