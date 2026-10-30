#!/usr/bin/env python3
"""Contain confirmed scaled-world cropping until presentation is hardware validated."""
from pathlib import Path
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[2]
p=R/'indra/newview/pipeline.cpp';s=p.read_text(encoding='utf-8-sig');a='static void kombatantScreenSize(U32& width, U32& height, bool main_target)\n{';assert s.count(a)==1;s=s.replace(a,a+'''
    // Safe default: disable unverified reduced world-pixel scaling ONLY
    // when the player has selected window-native resolution (target 0).
    // Explicit 4K/8K supersampling MUST reach the allocation path even
    // with Performance Mode enabled, otherwise the UI says "4K" while
    // the actual GPU buffer silently stays at the window's 1920 width.
    // Performance Mode's render multiplier is separately pinned to 1x.
    // Snapshot and reflection-probe targets retain their own sizes.
    if (main_target && gSavedSettings.getBOOL("KAMIPerformanceMode") &&
        gSavedSettings.getU32("KombatantRenderWidth") == 0) return;
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
  n.set('tool_tip','Scaled world rendering can crop the view. Performance Mode keeps the chosen window-native, 4K or 8K world buffer while retaining geometry and lighting optimizations.')
E.indent(tree,space=' ');tree.write(p,encoding='utf-8',xml_declaration=True)
print('Performance Mode preserves explicit native/4K/8K world buffers; unsafe adaptive resizing suspended; camera/FOV unchanged.')
