#!/usr/bin/env python3
"""Ensure the approved KAMI Beyond Reality fire-garden login is what users see.

Run *after* apply-static-theme.py and clean-login.py, before portable staging.
Do not remove the sign-in controller's required widgets or open-source credits.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
panel_path = ROOT / "indra/newview/skins/default/xui/en/panel_login.xml"
tree = ET.parse(panel_path)
root = tree.getroot()

def find_one(name):
    found = [n for n in root.iter() if n.get("name") == name]
    assert len(found) == 1, f"{name}: expected 1 widget, found {len(found)}"
    return found[0]

# The old scrollable Black Dragon changelog / marketing stack sits above the
# static wallpaper and can obscure it. It is not used by the login controller.
old_stacks = [n for n in list(root) if n.get("name") == "login_stack"]
assert len(old_stacks) == 1, "Expected a single legacy login changelog stack"
root.remove(old_stacks[0])

# Remove leftover solid-color Black Dragon background bars that cover the
# KAMI artwork. The translucent, styled login card remains in place.
legacy_bars = [n for n in list(root) if n.tag == "icon" and n.get("name") == "login_bg"]
assert len(legacy_bars) >= 2, "Legacy login background changed unexpectedly"
for n in legacy_bars:
    root.remove(n)

art = find_one("kami_fire_garden")
assert art.tag == "icon" and art.get("image_name") == "KAMI_FireGarden"
art.attrib.update(layout="topleft", left="0", top="0", width="996",
                  height="600", follows="all", mouse_opaque="false",
                  visible="true")
# Keep background as the earliest drawable, so foreground UI isn't obscured.
root.remove(art)
root.insert(3, art)  # following three static panel strings

# Give the approved brand emblem a visible place, instead of just text.
icon = ET.Element("icon", attrib={
    "name": "kami_header_emblem",
    "image_name": "KAMI_Logo",
    "layout": "topleft",
    "follows": "left|top",
    "left": "38", "top": "40",
    "width": "92", "height": "92",
    "mouse_opaque": "false",
})
title = find_one("kami_login_title")
subtitle = find_one("kami_login_subtitle")
title.set("left", "148")
subtitle.set("left", "150")
root.insert(list(root).index(title), icon)


# Optional local soundtrack: public builds contain no audio recording.
# Place the user-provided KAMI-login.mp3 next to KAMIViewer.exe.
# These are functional native settings; they do not affect world/voice volume.
music = ET.Element("panel", attrib={
    "name": "kami_login_music_controls", "layout": "topleft",
    "left": "40", "top": "160", "width": "355", "height": "95",
    "follows": "left|top", "mouse_opaque": "false",
    "background_visible": "false",
})
ET.SubElement(music, "check_box", attrib={
    "name": "kami_login_music_enabled", "layout": "topleft",
    "left": "0", "top": "0", "width": "220", "height": "20",
    "follows": "left|top", "initial_value": "true",
    "label": "Login soundtrack", "tool_tip": "Play a local KAMI-login.mp3 file beside KAMIViewer.exe",
})
ET.SubElement(music, "slider", attrib={
    "name": "kami_login_music_volume", "layout": "topleft",
    "left": "0", "top": "28", "width": "330", "height": "18",
    "follows": "left|top", "min_val": "0", "max_val": "100",
    "increment": "1", "initial_value": "35", "label": "Volume",
    "label_width": "75", "show_text": "true",
    "can_edit_text": "true", "tool_tip": "Login soundtrack volume only",
})
status = ET.SubElement(music, "text", attrib={
    "name": "kami_login_music_status", "layout": "topleft",
    "left": "0", "top": "52", "width": "350", "height": "37",
    "follows": "left|top", "wrap": "true", "font": "SansSerifSmall",
    "text_color": "0.78 0.88 0.97 1", "mouse_opaque": "false",
})
status.text = "Optional: add your own MP3 with Install-KAMI-Login-Music.cmd"
root.insert(list(root).index(title) + 1, music)

# Remove the now-inert Black Dragon changelog button, which has no associated
# content after the old promotional stack is removed.
for parent in list(root.iter()):
    for child in list(parent):
        if child.get("name") == "version_info_link":
            parent.remove(child)

# Preserve the actionable account controls and verified texture references.
for name in (
    "connect_btn", "start_location_combo", "username_combo",
    "password_edit", "remember_password", "remember_name",
    "server_combo", "quit_btn", "forgot_password_text",
    "create_new_account_text", "channel_text", "new_version",
    "panel_login2", "root_panel",
):
    find_one(name)

assert not list(root.iter("web_browser")), "Old streaming Black Dragon login browser survived"
assert not any(n.get("name") in {"login_bg", "login_stack"} for n in root.iter())
assert "K A M I" in (title.text or "")
assert "B E Y O N D" in (subtitle.text or "")
assert find_one("kami_fire_garden").get("image_name") == "KAMI_FireGarden"
assert find_one("kami_login_music_enabled").tag == "check_box"
assert find_one("kami_login_music_volume").tag == "slider"
assert find_one("kami_login_music_status").tag == "text"

ET.indent(tree, space=" ")
tree.write(panel_path, encoding="utf-8", xml_declaration=True)
ET.parse(panel_path)
print("KAMI Beyond Reality login: full-bleed fire-garden, blue/obsidian card and emblem; old Black Dragon login layers removed.")
