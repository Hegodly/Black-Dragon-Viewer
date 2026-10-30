#!/usr/bin/env python3
"""Windows Performance Monitor, actual DXGI/PDH/Windows counters, Fire Garden UI."""
from pathlib import Path
import shutil, xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
shutil.copyfile(R/"kami/performance/kami_hardware_monitor.h",
                R/"indra/newview/kami_hardware_monitor.h")
reg=R/"indra/newview/llviewerfloaterreg.cpp"
s=reg.read_text(encoding="utf-8-sig")
old='#include "llfloaterperformance.h"\n'
assert s.count(old)==1
s=s.replace(old,old+'#include "kami_hardware_monitor.h"\n',1)
old='    LLFloaterReg::add("performance", "floater_performance.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPerformance>);'
assert s.count(old)==1
s=s.replace(old,old+'''
    LLFloaterReg::add("kami_hardware_monitor", "floater_kami_hardware_monitor.xml",
                     (LLFloaterBuildFunc)&LLFloaterReg::build<KAMIHardwareMonitor>);''',1)
reg.write_text(s,encoding="utf-8")

ui=R/"indra/newview/skins/default/xui/en/floater_kami_hardware_monitor.xml"
floater=ET.Element("floater",{
 "name":"kami_hardware_monitor","title":"KAMI | PERFORMANCE MONITOR",
 "width":"410","height":"386","min_width":"380","min_height":"340",
 "layout":"topleft","save_rect":"true","reuse_instance":"true","can_close":"true",
 "can_resize":"false","bg_alpha_color":"0.035 0.055 0.085 0.96",
})
ET.SubElement(floater,"text",{
 "name":"kami_hw_brand","font":"SansSerifBoldLarge","left":"20","top":"34",
 "width":"360","height":"27","text_color":"0.48 0.76 0.96 1"
}).text="KAMI BEYOND REALITY / HARDWARE"
ET.SubElement(floater,"text",{
 "name":"kami_hw_sub","left":"20","top":"62","width":"360",
 "height":"28","text_color":"0.76 0.85 0.94 1"
}).text="Live hardware usage  |  1 second refresh"
for idx,(name,default) in enumerate([
 ("kami_hw_fps","FPS: collecting..."),
 ("kami_hw_frame","Frame time: collecting..."),
 ("kami_hw_cpu","CPU (system): collecting..."),
 ("kami_hw_gpu","GPU busiest engine: collecting..."),
 ("kami_hw_ram","RAM (system): collecting..."),
 ("kami_hw_process","KAMI process RAM: collecting..."),
 ("kami_hw_vram","VRAM (adapter 0): collecting..."),
]):
    ET.SubElement(floater,"text",{
      "name":name,"left":"20","top":str(99+idx*29),
      "width":"367","height":"24","font":"SansSerif","text_color":"0.9 0.95 1 1"
    }).text=default
ET.SubElement(floater,"text",{
 "name":"kami_hw_resolution","left":"20","top":"305","width":"367","height":"26",
 "wrap":"true","text_color":"0.56 0.80 1 1"
}).text="World buffer: initializing"
ET.SubElement(floater,"text",{
 "name":"kami_hw_note","left":"20","top":"339","width":"366","height":"42",
 "wrap":"true","text_color":"0.67 0.75 0.86 1"
}).text="Only samples while visible. GPU counters may be unavailable or differ from total GPU activity."
ET.indent(floater,space=" ")
ET.ElementTree(floater).write(ui,encoding="utf-8",xml_declaration=True)

menu=R/"indra/newview/skins/default/xui/en/menu_viewer.xml"
tree=ET.parse(menu)
root=tree.getroot()
me=[e for e in root.iter("menu") if e.get("name")=="Me"]
assert len(me)==1
item=ET.Element("menu_item_check",{
    "label":"KAMI Hardware Monitor (GPU / CPU / RAM)",
    "name":"KAMI Hardware Monitor",
})
ET.SubElement(item,"menu_item_check.on_click",
              {"function":"Floater.Toggle","parameter":"kami_hardware_monitor"})
ET.SubElement(item,"menu_item_check.on_check",
              {"function":"Floater.Visible","parameter":"kami_hardware_monitor"})
# Add beneath the existing local KAMI Preferences entry, not under the
# removed Native Graphics Lab or Photo Studio.
prefs=[i for i,e in enumerate(list(me[0])) if e.get("name")=="Preferences"]
if prefs:me[0].insert(prefs[0]+1,item)
else:me[0].append(item)
ET.indent(tree,space=" ")
tree.write(menu,encoding="utf-8",xml_declaration=True)

# Styling uses shared skinned Fire Garden controls/colors. No new texture pass.
ET.parse(menu);ET.parse(ui)
assert 'KAMIHardwareMonitor' in reg.read_text(encoding="utf-8")
print("KAMI Hardware Monitor registered. Native Windows counters, one sample/sec while visible, accurate N/A for unavailable GPU metrics.")
