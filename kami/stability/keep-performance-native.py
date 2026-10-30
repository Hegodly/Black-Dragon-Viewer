#!/usr/bin/env python3
"""Contain confirmed scaled-world cropping until presentation is hardware validated."""
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[2]
p=R/'indra/newview/pipeline.cpp';s=p.read_text(encoding='utf-8-sig');a='static void kombatantScreenSize(U32& width, U32& height, bool main_target)\n{';assert s.count(a)==1;s=s.replace(a,a+'''
    // Tested user report: reduced targets crop the presentation while UI stays
    // native. Keep Performance Mode on native dimensions until scaling is fixed.
    // Explicit snapshot dimensions and probe dimensions remain untouched.
    if (main_target && gSavedSettings.getBOOL("KAMIPerformanceMode")) return;
''');p.write_text(s,encoding='utf-8')
p=R/'indra/newview/kami_performance_mode.h';s=p.read_text();a='set("RenderResolutionMultiplier", 0.75);';assert a in s;s=s.replace(a,'set("RenderResolutionMultiplier", 1.0);');p.write_text(s,encoding='utf-8')
p=R/'indra/newview/llviewerdisplay.cpp';s=p.read_text(encoding='utf-8-sig')
start=s.index('    // KAMI native adaptive world resolution.')
opening=s.index('{',s.index('    if (!for_snapshot',start));depth=1;end=opening+1
while depth:
 if s[end]=='{':depth+=1
 elif s[end]=='}':depth-=1
 end+=1
s=s[:start]+'    // Adaptive resizing suspended pending a fix for scaled-world cropping.\n'+s[end:]
p.write_text(s,encoding='utf-8')
p=R/'indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml';tree=E.parse(p)
for n in tree.getroot().iter():
 if n.get('name')=='kami_adaptive_resolution':
  n.set('enabled','false');n.set('label','Adaptive resolution: temporarily suspended')
  n.set('tool_tip','Scaled world rendering can crop the view. Performance Mode currently uses native resolution while retaining its other optimizations.')
E.indent(tree,space=' ');tree.write(p,encoding='utf-8',xml_declaration=True)
print('Performance Mode stays native; adaptive target resizing suspended; camera input/FOV unchanged.')
