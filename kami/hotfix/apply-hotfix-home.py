#!/usr/bin/env python3
"""Add a native KAMI Hotfix Home preview without enabling remote self-updates.

Run AFTER apply-nextgen-workspaces.py and its UI verification; this is a
separate source-integrated milestone, not a fake working hotfix installer.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"
HERE = ROOT / "kami/hotfix"


def replace_once(path: Path, before: str, after: str) -> None:
    old = path.read_text(encoding="utf-8-sig")
    if old.count(before) != 1:
        raise RuntimeError(f"Unsafe integration anchor in {path}: {before!r}")
    path.write_text(old.replace(before, after, 1), encoding="utf-8")


header = VIEW / "kami_hotfix_center.h"
if header.exists():
    raise RuntimeError("KAMI Hotfix Center already integrated")
shutil.copyfile(HERE / "kami_hotfix_center.h", header)

reg = VIEW / "llviewerfloaterreg.cpp"
replace_once(reg, '#include "kami_workspace_hub.h"\n',
             '#include "kami_workspace_hub.h"\n#include "kami_hotfix_center.h"\n')
replace_once(
    reg,
    '    LLFloaterReg::add("kami_workspaces", "floater_kami_workspaces.xml",',
    '    LLFloaterReg::add("kami_hotfix_home", "floater_kami_hotfix_home.xml",\n'
    '                     (LLFloaterBuildFunc)&LLFloaterReg::build<KAMIHotfixCenter>);\n'
    '    LLFloaterReg::add("kami_workspaces", "floater_kami_workspaces.xml",'
)

# A visibly different native Fire Garden update panel, not a screenshot.
root = ET.Element("floater", {
    "name": "kami_hotfix_home", "title": "KAMI  |  HOTFIX CENTER",
    "layout": "topleft", "positioning": "centered",
    "width": "620", "height": "342", "min_width": "620", "min_height": "342",
    "can_close": "true", "can_resize": "false", "save_rect": "true",
    "background_visible": "true",
    "bg_opaque_color": "FloaterFocusBackgroundColor",
    "bg_alpha_color": "FloaterDefaultBackgroundColor",
})
def label(name: str, value: str, x: int, y: int, w: int, h: int, font="SansSerif"):
    el = ET.SubElement(root, "text", {
        "name": name, "layout": "topleft", "left": str(x), "top": str(y),
        "width": str(w), "height": str(h), "font": font,
        "text_color": "0.92 0.94 0.96 1",
    })
    el.text = value

ET.SubElement(root, "icon", {
    "name": "kami_hotfix_logo", "image_name": "KAMI_Logo",
    "layout": "topleft", "left": "28", "top": "36",
    "width": "64", "height": "64",
})
label("kami_hotfix_brand", "K A M I", 111, 38, 445, 32, "SansSerifBoldLarge")
label("kami_hotfix_title", "LIVE OPTIMIZATION  /  HOTFIX CENTER", 112, 80, 470, 24)
ET.SubElement(root, "view_border", {
    "name": "kami_hotfix_rule", "layout": "topleft",
    "left": "28", "top": "119", "width": "564", "height": "1",
    "bevel_style": "in",
})
label("kami_hotfix_state", "HOTFIX SERVICE: NOT YET ENABLED", 30, 141, 548, 26,
      "SansSerifBold")
label("kami_hotfix_info",
      "Full-release Kami Home will keep your Second Life session connected.",
      30, 182, 555, 28)
label("kami_hotfix_info2",
      "Optimization hotfixes: no restart. Feature updates may require one.",
      30, 210, 555, 26)
label("kami_hotfix_beta",
      "Preview only. No forced logout, patch download, or installation.",
      30, 245, 555, 27)
ET.SubElement(root, "button", {
    "name": "kami_hotfix_close", "label": "CLOSE", "layout": "topleft",
    "left": "434", "top": "283", "width": "154", "height": "36",
    "image_unselected": "PushButton_Off",
    "image_selected": "PushButton_Selected",
})
ET.indent(root, space=" ")
p = XUI / "floater_kami_hotfix_home.xml"
ET.ElementTree(root).write(p, encoding="utf-8", xml_declaration=True)
ET.parse(p)

# Retain all original Workspaces actions and only append a new native menu item.
menu = XUI / "menu_viewer.xml"
tree = ET.parse(menu)
m = [v for v in tree.getroot() if v.tag == "menu" and v.get("name") == "KAMI_Workspaces"]
if len(m) != 1 or any(e.get("name") == "Kami.HotfixHome" for e in tree.iter()):
    raise RuntimeError("Missing KAMI menu or duplicate hotfix action")
item = ET.SubElement(m[0], "menu_item_call", {
    "name": "Kami.HotfixHome", "label": "KAMI HOTFIX CENTER (PREVIEW)",
    "layout": "topleft",
})
ET.SubElement(item, "menu_item_call.on_click", {
    "function": "Floater.Show", "parameter": "kami_hotfix_home",
})
ET.indent(tree, space=" ")
tree.write(menu, encoding="utf-8", xml_declaration=True)
ET.parse(menu)
print("PASS KAMI native Hotfix Home preview installed; no live update service enabled.")
