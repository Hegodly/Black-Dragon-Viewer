#!/usr/bin/env python3
"""Remove deprecated Native Graphics Lab and KAMI Photo Studio floaters.

Retains the direct graphics controls in Preferences > Display and the modern
KAMI performance engine. Removes only obsolete legacy floater entry points,
registrations and private UI classes once dependent build patches have run.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
skin=R/"indra/newview/skins/default/xui/en"
menu=skin/"menu_viewer.xml"
tree=ET.parse(menu)
root=tree.getroot()
removed={}
def drop_named(tree, names, report):
    for parent in tree.iter():
        for child in list(parent):
            if child.get("name") in names:
                key=child.get("name")
                report[key]=report.get(key,0)+1
                parent.remove(child)

drop_named(root,{"Kombatant Effects","Kombatant Studio"},removed)
assert removed=={"Kombatant Effects":1,"Kombatant Studio":1},removed
ET.indent(tree,space=" ")
tree.write(menu,encoding="utf-8",xml_declaration=True)

panel=skin/"panel_preferences_graphics1.xml"
tree=ET.parse(panel)
found={}
drop_named(tree.getroot(),{"kd_effects","kd_studio"},found)
assert found=={"kd_effects":1,"kd_studio":1},found
ET.indent(tree,space=" ")
tree.write(panel,encoding="utf-8",xml_declaration=True)

reg=R/"indra/newview/llviewerfloaterreg.cpp"
s=reg.read_text(encoding="utf-8-sig")
for line in (
    '#include "kombatantstudio.h"\n',
    '#include "kombatanteffects.h"\n',
):
    assert s.count(line)==1,("Missing obsolete floater header include",line)
    s=s.replace(line,"",1)
for floater in ("kombatant_effects","kombatant_studio"):
    matches=[line for line in s.splitlines(True) if f'LLFloaterReg::add("{floater}"' in line]
    assert len(matches)==1,(floater,len(matches))
    s=s.replace(matches[0],"",1)
assert "KombatantStudio" not in s and "KombatantEffects" not in s
reg.write_text(s,encoding="utf-8")
for path in (
    R/"indra/newview/kombatantstudio.h",
    R/"indra/newview/kombatanteffects.h",
    skin/"floater_kombatant_studio.xml",
    skin/"floater_kombatant_effects.xml",
):
    assert path.is_file(),f"Expected unused floater source: {path}"
    path.unlink()

ET.parse(menu)
ET.parse(panel)
assert "kombatant_effects" not in menu.read_text() and "kombatant_studio" not in menu.read_text()
assert "kombatant_effects" not in panel.read_text() and "kombatant_studio" not in panel.read_text()
assert 'KAMIPerformanceMode' in panel.read_text()
print("Removed Native Graphics Lab and KAMI Photo Studio from menus, Display quick buttons, C++ registrations, and portable UI assets.")
print("All graphics settings remain accessible through the main Display preferences.")
