#!/usr/bin/env python3
"""Remove 12K live supersampling; preserve unrelated snapshot and VRAM settings."""
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[2]
removed=0
for p in (R/'indra/newview/skins/default/xui/en').glob('*.xml'):
 text=p.read_text(encoding='utf-8-sig')
 if 'KombatantRenderWidth' not in text:continue
 tree=E.parse(p)
 for n in tree.getroot().iter():
  if n.get('control_name')=='KombatantRenderWidth':
   for item in list(n):
    if item.tag=='combo_box.item' and int(item.get('value','0'))>7680:n.remove(item);removed+=1
 for n in tree.getroot().iter():
  if n.get('name')=='render_note':n.text='Renders above screen resolution, then fits to the display. Live rendering is limited to 8K; high resolutions can use substantial GPU memory. Resets on restart.'
 E.indent(tree,space=' ');tree.write(p,encoding='utf-8',xml_declaration=True)
assert removed>=1,'No 12K live controls found'
p=R/'indra/newview/pipeline.cpp';s=p.read_text(encoding='utf-8-sig');a='if (target <= 12288 && target <= limit';assert s.count(a)==1;s=s.replace(a,'if (target <= 7680 && target <= limit');p.write_text(s,encoding='utf-8')
print(f'Removed {removed} 12K live choices; renderer rejects live targets above 7680 and returns to native.')
