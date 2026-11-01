#!/usr/bin/env python3
from pathlib import Path
import importlib.util
import itertools
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('compact',Path(__file__).with_name('compact-graphics-layout.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=ROOT/'indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml'
r=ET.parse(p).getroot();before=m.signature(r);m.compact(r);assert before==m.signature(r)
s=next(e for e in r.iter('layout_stack') if e.get('name')=='gfx_stack')
q=next(e for e in s if e.get('name')=='kombatant_display_quick')
rects=[(int(e.get('top')),int(e.get('top'))+int(e.get('height','0')),e.get('name')) for e in q]
assert max(b for a,b,n in rects)<=int(q.get('height'))
rows=sorted(set(a for a,b,n in rects));bottom=0
for a in rows:
 assert a-bottom<=8,(a,bottom)
 bottom=max(bottom,max(b for t,b,n in rects if t==a))
# All combinations of the original collapsible groups include added KAMI cards.
panels=[e for e in s if e.tag=='layout_panel'];fixed=sum(int(e.get('height')) for e in panels if not e.get('visibility_control'))
assert fixed>=int(q.get('height'))
for state in itertools.product([False,True],repeat=3):
 visible=[e for e in panels if not e.get('visibility_control') or state[['PrefsViewerVisible','PrefsDeferredVisible','PrefsPostVisible'].index(e.get('visibility_control'))]]
 height=sum(int(e.get('height')) for e in visible)+4*(len(visible)-1)
 assert height>=fixed
print('PASS compact geometry, all renderer bindings and eight collapse-state extents')
