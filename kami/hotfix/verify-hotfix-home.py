#!/usr/bin/env python3
"""Preflight and integrated-source checks for the native Hotfix Home prototype."""
from pathlib import Path
import argparse
import ast
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
HOTFIX = ROOT / "kami/hotfix"
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"

p = argparse.ArgumentParser()
p.add_argument("--integrated", action="store_true")
opt = p.parse_args()

src = (HOTFIX / "apply-hotfix-home.py").read_text(encoding="utf-8")
ast.parse(src)
header = (HOTFIX / "kami_hotfix_center.h").read_text(encoding="utf-8")
assert "class KAMIHotfixCenter final : public LLFloater" in header
assert "closeFloater();" in header
assert "setClickedCallback" in header
assert 'LLFloaterReg::showInstance' not in header  # no fake install action
assert "NOT YET ENABLED" in src and "No download, forced logout" in src
assert "floater_kami_hotfix_home.xml" in src
assert "Kami.HotfixHome" in src
assert "Floater.Show" in src
assert "KAMI_Workspaces" in src

if opt.integrated:
    h = (VIEW / "kami_hotfix_center.h").read_text(encoding="utf-8")
    reg = (VIEW / "llviewerfloaterreg.cpp").read_text(encoding="utf-8")
    assert h == header
    assert reg.count('#include "kami_hotfix_center.h"') == 1
    assert reg.count('LLFloaterReg::add("kami_hotfix_home"') == 1
    panel = ET.parse(XUI / "floater_kami_hotfix_home.xml").getroot()
    assert panel.get("name") == "kami_hotfix_home"
    names = [e.get("name") for e in panel.iter() if e.get("name")]
    for name in ("kami_hotfix_state", "kami_hotfix_close", "kami_hotfix_info"):
        assert names.count(name) == 1, name
    menu = ET.parse(XUI / "menu_viewer.xml").getroot()
    actions = [e for e in menu.iter("menu_item_call")
               if e.get("name") == "Kami.HotfixHome"]
    assert len(actions) == 1
    clicks = list(actions[0].iter("menu_item_call.on_click"))
    assert len(clicks) == 1
    assert clicks[0].get("function") == "Floater.Show"
    assert clicks[0].get("parameter") == "kami_hotfix_home"

print("PASS KAMI Hotfix Home UI contracts", "integrated" if opt.integrated else "preflight")
