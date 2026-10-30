#!/usr/bin/env python3
"""Install opt-in native PBR diamond highlights into Kami's existing pass.

Build order: run after apply-dark-cinematic.py. The viewer keeps full control
of SL materials; this shader does not change uploaded inventory or server fees.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
V = R / "indra/newview"
SHADER = V / "app_settings/shaders/class3/deferred/softenLightF.glsl"
PIPELINE = V / "pipeline.cpp"
SETTINGS = V / "app_settings/settings_blackdragon.xml"
UI = V / "skins/default/xui/en/panel_preferences_graphics1.xml"


def once(data, anchor, replacement, label):
    if data.count(anchor) != 1:
        raise RuntimeError(f"Diamond Glints: {label} anchor changed ({data.count(anchor)})")
    return data.replace(anchor, replacement, 1)


s = SHADER.read_text(encoding="utf-8-sig")
uniform_anchor = "uniform float kami_cinematic_material;"
s = once(s, uniform_anchor, uniform_anchor + """
// KAMI optional native gemstone glints (dielectric PBR materials only).
uniform float kami_diamond_glint_strength;
uniform float kami_diamond_glint_fire;""", "shader uniform")
s = once(s, "void main()\n{",
         (R / "kami/jewelry/diamond-glint.glsl").read_text(encoding="utf-8") +
         "\nvoid main()\n{", "shader main")
lighting_anchor = """        color = pbrBaseLight(diffuseColor, specularColor, metallic, v, gb.normal, perceptualRoughness, light_dir, sunlit_linear, scol, radiance, irradiance, colorEmissive, ao, additive, atten);"""
s = once(s, lighting_anchor, lighting_anchor + """
        // Only non-HUD deferred PBR surfaces. No cost when the toggle is off.
        if (kami_diamond_glint_strength > 0.001 && cube_snapshot == 0)
            color.rgb += kamiDiamondGlints(
                gb.normal, v, light_dir, baseColor.rgb,
                perceptualRoughness, metallic, length(pos.xyz),
                sunlit_linear, scol);""", "PBR light path")
SHADER.write_text(s, encoding="utf-8")

s = PIPELINE.read_text(encoding="utf-8-sig")
bind_anchor = "            bindDeferredShader(soften_shader);"
bind_code = """
            // Native, opt-in PBR facet glints; never a GShade postprocess.
            static LLStaticHashedString kami_glint_strength_uniform("kami_diamond_glint_strength");
            static LLStaticHashedString kami_glint_fire_uniform("kami_diamond_glint_fire");
            static LLCachedControl<bool> kami_glints_enabled(
                gSavedSettings, "KAMIDiamondGlints", false);
            static LLCachedControl<F32> kami_glint_strength(
                gSavedSettings, "KAMIDiamondGlintStrength", 0.85f);
            static LLCachedControl<bool> kami_glint_fire(
                gSavedSettings, "KAMIDiamondGlintFire", false);
            const bool kami_glint_active =
                kami_glints_enabled && !gCubeSnapshot &&
                !gSavedSettings.getBOOL("KAMIPotatoMode") &&
                !gSavedSettings.getBOOL("KAMIPerformanceMode");
            soften_shader.uniform1f(kami_glint_strength_uniform,
                kami_glint_active ? llclamp((F32)kami_glint_strength, 0.f, 2.f) : 0.f);
            soften_shader.uniform1f(kami_glint_fire_uniform,
                kami_glint_active && kami_glint_fire ? 1.f : 0.f);
"""
s = once(s, bind_anchor, bind_anchor + bind_code, "deferred shader binding")
PIPELINE.write_text(s, encoding="utf-8")

doc = ET.parse(SETTINGS)
root = doc.getroot().find("map")
if root is None:
    raise RuntimeError("Diamond Glints: missing native settings map")
for key, typ, tag, val, comment in (
    ("KAMIDiamondGlints", "Boolean", "integer", "0",
     "Opt-in native specular facets on bright low-roughness nonmetallic PBR surfaces."),
    ("KAMIDiamondGlintStrength", "F32", "real", "0.85",
     "Strength of procedural gemstone specular glints, range zero to two."),
    ("KAMIDiamondGlintFire", "Boolean", "integer", "0",
     "Optional subtle chromatic highlights. Not ray-traced dispersion."),
):
    if any(x.tag == "key" and x.text == key for x in root):
        raise RuntimeError(f"Diamond Glints: duplicate setting {key}")
    keynode = ET.SubElement(root, "key")
    keynode.text = key
    valnode = ET.SubElement(root, "map")
    for childtag, value in (
        ("key", "Comment"), ("string", comment),
        ("key", "Persist"), ("integer", "1"),
        ("key", "Type"), ("string", typ),
        ("key", "Value"), (tag, val),
    ):
        node = ET.SubElement(valnode, childtag)
        node.text = value
ET.indent(doc, space=" ")
doc.write(SETTINGS, encoding="utf-8", xml_declaration=True)

tree = ET.parse(UI)
root = tree.getroot()
panels = [x for x in root.iter("layout_panel")
          if x.get("name") == "kombatant_display_quick"]
if len(panels) != 1:
    raise RuntimeError("Diamond Glints: KAMI Display quick panel not found")
panel = panels[0]
if not any(x.get("name") == "kami_cine_heading" for x in panel):
    raise RuntimeError("Diamond Glints: Dark Cinematic stage must run first")
height = int(panel.get("height"))
delta = 184
panel.set("height", str(height + delta))

def widget(tag, name, y, h, **attrs):
    data = {"name": name, "left": "12", "top": str(height + y),
            "height": str(h), "width": "465", "layout": "topleft"}
    data.update({k: str(v) for k, v in attrs.items()})
    return ET.SubElement(panel, tag, data)

widget("text", "kami_diamond_heading", 5, 27,
       font="SansSerifLarge").text = "DIAMOND GLINTS  |  Native PBR"
widget("check_box", "kami_diamond_enable", 39, 23,
       control_name="KAMIDiamondGlints",
       label="Diamond sparkle (experimental)",
       tool_tip="Angle-dependent glints on light low-roughness dielectric PBR materials.")
widget("slider", "kami_diamond_strength", 74, 20,
       control_name="KAMIDiamondGlintStrength", label="Sparkle strength",
       min_val="0", max_val="2", increment="0.05",
       decimal_digits="2", can_edit_text="true")
widget("check_box", "kami_diamond_fire", 106, 23,
       control_name="KAMIDiamondGlintFire",
       label="Subtle rainbow diamond fire",
       tool_tip="Approximate gentle chromatic sparkle; not true spectral dispersion.")
widget("text", "kami_diamond_note", 137, 40,
       wrap="true").text = (
           "Opt-in PBR material heuristic, not automatic gem identification. "
           "Disabled in Potato/Performance Mode; fades with distance.")
scroll = [x for x in root.iter("panel") if x.get("name") == "gfx_scroll_panel"]
stack = [x for x in root.iter("layout_stack") if x.get("name") == "gfx_stack"]
if len(scroll) != 1 or len(stack) != 1:
    raise RuntimeError("Diamond Glints: display scroll layout changed")
scroll[0].set("height", str(int(panel.get("height")) + 66))
parts = [x for x in stack[0] if x.tag == "layout_panel"]
stack[0].set("height", str(max(int(stack[0].get("height", "0")),
                                sum(int(x.get("height", "0")) for x in parts) + 96)))
ET.indent(tree, space=" ")
tree.write(UI, encoding="utf-8")

ET.parse(SETTINGS)
ET.parse(UI)
print("KAMI diamond glints installed: PBR-only, opt-in, adjustable, no extra pass.")
