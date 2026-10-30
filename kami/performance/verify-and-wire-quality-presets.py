#!/usr/bin/env python3
"""Route the actual quality dropdown through robust native KAMI quality presets.

Run after all Display settings patches and before Fire Garden themed controls.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
base=R/"kami/performance/kami_quality_presets.h"
assert base.is_file()
shutil.copyfile(base,R/"indra/newview/kami_quality_presets.h")

cpp=R/"indra/newview/llfloaterpreference.cpp"
s=cpp.read_text(encoding="utf-8-sig")
include='#include "kami_quality_presets.h"\n'
assert include not in s
anchor='#include "llfloaterpreference.h"\n'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+include,1)
start=s.index('    mCommitCallbackRegistrar.add("Pref.KombatantQuality",')
end=s.index("    });",start)+len("    });")
old=s[start:end]
assert "RenderVolumeLODFactor" in old and 'RenderFarClip' in old, "Upstream quality callback changed"
replacement='''    mCommitCallbackRegistrar.add("Pref.KombatantQuality", [](LLUICtrl* ctrl, const LLSD&) {
        // Use the shared, validated actual renderer preset, not an independent
        // list of partial settings that can drift from the requested preset.
        const S32 chosen = ctrl->getValue().asInteger();
        if (chosen >= 0 && chosen <= 4 && !KAMIQuality::apply(chosen))
            gSavedSettings.setS32("KAMIQualityPreset", -1);
    });'''
s=s[:start]+replacement+s[end:]
assert s.count('KAMIQuality::apply(chosen)')==1
cpp.write_text(s,encoding="utf-8")

settings=R/"indra/newview/app_settings/settings_blackdragon.xml"
tree=ET.parse(settings)
root=tree.getroot().find("map")
assert root is not None
assert not any(n.tag=="key" and n.text=="KAMIQualityPreset" for n in root)
key=ET.Element("key");key.text="KAMIQualityPreset"
entry=ET.Element("map")
for tag,name,value in (
 ("key","Comment",None),
 ("string",None,"Last quality preset selected in Display; manual graphics edits may override it. -1 = Custom."),
 ("key","Persist",None),
 ("integer",None,"1"),
 ("key","Type",None),
 ("string",None,"S32"),
 ("key","Value",None),
 ("integer",None,"-1"),
):
 e=ET.SubElement(entry,tag)
 if name is not None:e.text=name
 else:e.text=value
root.insert(0,entry);root.insert(0,key)
ET.indent(tree,space=" ")
tree.write(settings,encoding="utf-8",xml_declaration=True)

panel=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
pt=ET.parse(panel)
root=pt.getroot()
nodes=[n for n in root.iter() if n.get("name")=="kd_quality_preset"]
assert len(nodes)==1,nodes
combo=nodes[0]
assert len(combo.findall("combo_box.item"))==6
combo.set("control_name","KAMIQualityPreset")
combo.attrib.pop("value",None)
combo.set("tool_tip","Real native render quality levels, ordered Low, Medium, High, Ultra, Cinematic. They change draw distance, LOD, avatar/particle/light budgets, shadow resolution, reflections, ambient occlusion, glow and antialiasing. No forced FPS cap or render resolution change. Selecting a quality level turns off conflicting automatic modes.")
labels={
 "0":"Low — highest FPS",
 "1":"Medium — balanced",
 "2":"High — detailed",
 "3":"Ultra — richer lighting",
 "4":"Cinematic — maximum visuals",
}
for item in combo.findall("combo_box.item"):
 if item.get("value") in labels:item.set("label",labels[item.get("value")])
heading=[n for n in root.iter() if n.get("name")=="kd_preset_label"]
assert len(heading)==1
heading[0].text="Quality preset (last applied; custom edits override)"
ET.indent(pt,space=" ")
pt.write(panel,encoding="utf-8",xml_declaration=True)
ET.parse(settings);ET.parse(panel)
print("All five real native quality presets wired; saved selection; auto/Performance conflict cleanup; no FPS or 4K edits.")
