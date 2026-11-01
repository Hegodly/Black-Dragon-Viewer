#!/usr/bin/env python3
"""Validate native startup WASD behavior without touching personalized binds."""
import xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
app=(ROOT/"indra/newview/llappviewer.cpp").read_text(encoding="utf-8-sig")
viewer=(ROOT/"indra/newview/llviewerinput.cpp").read_text(encoding="utf-8-sig")
key_xml=ET.parse(ROOT/"indra/newview/app_settings/key_bindings.xml").getroot()
controls=ET.parse(ROOT/"indra/newview/app_settings/controls.xml").getroot()
assert "loadKeyboardlayout();" in app
assert 'getExpandedFilename(LL_PATH_USER_SETTINGS, "controls.xml")' in app
assert 'getExpandedFilename(LL_PATH_APP_SETTINGS, "controls.xml")' in app
assert "gViewerInput.loadBindingsSettings(key_bindings_file)" in app
assert "migrate_legacy_wasd" in viewer
assert 'legacy_key("W", "push_forward")' in viewer
assert 'legacy_key("S", "push_backward")' in viewer
assert 'legacy_key("A", "slide_left")' in viewer
assert 'legacy_key("D", "slide_right")' in viewer
assert 'legacy_key("Q", "turn_left")' in viewer
assert 'legacy_key("E", "turn_right")' in viewer
assert "migrate_legacy_arrows" in viewer
for check in (
    'legacy_key("Up", "push_forward")',
    'legacy_key("Down", "push_backward")',
    'legacy_key("Left", "slide_left")',
    'legacy_key("Right", "slide_right")',
):
    assert check in viewer
assert 'record["key"].asString() == "Left"' in viewer
assert 'record["key"].asString() == "Right"' in viewer
assert 'record["function"] = "turn_left";' in viewer
assert 'record["function"] = "turn_right";' in viewer
assert 'record["mode"].asInteger() != 0' in viewer
assert 'matches == 1;' in viewer
assert "infile.close();" in viewer
# Export is a separate function that legitimately serializes saved binds.
# Only the loader/migration must avoid rewriting residents' controls.
loader = viewer.split("S32 LLViewerInput::loadBindingsSettings(", 1)[1].split(
    "EKeyboardMode LLViewerInput::getMode()", 1)[0]
assert 'LLSDSerialize::toXML(records' not in loader
assert "return loaded_bindings > 0;" in viewer
for tag,mapping in (("third_person",{"W":"push_forward","A":"turn_left",
                                    "S":"push_backward","D":"turn_right",
                                    "UP":"push_forward","DOWN":"push_backward",
                                    "LEFT":"turn_left","RIGHT":"turn_right"}),):
    node=key_xml.find(tag)
    assert node is not None
    lookup={(e.get("key"),e.get("mask")):e.get("command")
            for e in node.findall("binding")}
    for key,cmd in mapping.items():
        assert lookup.get((key,"NONE"))==cmd
records=[]
for node in controls.findall("./array/map"):
    fields=list(node)
    mapping={fields[i].text:fields[i+1].text for i in range(0,len(fields),2)}
    if mapping.get("mode")=="0" and mapping.get("mask")=="NONE":
        records.append(mapping)
for k,expected in (("W","push_forward"),("A","turn_left"),
                   ("S","push_backward"),("D","turn_right"),
                   ("Up","push_forward"),("Down","push_backward"),
                   ("Left","turn_left"),("Right","turn_right")):
    matches=[r["function"] for r in records if r.get("key")==k]
    assert matches == [expected],(k,matches)
print("PASS: stock KAMI WASD and arrow-key controls, exact legacy startup migrations, custom layouts preserved")
