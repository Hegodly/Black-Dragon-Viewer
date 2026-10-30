#!/usr/bin/env python3
"""Remove KAMI-added grading from the native gamma pass, retaining core gamma/tone mapping."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
grading = {
    "KombatantNativeFX", "KombatantSaturation", "KombatantContrast",
    "KombatantVignette", "KombatantTemperature", "KombatantTint",
    "KombatantGrain", "KombatantVignetteSize", "KombatantVignetteSoftness"
}

shader = R / "indra/newview/app_settings/shaders/class1/deferred/postDeferredGammaCorrect.glsl"
s = shader.read_text(encoding="utf-8-sig")
assert "uniform vec3 kombatant_fx;" in s, "Expected grading shader not found"
s = re.sub(r"^uniform (?:vec2|vec3) kombatant_(?:fx|color|vignette_shape);[^\n]*\n", "", s, flags=re.M)
start = s.index("    diff.rgb = clamp(diff.rgb, vec3(0.0), vec3(1.0));")
end = s.index("\n}", start)
s = s[:start] + "    diff.rgb = clamp(diff.rgb, vec3(0.0), vec3(1.0));\n    frag_color = diff;" + s[end:]
assert "kombatant_" not in s
shader.write_text(s, encoding="utf-8")

pipeline = R / "indra/newview/pipeline.cpp"
s = pipeline.read_text(encoding="utf-8-sig")
start = s.index("        static LLCachedControl<bool> native_fx(gSavedSettings")
end = s.index("        mScreenTriangleVB->setBuffer();", start)
section = s[start:end]
assert "shader.uniform3f(color_params" in section and "shader.uniform3f(fx_params" in section
assert "bindTexture" not in section, "Unexpected code between grading and draw; review manually"
s = s[:start] + s[end:]
pipeline.write_text(s, encoding="utf-8")

settings = R / "indra/newview/app_settings/settings_blackdragon.xml"
tree = ET.parse(settings)
root = tree.getroot().find("map")
removed = set()
children = list(root)
for i in range(len(children) - 2, -1, -1):
    if children[i].tag == "key" and children[i].text in grading:
        assert children[i + 1].tag == "map"
        removed.add(children[i].text)
        root.remove(children[i + 1])
        root.remove(children[i])
assert removed == grading, f"Missing KAMI grading settings: {grading - removed}"
ET.indent(tree, space="  ")
tree.write(settings, encoding="utf-8", xml_declaration=True)

for name in (
    "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml",
    "indra/newview/skins/default/xui/en/floater_kombatant_effects.xml",
):
    p = R / name
    tree = ET.parse(p)
    for parent in tree.getroot().iter():
        for child in list(parent):
            if child.get("control_name") in grading:
                parent.remove(child)
    ET.indent(tree, space=" ")
    tree.write(p, encoding="utf-8", xml_declaration=True)
    assert not any(n.get("control_name") in grading for n in tree.getroot().iter())

print("Removed KAMI native color grading uniforms, shader math and controls; core gamma/tone mapping untouched.")
