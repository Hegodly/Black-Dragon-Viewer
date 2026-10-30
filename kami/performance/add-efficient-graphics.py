#!/usr/bin/env python3
"""KAMI optional low-cost skin light, tone highlights, shadows/fog UI.
Apply after the native, bounce and smart performance build steps.
No per-pixel texture fetches or shader-injection DLLs.
"""
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]

def edit(path,old,new):
    p=R/path
    s=p.read_text(encoding="utf-8-sig")
    if s.count(old)!=1: raise RuntimeError(f"{path}: expected 1 match of {old[:80]!r}, got {s.count(old)}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")

settings="indra/newview/app_settings/settings_blackdragon.xml"
defs='''  <key>KAMIAvatarSoftLight</key>
  <map><key>Comment</key><string>Lightweight base-avatar face and skin fill. Only affects native system avatar GBuffer, not separately attached mesh bodies or faces.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.25</real></map>
  <key>KAMISkinLightWrap</key>
  <map><key>Comment</key><string>Optional soft warm silhouette effect on native system avatar mesh. Approximation, not true subsurface scattering.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0</real></map>
  <key>KAMIFilmicHighlights</key>
  <map><key>Comment</key><string>Subtle native SDR highlight compression in final gamma pass; neutral at zero.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0</real></map>
'''
edit(settings,'  <key>KAMINearFieldGI</key>',defs+'  <key>KAMINearFieldGI</key>')

# GBuffer for native avatar base mesh: no new passes or sampling.
shader="indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl"
edit(shader,'uniform float minimum_alpha;','uniform float minimum_alpha;\nuniform vec2 kami_avatar_light;')
edit(shader, '    vec4 diff = texture(diffuseMap, vary_texcoord0.xy);',
'''    vec4 diff = texture(diffuseMap, vary_texcoord0.xy);
    if (kami_avatar_light.x > 0.001 || kami_avatar_light.y > 0.001)
    {
        vec3 n = normalize(vary_normal);
        float facing = clamp(dot(n, normalize(-vary_position)), 0.0, 1.0);
        float fill = (0.008 + 0.018 * facing) * clamp(kami_avatar_light.x, 0.0, 1.0);
        float rim = (1.0 - facing) * (1.0 - facing) * clamp(kami_avatar_light.y, 0.0, 1.0);
        diff.rgb = clamp(diff.rgb + vec3(fill) +
                         rim * vec3(0.017, 0.009, 0.003), 0.0, 1.0);
    }''')
render="indra/newview/lldrawpoolavatar.cpp"
anchor='''    sVertexProgram->bind();
    sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
    sDiffuseChannel = sVertexProgram->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);'''
replace='''    sVertexProgram->bind();
    sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
    static LLCachedControl<F32> kami_fill(gSavedSettings, "KAMIAvatarSoftLight", 0.25f);
    static LLCachedControl<F32> kami_wrap(gSavedSettings, "KAMISkinLightWrap", 0.0f);
    static LLStaticHashedString kami_avatar_uniform("kami_avatar_light");
    const bool low_quality = gSavedSettings.getBOOL("KAMIPotatoMode");
    sVertexProgram->uniform2f(kami_avatar_uniform,
        low_quality ? 0.f : llclamp((F32)kami_fill, 0.f, 1.f),
        low_quality ? 0.f : llclamp((F32)kami_wrap, 0.f, 1.f));
    sDiffuseChannel = sVertexProgram->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);'''
# On this shader path the source GBuffer-only pass is unique to deferred skinned.
edit(render,anchor,replace)

# One short arithmetic adjustment in the preexisting gamma output pass.
gamma="indra/newview/app_settings/shaders/class1/deferred/postDeferredGammaCorrect.glsl"
edit(gamma,'uniform vec2 kombatant_vignette_shape;',
           'uniform vec2 kombatant_vignette_shape;\nuniform float kami_filmic_highlights;')
edit(gamma,'    frag_color = vec4(clamp(diff.rgb, 0.0, 1.0), diff.a);',
'''    // Apply only a subtle rolloff: leave contrast/midtones almost unchanged.
    if (kami_filmic_highlights > 0.001)
    {
        vec3 h = max(diff.rgb - vec3(0.7), vec3(0.0));
        diff.rgb -= h * h * (0.6 * clamp(kami_filmic_highlights, 0.0, 1.0));
    }
    frag_color = vec4(clamp(diff.rgb, 0.0, 1.0), diff.a);''')
