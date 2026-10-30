#!/usr/bin/env python3
"""Reduce the fixed login precache wait without shortening recovery timeouts."""
from pathlib import Path
import xml.etree.ElementTree as ET
R = Path(__file__).resolve().parents[2]
p = R/'indra/newview/app_settings/settings_blackdragon.xml'
s = p.read_text(encoding='utf-8-sig')
anchor='<map>\n'
assert s.count('<key>KAMIFastLogin</key>') == 0
s=s.replace(anchor, anchor+'''  <key>KAMIFastLogin</key>
  <map><key>Comment</key><string>Reduce the fixed world precache hold to 0.5 seconds once an avatar exists. Assets continue streaming normally; outfit readiness and recovery timeouts remain unchanged.</string><key>Persist</key><integer>1</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>1</integer></map>
''',1)
p.write_text(s,encoding='utf-8');ET.parse(p)
p=R/'indra/newview/llstartup.cpp';s=p.read_text(encoding='utf-8-sig')
old='if ((timeout_frac > 1.f) && isAgentAvatarValid())'
assert s.count(old)==1
s=s.replace(old,'''// Fast entry changes only the minimum precache hold. Keep timeout_frac
        // and the original recovery deadline below independent of this setting.
        const F32 minimum_precache = gSavedSettings.getBOOL("KAMIFastLogin")
            ? llmin(llmax(PRECACHING_DELAY, 0.f), 0.5f)
            : llmax(PRECACHING_DELAY, 0.f);
        if ((timeout.getElapsedTimeF32() > minimum_precache) && isAgentAvatarValid())''',1)
# A zero PrecachingDelay previously divided by zero; preserve its instant-entry
# intent while keeping progress and recovery calculations finite.
s=s.replace('timeout.getElapsedTimeF32() / PRECACHING_DELAY;', 'timeout.getElapsedTimeF32() / llmax(PRECACHING_DELAY, 0.1f);',1)
p.write_text(s,encoding='utf-8')
assert 'else if (timeout_frac > 10.f)' in s
assert 'gAgentWearables.areWearablesLoaded()' in s
# Fast login is independent of the retired MXAO panel. Attach directly
# to the existing Display scroll area and compute its actual current height.
p=R/'indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml'
doc=ET.parse(p)
root=doc.getroot()
panels=[n for n in root.iter("layout_panel") if n.get("name")=="kombatant_display_quick"]
assert len(panels)==1, "KAMI Display quick panel missing"
panel=panels[0]
previous=int(panel.get("height"))
assert previous>=4200, ("Unexpected graphics panel height",previous)
delta=100
outer=[n for n in root.iter() if n is not panel and n.get("height")==str(previous+4)]
assert outer, "Graphics scroll height parent not found"
for n in outer:n.set("height",str(previous+4+delta))
panel.set("height",str(previous+delta))
ET.SubElement(panel,"check_box",{
    "name":"kami_fast_login","control_name":"KAMIFastLogin",
    "label":"Fast login: shorten fixed precache wait (next login)",
    "left":"12","top":str(previous+8),"width":"465","height":"22"})
note=ET.SubElement(panel,"text",{
    "name":"kami_fast_login_help","left":"12","top":str(previous+38),
    "width":"465","height":"50","wrap":"true"})
note.text="World assets continue streaming after entry. Avatar and outfit checks remain active. Disable for the original precache wait."
ET.indent(doc,space=" ")
doc.write(p,encoding="utf-8",xml_declaration=True)
ET.parse(p)
print('Fast login integrated: shorter fixed hold, original outfit readiness and recovery guards retained.')
