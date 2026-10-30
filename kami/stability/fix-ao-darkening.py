#!/usr/bin/env python3
"""Make AO darkening control visibly affect native PBR and legacy indirect light.

This is not post-process color grading. It adjusts the contribution of the
existing SSAO/HBAO light-map occlusion in the existing diffuse irradiance pass.
No extra depth samples or render targets are added.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]

settings = R / "indra/newview/app_settings/settings_blackdragon.xml"
s = settings.read_text(encoding="utf-8-sig")
anchor = "<key>KAMIMXAO</key>"
assert s.count(anchor) == 1, "Cannot add AO setting after MXAO integration"
s = s.replace(anchor, '''<key>KAMIAODarkening</key>
  <map><key>Comment</key><string>Native AO indirect-light darkening; 0 off, 1 normal, 2 stronger. Requires SSAO enabled; no extra graphics pass.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>1.0</real></map>
<key>KAMIMXAO</key>''', 1)
settings.write_text(s, encoding="utf-8")

shader = R / "indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl"
s = shader.read_text(encoding="utf-8-sig")
old = '''uniform float ssao_irradiance_scale;
uniform float ssao_irradiance_max;'''
assert s.count(old) == 1
s = s.replace(old, old + '''
uniform float kami_ao_darkening; // 0 = no screen-space shading, 1 = normal, 2 = strong''', 1)
old = '''    irradiance = mix(ssao_effect_mat * min(irradiance.rgb*ssao_irradiance_scale, vec3(ssao_irradiance_max)), irradiance.rgb, ambocc);'''
new = '''    // SSAO is encoded as ambient *visibility* (1 = unoccluded).
    // The legacy "SSAO Effect" matrix changes the fully shaded color but did
    // not reliably increase occlusion coverage. Apply strength to visibility
    // instead so the slider has a predictable, visible effect.
    float ao_occlusion = clamp((1.0 - clamp(ambocc, 0.0, 1.0)) *
                               clamp(kami_ao_darkening, 0.0, 2.0), 0.0, 1.0);
    vec3 occluded_irradiance = ssao_effect_mat *
        min(irradiance.rgb * ssao_irradiance_scale, vec3(ssao_irradiance_max));
    irradiance = mix(irradiance.rgb, occluded_irradiance, ao_occlusion);'''
assert s.count(old) == 1, "Original AO shading expression changed"
s = s.replace(old, new, 1)
shader.write_text(s, encoding="utf-8")

pipeline = R / "indra/newview/pipeline.cpp"
s = pipeline.read_text(encoding="utf-8-sig")
old = '''            soften_shader.uniform1f(ssao_scale_str, ssao_scale);
            soften_shader.uniform1f(ssao_max_str, ssao_max);'''
new = '''            soften_shader.uniform1f(ssao_scale_str, ssao_scale);
            soften_shader.uniform1f(ssao_max_str, ssao_max);
            static LLCachedControl<F32> ao_darkening(gSavedSettings, "KAMIAODarkening", 1.f);
            static LLStaticHashedString ao_darkening_uniform("kami_ao_darkening");
            soften_shader.uniform1f(ao_darkening_uniform, llclamp((F32)ao_darkening, 0.f, 2.f));'''
assert s.count(old) == 1, "Soften shader AO upload anchor changed"
s = s.replace(old,new,1)
pipeline.write_text(s, encoding="utf-8")

ui = R / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree = ET.parse(ui)
nodes = [n for n in tree.getroot().iter() if n.get("name") == "kd_ao_strength"]
assert len(nodes) == 1, f"Expected native AO darkening UI once, found {len(nodes)}"
slider = nodes[0]
assert slider.get("control_name") == "RenderSSAOEffect", "AO control was already rebound"
slider.attrib.update(control_name="KAMIAODarkening", label="AO darkening (0-2)",
                     min_val="0",max_val="2",increment="0.05",decimal_digits="2",
                     tool_tip="Adjusts real SSAO/HBAO contact darkening on PBR and legacy surfaces. 0=none; 1=normal; 2=stronger. Requires native ambient occlusion enabled. Automatic quality, Potato or Performance Mode can switch AO off.")
ET.indent(tree, space=" ")
tree.write(ui, encoding="utf-8", xml_declaration=True)
ET.parse(settings)
ET.parse(ui)
assert "kami_ao_darkening" in shader.read_text(encoding="utf-8")
assert "ao_darkening_uniform" in pipeline.read_text(encoding="utf-8")
print("KAMI native AO darkening slider now drives the real SSAO visibility shader; no extra pass.")