pipeline="indra/newview/pipeline.cpp"
edit(pipeline,
'''        shader.uniform2f(vignette_params, llclamp(radius(), 0.f, 1.f), llclamp(softness(), 0.05f, 2.f));''',
'''        shader.uniform2f(vignette_params, llclamp(radius(), 0.f, 1.f), llclamp(softness(), 0.05f, 2.f));
        static LLCachedControl<F32> kami_filmic(gSavedSettings, "KAMIFilmicHighlights", 0.f);
        static LLStaticHashedString kami_filmic_uniform("kami_filmic_highlights");
        shader.uniform1f(kami_filmic_uniform, native_fx() ? llclamp((F32)kami_filmic, 0.f, 1.f) : 0.f);''')

# Native Graphics Lab already contains existing HBAO/probe/shadow controls.
# Extend the existing floater with useful, non-duplicative sliders.
ui="indra/newview/skins/default/xui/en/floater_kombatant_effects.xml"
p=R/ui;s=p.read_text(encoding="utf-8-sig")
assert 'height="860"' in s
s=s.replace('height="860"','height="1160"',1)
xui=''' <text name="kami_quality_heading" left="24" top="864" width="512" height="24" font="SansSerifLarge">Efficient character and environment lighting</text>
 <slider name="kami_face_fill" label="Base-avatar soft light" control_name="KAMIAvatarSoftLight" left="24" top="894" width="512" height="20" label_width="205" min_val="0" max_val="1" increment="0.05" decimal_digits="2" />
 <slider name="kami_skin_wrap" label="Base-avatar warm rim (approx.)" control_name="KAMISkinLightWrap" left="24" top="926" width="512" height="20" label_width="205" min_val="0" max_val="1" increment="0.05" decimal_digits="2" />
 <slider name="kami_filmic" label="Filmic highlight rolloff" control_name="KAMIFilmicHighlights" left="24" top="958" width="512" height="20" label_width="205" min_val="0" max_val="1" increment="0.05" decimal_digits="2" />
 <slider name="kami_contact_radius" label="SSAO contact radius" control_name="RenderSSAOScale" left="24" top="990" width="512" height="20" label_width="205" min_val="50" max_val="800" increment="25" decimal_digits="0" />
 <combo_box name="kami_fog_quality" control_name="RenderVolumetricLightingResolution" left="24" top="1026" width="512" height="24">
  <combo_box.item label="Volumetric samples: Performance (8)" value="8" />
  <combo_box.item label="Volumetric samples: Balanced (16)" value="16" />
  <combo_box.item label="Volumetric samples: Quality (32)" value="32" />
  <combo_box.item label="Volumetric samples: Cinematic (64)" value="64" />
 </combo_box>
 <text name="kami_quality_note" left="24" top="1066" width="512" height="82" wrap="true">System-avatar fill and rim are inexpensive approximations, not physically accurate skin scattering and not applied to separate mesh heads/bodies. SSAO radius requires ambient occlusion. Fog samples matter only when volumetric lighting is enabled. Expensive effects stay optional.</text>
'''
assert s.count("</floater>")==1
s=s.replace("</floater>",xui+"</floater>")
p.write_text(s,encoding="utf-8")
# Add optional features to smart quality backup and turn down costs at stage 1.
smart="kami/performance/kami_smart_engine.h"
edit(smart,'        "KombatantNativeFX"\n','        "KombatantNativeFX", "KAMIAvatarSoftLight", "KAMISkinLightWrap",\n        "KAMIFilmicHighlights", "RenderVolumetricLightingResolution"\n')
edit(smart,'            set("RenderVolumetricLighting", false);',
'''            set("RenderVolumetricLighting", false);
            set("KAMISkinLightWrap", 0.f);
            set("KAMIFilmicHighlights", 0.f);''')
edit(smart,'            set("KombatantNativeFX", false);',
'''            set("KombatantNativeFX", false);
            set("KAMIAvatarSoftLight", 0.f);''')
ET.parse(R/settings)
ET.parse(R/ui)
assert 'kami_avatar_light' in (R/shader).read_text()
assert 'kami_filmic_highlights' in (R/gamma).read_text()
print("KAMI optional avatar soft-light, warm rim, filmic highlights, contact-shading radius and fog sample quality integrated.")
