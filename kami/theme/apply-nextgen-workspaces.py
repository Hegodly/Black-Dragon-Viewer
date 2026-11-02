#!/usr/bin/env python3
"""Reorganize KAMI's native UI around Fire Garden Workspaces, not Black Dragon widgets.

Install after all existing XUI and native-floater integrations.
The original menu actions, floater registrations, graphics controls, and
keyboard shortcuts remain intact. No 3D GPU pass, fullscreen effects or
new background processing is introduced.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"
THEME = ROOT / "kami/theme"

def read(path):
    return path.read_text(encoding="utf-8-sig")

def replace_once(path, needle, replacement):
    src = read(path)
    if src.count(needle) != 1:
        raise RuntimeError(f"Expected unique anchor in {path}: {needle[:115]!r} (found {src.count(needle)})")
    path.write_text(src.replace(needle, replacement, 1), encoding="utf-8")


# A real, registered C++ LLFloater, not a static picture or web UI.
header_dest = VIEW / "kami_workspace_hub.h"
if header_dest.exists():
    raise RuntimeError("Workspaces has already been integrated")
shutil.copyfile(THEME / "kami_workspace_hub.h", header_dest)
reg = VIEW / "llviewerfloaterreg.cpp"
replace_once(reg, '#include "llfloaterperformance.h"\n',
'''#include "llfloaterperformance.h"
#include "kami_workspace_hub.h"
''')
replace_once(reg,
    '    LLFloaterReg::add("preferences", "floater_preferences.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPreference>);',
    '''    LLFloaterReg::add("preferences", "floater_preferences.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPreference>);
    LLFloaterReg::add("kami_workspaces", "floater_kami_workspaces.xml",
                     (LLFloaterBuildFunc)&LLFloaterReg::build<KAMIWorkspaceHub>);'''
)

# The new launcher is a meaningful alternative information architecture.
# Every button routes to an already registered native feature. All labels,
# sizes and hit targets are defined in XML for localization and adjustments.
floater = ET.Element("floater", {
    "name": "kami_workspaces",
    "title": "KAMI  |  BEYOND REALITY  |  WORKSPACES",
    "layout": "topleft",
    "positioning": "centered",
    "width": "786", "height": "602",
    "min_width": "786", "min_height": "602",
    "can_close": "true", "can_resize": "false",
    "save_rect": "true", "reuse_instance": "true",
    "background_visible": "true",
    "bg_opaque_color": "FloaterFocusBackgroundColor",
    "bg_alpha_color": "FloaterDefaultBackgroundColor",
})

def text(name, value, x, y, w, h, *, font="SansSerif", color="0.93 0.95 0.97 1"):
    e = ET.SubElement(floater, "text", {
        "name": name, "layout": "topleft", "left": str(x), "top": str(y),
        "width": str(w), "height": str(h), "font": font,
        "text_color": color, "mouse_opaque": "false",
    })
    e.text = value
    return e

ET.SubElement(floater, "icon", {
    "name": "kami_workspaces_logo", "image_name": "KAMI_Logo",
    "layout": "topleft", "left": "24", "top": "39",
    "width": "76", "height": "76", "mouse_opaque": "false"
})
text("kami_workspaces_name", "K A M I", 112, 42, 490, 34,
     font="SansSerifBoldLarge")
text("kami_workspaces_tagline", "BEYOND REALITY  /  WORKSPACE NAVIGATOR", 114, 78, 580, 24,
     color="0.65 0.78 0.85 1")
text("kami_workspaces_intro",
     "Native tools. Original Second Life actions. A new way to work.",
     26, 130, 730, 27, color="0.78 0.83 0.88 1")

# Four workspaces and a 3x4 grid. Sections are arranged as two columns
# of 2-button rows for discoverability and predictable keyboard navigation.
groups = [
    ("EXPLORE", 24, 182, [
        ("hub_inventory", "INVENTORY", "Inventory window"),
        ("hub_places", "PLACES", "Saved and recent locations"),
        ("hub_map", "WORLD MAP", "World map"),
        ("hub_world", "WORLD MENU", "World actions, Area Search and experimental DX12 preview"),
    ]),
    ("AVATAR", 408, 182, [
        ("hub_outfits", "OUTFITS", "Appearance and clothing"),
        ("hub_poser", "POSER", "Native Black Dragon pose tool"),
        ("hub_pose_creator", "POSE CREATOR", "Native animation creator"),
    ]),
    ("CREATE", 24, 358, [
        ("hub_material", "MATERIALS", "PBR material editor"),
        ("hub_snapshot", "SNAPSHOT", "Photo capture and screenshots"),
        ("hub_camera", "CAMERA", "Camera controls"),
    ]),
    ("CONTROL", 408, 358, [
        ("hub_preferences", "GRAPHICS / SETTINGS", "KAMI preferences"),
        ("hub_hardware", "PERFORMANCE", "Live GPU / CPU / RAM monitor"),
        ("hub_toolbar", "DOCK EDITOR", "Customize the native toolbar"),
    ]),
]

for title, left, top, buttons in groups:
    text(f"kami_group_{title.lower()}", title, left, top, 350, 26,
         font="SansSerifBold", color="0.84 0.88 0.92 1")
    border = ET.SubElement(floater, "view_border", {
        "name": f"kami_group_{title.lower()}_rule",
        "layout": "topleft", "left": str(left), "top": str(top+29),
        "width": "347", "height": "1", "bevel_style": "in",
        "follows": "top|left", "mouse_opaque": "false",
    })
    for idx, (key,label,tip) in enumerate(buttons):
        x = left + (idx % 2) * 177
        y = top + 42 + (idx // 2) * 53
        ET.SubElement(floater, "button", {
            "name": key, "label": label, "layout": "topleft",
            "left": str(x), "top": str(y), "width": "168",
            "height": "39", "font": "SansSerifSmall",
            "tool_tip": tip, "follows": "top|left",
            "image_unselected": "PushButton_Off",
            "image_selected": "PushButton_Selected",
            "image_hover_unselected": "Toolbar_Middle_Over",
        })

text("kami_workspaces_footer",
     "BEYOND REALITY  |  Built on Black Dragon by NiranV Dean",
     26, 558, 700, 23, font="SansSerifSmall",
     color="0.59 0.67 0.72 1")
workspace_file = XUI / "floater_kami_workspaces.xml"
ET.indent(floater, space=" ")
ET.ElementTree(floater).write(workspace_file, encoding="utf-8", xml_declaration=True)
ET.parse(workspace_file)

# Keep all Black Dragon menu commands in place; add *one native KAMI launcher*
# before those menus. Renaming the existing menu's internal "Me" ID would
# break listener wiring, so change only its visible label.
menu_file = XUI / "menu_viewer.xml"
tree = ET.parse(menu_file)
root = tree.getroot()
assert root.tag == "menu_bar"
assert not any(e.get("name")=="KAMI_Workspaces" for e in root.iter())
original_items = list(root)
original_actions = [
    (e.tag, e.get("name"), e.get("shortcut"), e.get("label"))
    for e in root.iter() if e.tag.startswith("menu_item_")
]
me = [x for x in root if x.tag=="menu" and x.get("name")=="Me"]
assert len(me)==1
me[0].set("label", "KAMI")

new = ET.Element("menu", {
    "label": "KAMI  |  WORKSPACES", "name": "KAMI_Workspaces",
    "create_jump_keys": "true", "tear_off": "false",
})
for label,key,parameter in [
    ("OPEN WORKSPACE NAVIGATOR", "Kami.OpenWorkspaces", "kami_workspaces"),
    ("MY INVENTORY", "Kami.Inventory", "inventory"),
    ("MY APPEARANCE", "Kami.Appearance", "appearance"),
    ("CREATOR POSER", "Kami.Poser", "poser"),
    ("MATERIAL EDITOR", "Kami.Materials", "material_editor"),
    ("CAMERA & SNAPSHOT", "Kami.Snapshot", "snapshot"),
    ("HARDWARE PERFORMANCE", "Kami.Hardware", "kami_hardware_monitor"),
    ("PREFERENCES", "Kami.Preferences", "preferences"),
]:
    item = ET.SubElement(new, "menu_item_call", {
        "name": key, "label": label, "layout": "topleft",
    })
    ET.SubElement(item, "menu_item_call.on_click", {
        "function": "Floater.Show", "parameter": parameter,
    })
# Startup resolves MarketplaceListings relative to the original Me menu.
# Keep every original submenu under its original parent; moving controls
# preserves XML attributes but breaks native parent-relative lookups.
marketplace = list(me[0].iter())
assert any(e.get("name") == "MarketplaceListings" for e in marketplace), \
    "MarketplaceListings must remain reachable from the Me menu"
assert any(e.get("name") == "World" for e in marketplace), \
    "The original World menu must remain under Me"

root.insert(0, new)
root.set("bg_visible", "true")
menu_after = [
    (e.tag, e.get("name"), e.get("shortcut"), e.get("label"))
    for e in root.iter() if e.tag.startswith("menu_item_")
]
for item in original_actions:
    assert item in menu_after, ("A legacy menu action was removed",item)
assert all(node in list(root) for node in original_items)
ET.indent(tree, space=" ")
tree.write(menu_file, encoding="utf-8", xml_declaration=True)
ET.parse(menu_file)

# Rework the dock silhouette and spacing, not just bitmap colors.
# The existing toolbar objects remain the same controllers; residents can
# drag and rearrange their existing buttons as they always could.
dock = XUI / "panel_toolbar_view.xml"
tree = ET.parse(dock)
r = tree.getroot()
panels = {e.get("name"):e for e in r.iter("layout_panel")}
bars = {e.get("name"):e for e in r.iter("toolbar")}
for name in ("toolbar_top", "toolbar_bottom", "toolbar_left", "toolbar_right"):
    assert name in bars
for name in ("top_toolbar_panel", "bottom_toolbar_panel",
             "left_toolbar_panel", "right_toolbar_panel"):
    assert name in panels

# Distinct compact game-style command rail with a more substantial lower dock.
for panel_name, toolbar_name, value in (
    ("top_toolbar_panel", "toolbar_top", "40"),
    ("bottom_toolbar_panel", "toolbar_bottom", "54"),
):
    panels[panel_name].set("height", value)
    bars[toolbar_name].set("height", value)
    bars[toolbar_name].set("left", "0")
    bars[toolbar_name].set("width", panels[panel_name].get("width"))
bars["toolbar_top"].set("button_display_mode", "icons_small")
bars["toolbar_bottom"].set("button_display_mode", "icons_with_text")
for side in ("left", "right"):
    pn,tn=f"{side}_toolbar_panel",f"toolbar_{side}"
    panels[pn].set("width","42")
    bars[tn].set("width","42")
    bars[tn].set("button_display_mode", "icons_only")
ET.indent(tree,space=" ")
tree.write(dock,encoding="utf-8",xml_declaration=True)
ET.parse(dock)

# Preferences remains native and fully searchable, but navigation uses
# KAMI group labels. Names, callbacks and panel file paths are immutable.
prefs = XUI / "floater_preferences.xml"
tree = ET.parse(prefs)
r = tree.getroot()
r.set("title","KAMI  |  CONTROL DECK  |  BEYOND REALITY")
tabs = [e for e in r.iter("tab_container") if e.get("name")=="pref core"]
assert len(tabs)==1
tabs[0].set("tab_width","222")
tabs[0].set("tab_padding_right","14")
prior = int(r.get("width"))
previous = int(tabs[0].get("width"))
expected_tabs = {
    "general": "01  |  GENERAL",
    "display": "02  |  GRAPHICS + FPS",
    "render_settings": "03  |  AVATAR CONTROL",
    "audio": "04  |  SOUND + MEDIA",
    "chat": "05  |  SOCIAL",
    "camera": "06  |  CAMERA",
    "controls": "07  |  CONTROLS",
    "keybinds": "08  |  INPUT BINDINGS",
    "msgs": "09  |  NOTIFICATIONS",
    "privacy": "10  |  PRIVACY",
    "colors": "11  |  THEME COLORS",
    "interface": "12  |  INTERFACE",
    "viewer": "13  |  VIEWER",
    "browser": "14  |  WEB",
    "uploads": "15  |  UPLOADS",
}
found = {}
for el in tabs[0]:
    key=el.get("name")
    if key in expected_tabs:
        found[key]=el
assert set(found)==set(expected_tabs),(set(found),set(expected_tabs))
for key,el in found.items():
    el.set("label",expected_tabs[key])
# Move the content border to the right by the extra sidebar width, and
# expand the outer container with it. The original panel width is unchanged.
# If an earlier layout stage already enlarged the sidebar to 196px,
# we only add the delta from there, not the original 140px.
old_tab_width=196
delta=222-old_tab_width
assert prior>=690 and previous>=650
r.set("width",str(prior+delta))
r.set("min_width",str(prior+delta))
tabs[0].set("width",str(previous+delta))
backs = [e for e in r.iter("icon") if e.get("name")=="vicon"]
assert len(backs)==1
backs[0].set("left",str(int(backs[0].get("left"))+delta))
searches=[e for e in r.iter("filter_editor") if e.get("name")=="search_prefs_edit"]
assert len(searches)==1
searches[0].set("label","Search KAMI settings")
ET.indent(tree,space=" ")
tree.write(prefs,encoding="utf-8",xml_declaration=True)
ET.parse(prefs)

# Shared visual hierarchy; strong focus and responsive-feeling buttons
# without timing hooks or GPU work in a world render frame.
button = XUI / "widgets/button.xml"
tree = ET.parse(button)
b = tree.getroot()
assert b.tag == "button"
b.set("hover_glow_amount","0.10")
b.set("height","28")
b.set("pad_left","10")
b.set("pad_bottom","3")
b.set("imgoverlay_label_space","9")
tree.write(button,encoding="utf-8",xml_declaration=True)

floating = XUI / "widgets/floater.xml"
tree = ET.parse(floating)
f = tree.getroot()
assert f.tag == "floater"
f.set("header_height","32")
tree.write(floating,encoding="utf-8",xml_declaration=True)

print("KAMI next-gen theme installed: native functional Workspaces, "
      "distinct 4-rail dock, KAMI control deck and preserving legacy actions.")
