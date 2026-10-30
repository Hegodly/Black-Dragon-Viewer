#!/usr/bin/env python3
"""Source contracts for native diamond glints; not an in-world visual test."""
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
V = R / "indra/newview"

def read(path):
    return (R / path).read_text(encoding="utf-8-sig")

def require(label, condition):
    if not condition:
        raise AssertionError("KAMI Diamond Glints: " + label)
    print("PASS", label)

shader = read("indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl")
glint = read("kami/jewelry/diamond-glint.glsl")
pipeline = read("indra/newview/pipeline.cpp")
require("authored GLSL integrated exactly once",
        shader.count(glint) == 1 and shader.count("kamiDiamondGlints(") == 2)
require("only PBR in existing deferred light pass",
        shader.index("GBUFFER_FLAG_HAS_PBR") <
        shader.index("color.rgb += kamiDiamondGlints(") <
        shader.index("else if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_HDRI))"))
require("no new texture or history buffers", all(word not in glint for word in (
        "sampler2D", "imageStore", "frameIndex", "gl_FragCoord", "atomic")))
require("light/view/normal driven, not frame-time randomized",
        all(word in glint for word in ("dot(n, h)", "dot(n, l)", "dot(n, v)",
                                       "pow(nDotH, exponent)", "smoothstep(7.0, 24.0"))))
require("direct reflection and cube-snapshot suppression",
        "color.rgb += kamiDiamondGlints(" in shader and
        "cube_snapshot == 0" in shader and "!gCubeSnapshot" in pipeline)
require("native settings bound in actual deferred draw",
        all(word in pipeline for word in (
            'bindDeferredShader(soften_shader);',
            'uniform1f(kami_glint_strength_uniform,',
            'uniform1f(kami_glint_fire_uniform,',
            '"KAMIDiamondGlints"', '"KAMIDiamondGlintStrength"',
            '"KAMIDiamondGlintFire"')))
require("Potato and Performance always bypass sparkle",
        '"KAMIPotatoMode"' in pipeline and
        '"KAMIPerformanceMode"' in pipeline and
        "kami_glint_active ?" in pipeline)
settings = ET.parse(V / "app_settings/settings_blackdragon.xml").getroot().find("map")
children = list(settings)
opts = {}
for i, node in enumerate(children[:-1]):
    if node.tag == "key" and children[i+1].tag == "map":
        mp = list(children[i+1])
        opts[node.text] = {n.text: mp[j+1].text
                           for j, n in enumerate(mp[:-1]) if n.tag == "key"}
require("opt-in off by default",
        opts["KAMIDiamondGlints"]["Value"] == "0" and
        opts["KAMIDiamondGlintFire"]["Value"] == "0" and
        opts["KAMIDiamondGlintStrength"]["Value"] == "0.85" and
        opts["KAMIDiamondGlintStrength"]["Type"] == "F32")
ui = ET.parse(V / "skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
named = {x.get("name"): x for x in ui.iter() if x.get("name")}
require("real opt-in and fire switches, native strength slider",
        named["kami_diamond_enable"].get("control_name") == "KAMIDiamondGlints" and
        named["kami_diamond_strength"].get("control_name") == "KAMIDiamondGlintStrength" and
        named["kami_diamond_fire"].get("control_name") == "KAMIDiamondGlintFire" and
        named["kami_diamond_strength"].tag == "slider")
require("UI height and scroll contain every control",
        int(named["kami_diamond_note"].get("top")) +
        int(named["kami_diamond_note"].get("height")) <=
        int(named["kombatant_display_quick"].get("height")) and
        int(named["gfx_scroll_panel"].get("height")) >
        int(named["kombatant_display_quick"].get("height")))
print("Diamond Glints static source verification passed; test real PBR jewelry in-world.")
