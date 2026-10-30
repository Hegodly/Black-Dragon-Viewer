#!/usr/bin/env python3
"""Make native KAMI quality tiers visibly distinct in PBR materials.

Run AFTER native Dark Cinematic and Diamond Glint integration. This changes
only in-viewer shading; the SL simulator, source materials, GShade and custom
render resolution are untouched. Off in Custom, Auto and Performance modes.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
PBR = VIEW / "app_settings/shaders/class3/deferred/softenLightF.glsl"
PIPELINE = VIEW / "pipeline.cpp"
PRESETS = ROOT / "kami/performance/kami_quality_presets.h"
SETTINGS = VIEW / "app_settings/settings_blackdragon.xml"
PANEL = VIEW / "skins/default/xui/en/panel_preferences_graphics1.xml"

def once(text, source, replacement, reason):
    count = text.count(source)
    if count != 1:
        raise AssertionError(f"KAMI visible quality: {reason} anchor count = {count}")
    return text.replace(source, replacement, 1)

# Both normal and specular PBR responses use the same material's authored
# roughness. We preserve the actual normal map and geometry, only establishing
# a minimum roughness for cost-effective, visibly softer Low/Medium reflections.
s = PBR.read_text(encoding="utf-8-sig")
s = once(s, "uniform float kami_diamond_glint_fire;",
    "uniform float kami_diamond_glint_fire;\n"
    "// -1 = custom/default untouched, 0..4 = Low..Cinematic.\n"
    "uniform float kami_preset_material_quality;",
    "quality shader uniform")
s = once(s, "void main()\n{",
    (ROOT / "kami/presets/pbr-fidelity.glsl").read_text(encoding="utf-8")
    + "\nvoid main()\n{", "quality shader helpers")
anchor = "        float metallic = orm.b;"
s = once(s, anchor, """        if (kami_preset_material_quality >= 0.0)
            perceptualRoughness = max(perceptualRoughness,
                kamiQualityRoughnessFloor(kami_preset_material_quality));
""" + anchor, "PBR roughness floor")
anchor = "        adjustIrradiance(irradiance, ambocc);"
# This appears again for legacy shading; only change the PBR block.
start = s.index("if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))")
end = s.index("else if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_HDRI))")
pbr = s[start:end]
pbr = once(pbr, anchor, """        if (kami_preset_material_quality >= 0.0)
            radiance *= kamiQualityProbeWeight(kami_preset_material_quality);

""" + anchor, "PBR probe contribution")
s = s[:start] + pbr + s[end:]
PBR.write_text(s, encoding="utf-8")

s = PIPELINE.read_text(encoding="utf-8-sig")
anchor = "            bindDeferredShader(soften_shader);"
code = """
            // Opt-in, native shader fidelity tied to the quality combo's saved
            // value. Custom and adaptive modes preserve authored PBR exactly.
            static LLStaticHashedString kami_preset_quality_uniform("kami_preset_material_quality");
            static LLCachedControl<S32> kami_quality_index(gSavedSettings, "KAMIQualityPreset", -1);
            const bool quality_owns_materials =
                !gCubeSnapshot && !gSavedSettings.getBOOL("KAMIPerformanceMode") &&
                !gSavedSettings.getBOOL("KAMIPotatoMode") &&
                !gSavedSettings.getBOOL("KAMIAutoTune") &&
                !gSavedSettings.getBOOL("KAMICrowdMode");
            const S32 quality_value = (S32)kami_quality_index;
            soften_shader.uniform1f(kami_preset_quality_uniform,
                quality_owns_materials && quality_value >= 0 && quality_value <= 4
                    ? (F32)quality_value : -1.f);
"""
s = once(s, anchor, anchor + code, "native deferred shader binding")
PIPELINE.write_text(s, encoding="utf-8")

# The original preset remains owner of the draw-distance, shadows, avatar
# impostors, local light, reflection, anti-aliasing and particle settings.
h = PRESETS.read_text(encoding="utf-8-sig")
anchor = '        set("KAMIQualityPreset", q);'
h = once(h, anchor, """        static const char* const visualStatus[] = {
            "LOW: softer PBR highlights, reduced reflections; distant detail budget reduced.",
            "MEDIUM: moderate PBR highlights and reflections; balanced scene detail.",
            "HIGH: detailed PBR response, native SSAO, higher geometry and lighting budgets.",
            "ULTRA: near-authored glossy PBR, HBAO, reflections and detailed shadows.",
            "CINEMATIC: authored PBR finish, richest probe lighting and scene detail."
        };
        set("KAMIQualityVisualStatus", LLSD(visualStatus[q]));
""" + anchor, "quality status update")
PRESETS.write_text(h, encoding="utf-8")

tree = ET.parse(SETTINGS)
root = tree.getroot().find("map")
if root is None or any(x.tag == "key" and x.text == "KAMIQualityVisualStatus" for x in root):
    raise RuntimeError("Quality status control missing map or already installed")
key = ET.SubElement(root, "key")
key.text = "KAMIQualityVisualStatus"
value = ET.SubElement(root, "map")
for tag, content in (
    ("key", "Comment"), ("string", "Last selected native quality tier and its visible shading contract."),
    ("key", "Persist"), ("integer", "0"), ("key", "Type"), ("string", "String"),
    ("key", "Value"), ("string", "Custom: authored PBR shading is preserved."),
):
    node = ET.SubElement(value, tag)
    node.text = content
ET.indent(tree, space=" ")
tree.write(SETTINGS, encoding="utf-8", xml_declaration=True)

# UI remains in the same Display section, but explicitly reports the visible
# shading tradeoff instead of suggesting that an FPS change proves quality.
tree = ET.parse(PANEL)
root = tree.getroot()
panels = [p for p in root.iter("layout_panel")
          if p.get("name") == "kombatant_display_quick"]
if len(panels) != 1:
    raise AssertionError("Quick graphics panel not found")
panel = panels[0]
if not any(p.get("name") == "kami_diamond_note" for p in panel):
    raise AssertionError("Quality stage must run after Diamond Glints")
height = int(panel.get("height"))
panel.set("height", str(height + 82))
ET.SubElement(panel, "text", {
    "name": "kami_quality_live_status",
    "control_name": "KAMIQualityVisualStatus",
    "left": "12", "top": str(height + 8),
    "width": "465", "height": "65",
    "wrap": "true", "layout": "topleft",
}).text = "Preset visual quality: Custom / not yet applied."
scroll = [p for p in root.iter("panel") if p.get("name") == "gfx_scroll_panel"]
stacks = [p for p in root.iter("layout_stack") if p.get("name") == "gfx_stack"]
if len(scroll) != 1 or len(stacks) != 1:
    raise AssertionError("Graphics scroll layout changed")
scroll[0].set("height", str(int(panel.get("height")) + 66))
children = [p for p in stacks[0] if p.tag == "layout_panel"]
stacks[0].set("height", str(max(int(stacks[0].get("height", "0")),
    sum(int(p.get("height", "0")) for p in children) + 96)))
ET.indent(tree, space=" ")
tree.write(PANEL, encoding="utf-8")
ET.parse(PANEL)
ET.parse(SETTINGS)
print("KAMI preset material fidelity installed; 5 visible PBR tiers, safe Custom bypass.")
