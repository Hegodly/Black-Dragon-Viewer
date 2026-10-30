#!/usr/bin/env python3
"""Verify Firestorm-style KAMI WASD defaults, without overwriting custom binds."""
from pathlib import Path
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
file=root/"indra/newview/app_settings/key_bindings.xml"
tree=ET.parse(file).getroot()
expected={
    "third_person":{
        ("W","NONE"):"push_forward",
        ("A","NONE"):"turn_left",
        ("S","NONE"):"push_backward",
        ("D","NONE"):"turn_right",
        ("A","SHIFT"):"slide_left",
        ("D","SHIFT"):"slide_right",
        ("W","SHIFT"):"push_forward",
        ("S","SHIFT"):"push_backward",
        ("LEFT","NONE"):"turn_left",
        ("RIGHT","NONE"):"turn_right",
        ("UP","NONE"):"push_forward",
        ("DOWN","NONE"):"push_backward",
        ("E","NONE"):"jump",
        ("C","NONE"):"push_down",
        ("F","NONE"):"toggle_fly",
        ("ENTER","NONE"):"start_chat",
    },
    "first_person":{
        ("W","NONE"):"push_forward",
        ("A","NONE"):"slide_left",
        ("S","NONE"):"push_backward",
        ("D","NONE"):"slide_right",
    },
    "sitting":{
        ("W","NONE"):"move_forward_sitting",
        ("A","NONE"):"spin_around_cw_sitting",
        ("S","NONE"):"move_backward_sitting",
        ("D","NONE"):"spin_around_ccw_sitting",
    }
}
for mode,bindings in expected.items():
    node=tree.find(mode)
    if node is None: raise AssertionError("Missing movement mode: "+mode)
    seen={}
    for bind in node.findall("binding"):
        # Mouse-only/unbound rows do not share a keyboard key.
        if not bind.get("key"):
            continue
        key=(bind.get("key"),bind.get("mask"))
        if key in seen and seen[key]!=bind.get("command"):
            raise AssertionError(f"{mode}: conflicting actions for {key}")
        seen[key]=bind.get("command")
    for key,action in bindings.items():
        if seen.get(key)!=action:
            raise AssertionError(f"{mode}: {key} expected {action}, got {seen.get(key)}")
print("KAMI WASD primary movement and arrow-key fallback validated; custom binds untouched.")
