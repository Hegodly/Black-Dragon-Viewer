#!/usr/bin/env python3
"""Fix SSR scene/depth target allocation when its preference changes live."""
from pathlib import Path
import xml.etree.ElementTree as ET
R = Path(__file__).resolve().parents[2]
p = R / "indra/newview/pipeline.cpp"
s = p.read_text(encoding="utf-8-sig")
old = '    RenderScreenSpaceReflections = gSavedSettings.getBOOL("RenderScreenSpaceReflections");'
assert s.count(old) == 1
new = '''    const bool ssr_was_enabled = RenderScreenSpaceReflections;
    RenderScreenSpaceReflections = gSavedSettings.getBOOL("RenderScreenSpaceReflections");
    // mSceneMap (color + depth) is allocated ONLY with SSR enabled. Toggling
    // the preference without a resize otherwise leaves the setting visibly
    // checked while the renderer still has no SSR target, or the old target
    // allocated after switching SSR off. Rebuild it on either transition.
    if (ssr_was_enabled != RenderScreenSpaceReflections)
        gPipeline.requestResizeScreenTexture();'''
s=s.replace(old,new)
assert '$1gPipeline' not in s and 'gPipeline.requestResizeScreenTexture();' in s
assert 'if(RenderScreenSpaceReflections)' in s and 'mSceneMap.allocate' in s
p.write_text(s,encoding="utf-8")

ui = R / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
t = ET.parse(ui)
count = 0
for n in t.getroot().iter():
    if n.get("control_name") == "RenderScreenSpaceReflections":
        n.set("tool_tip", "Live screen-space reflections use the scene color/depth buffers and reflection probes on supported PBR materials. The viewer reallocates the required texture when toggled. This cannot reflect offscreen objects. Potato Mode and auto-quality stages may disable SSR.")
        count += 1
assert count >= 1
ET.indent(t,space=" ")
t.write(ui,encoding="utf-8",xml_declaration=True)
print(f"SSR live texture reallocation and dependency tooltip integrated ({count} controls)")
