#!/usr/bin/env python3
"""Protect Back / Home / Forward / Location from the KAMI header launcher.

This is a source-level XUI geometry regression check. Real in-world display
scaling and restored toolbar layouts still require a visual check.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NAV = ROOT / "indra/newview/skins/default/xui/en/panel_navigation_bar.xml"
root = ET.parse(NAV).getroot()
assert root.tag == "panel" and root.get("name") == "navigation_bar"
panels = [p for p in root.iter("layout_panel") if p.get("name") == "navigationbar_layout_panel"]
assert len(panels) == 1, "The real history navigation panel has changed"
panel = panels[0]

def control(name):
    matching = [v for v in panel.iter() if v.get("name") == name]
    assert len(matching) == 1, f"Navigation control missing or duplicated: {name}"
    return matching[0]

back = control("back_btn")
home = control("home_btn")
forward = control("forward_btn")
location = control("location_combo")
assert back.tag == "pull_button" and forward.tag == "pull_button"
assert home.tag == "button" and location.tag == "location_input"
assert home.get("left_pad") == "0" and forward.get("left_pad") == "0"
back_left, icon_width = int(back.get("left")), int(back.get("width"))
location_left = int(location.get("left"))
location_right = location_left + int(location.get("width"))
panel_width = int(panel.get("width"))
assert back_left >= 200, "KAMI launcher overlaps Back/Home/Forward"
assert location_left >= back_left + 3 * icon_width + 6, "Location overlaps navigation controls"
assert location_right + 8 <= panel_width, "Location field is clipped by its panel"
assert panel.get("auto_resize") == "true", "Navigation must remain resizable"

stacks = [v for v in root.iter("layout_stack") if v.get("name") == "nvpb_stack"]
assert len(stacks) == 1 and int(stacks[0].get("width")) >= panel_width
print("PASS: KAMI header clearance preserves Back, Home, Forward and Location.")
