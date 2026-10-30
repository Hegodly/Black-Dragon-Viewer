#!/usr/bin/env python3
"""Separate the automatic quality target from the existing microsecond FPS cap."""
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[2]
p=R/'indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml';tree=E.parse(p);root=tree.getroot()
cap=next(n for n in root.iter() if n.get('name')=='FramePerSecondLimit')
for n in list(cap):
 if n.tag=='combo_box.item':cap.remove(n)
for fps in (30,45,60,90,100,120,144,165,240):
 E.SubElement(cap,'combo_box.item',label=f'{fps} FPS',value=str(round(1000000/fps)))
E.SubElement(cap,'combo_box.item',label='Unlimited',value='0')
cap.set('tool_tip','Actual active-window FPS limit. Independent of the automatic graphics quality target. Frame generation should be off when measuring the native cap.')
for n in root.iter():
 if n.get('control_name')=='KAMIAutoTune':n.set('label','Auto-adjust quality toward target FPS (does not cap FPS)')
 if n.get('name')=='kami_target':
  for child in n:
   if child.get('label'):child.set('label',child.get('label').replace('Target ','Quality target: '))
E.indent(tree,space=' ');tree.write(p,encoding='utf-8',xml_declaration=True)
p=R/'indra/newview/llviewerdisplay.cpp';s=p.read_text(encoding='utf-8-sig')
a='const U32 target = llclamp(gSavedSettings.getU32("KAMITargetFPS"), 30U, 120U);';assert s.count(a)==1
s=s.replace(a,'''const U32 desired = llclamp(gSavedSettings.getU32("KAMITargetFPS"), 30U, 120U);
                const S32 cap_us = gSavedSettings.getS32("FramePerSecondLimit");
                const F64 target = cap_us > 0 ? llmin(static_cast<F64>(desired), 1000000.0 / cap_us) : desired;''');p.write_text(s,encoding='utf-8')
# Contract: caps use microseconds, not FPS. Menu values must match named rates.
for n in cap.findall('combo_box.item'):
 value=int(n.get('value'))
 if value:assert abs(1000000/value-float(n.get('label').split()[0]))<0.1
print('FPS cap menu validated; adaptive quality honors the cap using correct time units.')
