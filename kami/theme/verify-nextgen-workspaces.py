#!/usr/bin/env python3
"""Fail build when KAMI theme becomes only a recolor or loses native actions.

Preflight checks the source integration script and real workspace header.
--integrated verifies generated XUI and the actual Windows viewer registry.
"""
import argparse
import ast
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
VIEW=ROOT/"indra/newview"
XUI=VIEW/"skins/default/xui/en"
THEME=ROOT/"kami/theme"
SCRIPT=THEME/"apply-nextgen-workspaces.py"
HEADER=THEME/"kami_workspace_hub.h"

ACTION_TARGETS = {
    "hub_inventory": "inventory",
    "hub_outfits": "appearance",
    "hub_places": "places",
    "hub_map": "world_map",
    "hub_poser": "poser",
    "hub_pose_creator": "poser_creator",
    "hub_material": "material_editor",
    "hub_snapshot": "snapshot",
    "hub_camera": "camera",
    "hub_preferences": "preferences",
    "hub_hardware": "kami_hardware_monitor",
    "hub_toolbar": "toybox",
}

def read(p):
    return p.read_text(encoding="utf-8-sig")

def assert_native():
    source=read(SCRIPT)
    ast.parse(source,filename=str(SCRIPT))
    header=read(HEADER)
    assert "class KAMIWorkspaceHub final : public LLFloater" in header
    assert "LLFloaterReg::showInstance(nativeFloater, LLSD(), true)" in header
    assert "setClickedCallback" in header
    assert "KAMIWorkspaceHub" in source
    assert "LLFloaterReg::add(" in source
    assert "LLFloaterReg::build<KAMIWorkspaceHub>" in source
    assert "Floater.Show" in source
    assert "kami_workspaces" in source
    assert "KAMI  |  WORKSPACES" in source
    assert "KAMI_LegacyOptions" in source
    assert 'world.iter("menu_item_check")' in source
    for button,target in ACTION_TARGETS.items():
        assert re.search(r'\b'+re.escape(button)+r'\b',header),button
        assert '"'+target+'"' in header, target
    # A menu rebrand MUST retain the original native actions, not delete
    # content or sever callback IDs. Their nodes may be re-labeled.
    assert "original_actions" in source
    assert "for item in original_actions:" in source
    assert "assert item in menu_after" in source
    assert "assert all(node in list(root)" in source
    assert "menu_file" in source and "floater_preferences.xml" in source
    for value in ("toolbar_bottom", "toolbar_top", "toolbar_left",
                  "toolbar_right", "tab_width", "KAMI"):
        assert value in source
    assert "return true" in header and "gl" not in header.lower()
    print("PASS native workspace launcher, menu callbacks, preserved tools, and dock architecture")

def assert_integrated():
    reg=read(VIEW/"llviewerfloaterreg.cpp")
    assert reg.count('#include "kami_workspace_hub.h"')==1
    assert reg.count('LLFloaterReg::add("kami_workspaces", "floater_kami_workspaces.xml"')==1
    hub=ET.parse(XUI/"floater_kami_workspaces.xml").getroot()
    assert hub.tag=="floater" and hub.get("name")=="kami_workspaces"
    assert "KAMI" in hub.get("title","")
    assert len([x for x in hub.iter("button")])==len(ACTION_TARGETS)
    assert {x.get("name") for x in hub.iter("button")}==set(ACTION_TARGETS)
    assert len([x for x in hub.iter("icon") if x.get("image_name")=="KAMI_Logo"])==1

    menu=ET.parse(XUI/"menu_viewer.xml").getroot()
    nav=[x for x in menu if x.tag=="menu" and x.get("name")=="KAMI_Workspaces"]
    assert len(nav)==1
    assert [x.get("label") for x in menu if x.tag=="menu" and x.get("name")=="Me"]==["KAMI"]
    link=[x for x in nav[0].iter("menu_item_call") if x.get("name")=="Kami.OpenWorkspaces"]
    assert len(link)==1
    assert link[0].find("menu_item_call.on_click").get("parameter")=="kami_workspaces"
    assert link[0].find("menu_item_call.on_click").get("function")=="Floater.Show"
    assert menu.get("bg_visible")=="true"
    # The new UI owns the original menu controls without making cloned items.
    world=[x for x in nav[0] if x.tag=="menu" and x.get("name")=="World"]
    assert len(world)==1
    preview=[x for x in world[0].iter("menu_item_check")
             if x.get("name")=="Kami DX12 Live Preview"]
    assert len(preview)==1
    check=preview[0].find("menu_item_check.on_check")
    click=preview[0].find("menu_item_check.on_click")
    assert check is not None and check.get("control")=="KamiDX12LivePreview"
    assert click is not None and click.get("function")=="ToggleControl"
    assert click.get("parameter")=="KamiDX12LivePreview"
    legacy=[x for x in nav[0] if x.tag=="menu" and x.get("name")=="KAMI_LegacyOptions"]
    assert len(legacy)==1 and len(list(legacy[0]))>=10
    assert any(x.get("name")=="BuildTools" for x in legacy[0])
    assert any(x.get("name")=="Help" for x in legacy[0])
    me=[x for x in menu if x.tag=="menu" and x.get("name")=="Me"]
    assert len(me)==1
    assert not any(child.tag=="menu" for child in me[0])
    # Original menu IDs and callbacks remain unique, including the DX12 toggle.
    assert len([x for x in menu.iter("menu") if x.get("name")=="World"])==1
    assert len([x for x in menu.iter("menu") if x.get("name")=="BuildTools"])==1
    assert len([x for x in menu.iter("menu_item_check")
                if x.get("name")=="Kami DX12 Live Preview"])==1
    # Critical legacy actions remain available.
    assert any(x.get("name")=="BuildTools" for x in menu.iter("menu"))
    assert any(x.get("name")=="Upload Model" for x in menu.iter())
    assert any(x.get("name")=="Take Snapshot" for x in menu.iter())
    assert any(x.get("name")=="Inventory" for x in menu.iter())

    prefs=ET.parse(XUI/"floater_preferences.xml").getroot()
    assert "KAMI" in prefs.get("title","")
    tabs=[x for x in prefs.iter("tab_container") if x.get("name")=="pref core"]
    assert len(tabs)==1 and tabs[0].get("tab_width")=="222"
    assert any("GRAPHICS + FPS" in e.get("label","") for e in tabs[0])
    assert any("AVATAR CONTROL" in e.get("label","") for e in tabs[0])
    assert len(list(tabs[0]))>=15
    assert tabs[0].get("width") and prefs.get("width")

    dock=ET.parse(XUI/"panel_toolbar_view.xml").getroot()
    bars={e.get("name"):e for e in dock.iter("toolbar")}
    assert all(x in bars for x in
               ("toolbar_top","toolbar_bottom","toolbar_left","toolbar_right"))
    assert bars["toolbar_top"].get("height")=="40"
    assert bars["toolbar_bottom"].get("height")=="54"
    assert bars["toolbar_left"].get("width")=="42"
    assert bars["toolbar_right"].get("width")=="42"
    # All user-installed tool actions remain the original native controller.
    print("PASS integrated KAMI Workspaces, menu, control deck, dock and original tool links")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--integrated",action="store_true")
    opts=p.parse_args()
    assert_native()
    if opts.integrated:assert_integrated()
    else:print("Source checks only; Windows integration and in-world UI QA are separate gates.")

if __name__=="__main__":
    main()
