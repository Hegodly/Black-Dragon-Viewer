#!/usr/bin/env python3
"""KAMI low-cost native visual quality: real shader uniforms and live graphics UI.

Run after the baseline graphics, bounce and adaptive performance patches. This
uses existing viewer OpenGL shaders, never GShade/ReShade or fake menu options.
It does NOT claim physically correct, skin-only SSS or true ray-traced shadows.
"""
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]

def edit(path,before,after):
    p=R/path
    s=p.read_text(encoding="utf-8-sig")
    n=s.count(before)
    if n!=1:
        raise RuntimeError(f"{path}: expected one exact anchor ({n}): {before[:95]!r}")
    p.write_text(s.replace(before,after,1),encoding="utf-8")
    print("Updated",path)

settings="indra/newview/app_settings/settings_blackdragon.xml"
add='''  <key>KAMIAvatarSoftLighting</key>
  <map><key>Comment</key><string>Gentle camera-oriented normal smoothing for classic skinned avatar meshes. Affects clothing as well as faces; 0 is off.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.0</real></map>
  <key>KAMIAvatarRimTranslucency</key>
  <map><key>Comment</key><string>Experimental inexpensive avatar-facing color lift, not skin-only physically correct subsurface scattering; 0 is off.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.0</real></map>
  <key>KAMIColorTemperature</key>
  <map><key>Comment</key><string>Subtle native warm/cool white-balance shift after lighting; 0 is neutral.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.0</real></map>
  <key>KAMIColorVibrance</key>
  <map><key>Comment</key><string>Cheap low-saturation chroma enhancement in the existing gamma pass; 0 is off.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.0</real></map>
'''
edit(settings,'  <key>KAMISmartBackup</key>',add+'  <key>KAMISmartBackup</key>')

# 1) Low-cost avatar lighting (skinned avatars only). Never calls any new
# texture fetch or offscreen pass. Conservative baseline: disabled.
avatar="indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl"
edit(avatar,
     'uniform float minimum_alpha;\n',
     'uniform float minimum_alpha;\n'
     'uniform float kami_avatar_soft_light;\n'
     'uniform float kami_avatar_rim_translucency;\n')
edit(avatar,
     '    frag_data[0] = vec4(diff.rgb, 0.0);\n    frag_data[1] = vec4(0,0,0,0);\n    vec3 nvn = normalize(vary_normal);\n',
     '''    vec3 nvn = normalize(vary_normal);
    // Optional view-facing wrap of interpolated avatar normals. This reduces
    // hard profile lighting but does not identify skin vs clothing.
    float soft = clamp(kami_avatar_soft_light, 0.0, 0.35);
    float translucent = clamp(kami_avatar_rim_translucency, 0.0, 0.2);
    if (soft > 0.001)
    {
        vec3 towards_camera = normalize(-vary_position);
        nvn = normalize(mix(nvn, towards_camera, soft));
    }
    if (translucent > 0.001)
    {
        // Single cheap Fresnel-style tint. NOT physical SSS; no extra samples.
        float grazing = 1.0 - clamp(dot(nvn, normalize(-vary_position)), 0.0, 1.0);
        diff.rgb = min(vec3(1.0), diff.rgb * (1.0 + translucent * grazing * grazing));
    }
    frag_data[0] = vec4(diff.rgb, 0.0);
    frag_data[1] = vec4(0,0,0,0);
''')
draw="indra/newview/lldrawpoolavatar.cpp"
edit(draw,
     '    sVertexProgram->bind();\n    sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);\n    sDiffuseChannel = sVertexProgram->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);\n    gGL.getTexUnit(0)->activate();\n}\n\nvoid LLDrawPoolAvatar::endDeferredSkinned()',
     '''    sVertexProgram->bind();
    static LLStaticHashedString soft_avatar_uniform("kami_avatar_soft_light");
    static LLStaticHashedString translucent_uniform("kami_avatar_rim_translucency");
    static LLCachedControl<F32> soft_avatar(gSavedSettings, "KAMIAvatarSoftLighting", 0.f);
    static LLCachedControl<F32> rim_avatar(gSavedSettings, "KAMIAvatarRimTranslucency", 0.f);
    // These are native fragment-shader uniforms, not decorative preferences.
    sVertexProgram->uniform1f(soft_avatar_uniform, llclamp((F32)soft_avatar, 0.f, 0.35f));
    sVertexProgram->uniform1f(translucent_uniform, llclamp((F32)rim_avatar, 0.f, 0.2f));
    sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
    sDiffuseChannel = sVertexProgram->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
    gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::endDeferredSkinned()''')

# 2) Color controls integrated with the already existing gamma pass.
gamma="indra/newview/app_settings/shaders/class1/deferred/postDeferredGammaCorrect.glsl"
edit(gamma, 'uniform vec3 kombatant_fx; // saturation, contrast, vignette\n',
     'uniform vec3 kombatant_fx; // saturation, contrast, vignette\n'
     'uniform vec2 kami_color_balance; // temperature (-1..1), vibrance (0..0.5)\n')
edit(gamma,
     '    diff.rgb = (diff.rgb - 0.5) * kombatant_fx.y + 0.5;\n',
     '''    diff.rgb = (diff.rgb - 0.5) * kombatant_fx.y + 0.5;
    // Native white balance; neutral at 0, no extra texture fetch.
    float kami_warmth = clamp(kami_color_balance.x, -1.0, 1.0);
    diff.rgb *= vec3(1.0 + 0.06 * kami_warmth, 1.0, 1.0 - 0.06 * kami_warmth);
    // Lift muted colors more than highly saturated colors.
    float kami_vibrance = clamp(kami_color_balance.y, 0.0, 0.5);
    float kami_min = min(diff.r, min(diff.g, diff.b));
    float kami_max = max(diff.r, max(diff.g, diff.b));
    float kami_vib = kami_vibrance * (1.0 - clamp(kami_max - kami_min, 0.0, 1.0));
    float kami_luma = dot(diff.rgb, vec3(0.2126, 0.7152, 0.0722));
    diff.rgb = mix(vec3(kami_luma), diff.rgb, 1.0 + kami_vib);
''')

renderer="indra/newview/pipeline.cpp"
anchor='''        shader.uniform3f(fx_params, native_fx() ? llclamp(saturation(), 0.f, 2.f) : 1.f,
            native_fx() ? llclamp(contrast(), 0.5f, 1.5f) : 1.f,
            native_fx() ? llclamp(vignette(), 0.f, 1.f) : 0.f);
'''
edit(renderer,anchor,anchor+'''        static LLCachedControl<F32> kami_temperature(gSavedSettings, "KAMIColorTemperature", 0.f);
        static LLCachedControl<F32> kami_vibrance(gSavedSettings, "KAMIColorVibrance", 0.f);
        static LLStaticHashedString kami_balance("kami_color_balance");
        shader.uniform2f(kami_balance,
            native_fx() ? llclamp((F32)kami_temperature, -1.f, 1.f) : 0.f,
            native_fx() ? llclamp((F32)kami_vibrance, 0.f, 0.5f) : 0.f);
''')

# 3) Put controls in the scrollable Display panel so low-resolution laptop
# screens can reach every quality option. The separate Graphics Lab remains
# unchanged; a 1320px floating dialog would be unusable on smaller screens.
panel="indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
p=R/panel
s=p.read_text(encoding="utf-8-sig")
start=s.index('<layout_panel name="kombatant_display_quick"')
end=s.index('</layout_panel>', start)
area=s[start:end]
if 'height="2510" auto_resize="false"' not in area:
    raise RuntimeError("Smart Performance Display panel layout changed")
area=area.replace('height="2510" auto_resize="false"',
                  'height="3020" auto_resize="false"',1)
additional='''       <text name="kami_quality_heading" left="12" top="2530" width="465" height="24" font="SansSerifLarge">KAMI visual quality / low-cost effects</text>
       <slider name="kami_soft_avatar" label="Soft avatar lighting (all rigged surfaces)" control_name="KAMIAvatarSoftLighting" left="12" top="2564" width="465" height="20" label_width="245" min_val="0" max_val="0.35" increment="0.025" decimal_digits="3" />
       <slider name="kami_rim_scatter" label="Avatar rim translucency (approximate)" control_name="KAMIAvatarRimTranslucency" left="12" top="2596" width="465" height="20" label_width="245" min_val="0" max_val="0.2" increment="0.025" decimal_digits="3" />
       <slider name="kami_color_temp" label="Color temperature (neutral at 0)" control_name="KAMIColorTemperature" left="12" top="2628" width="465" height="20" label_width="245" min_val="-1" max_val="1" increment="0.05" decimal_digits="2" />
       <slider name="kami_vibrance" label="Color vibrance" control_name="KAMIColorVibrance" left="12" top="2660" width="465" height="20" label_width="245" min_val="0" max_val="0.5" increment="0.05" decimal_digits="2" />
       <text name="kami_contact_label" left="12" top="2700" width="465" height="22" font="SansSerifLarge">Contact shading, shadows and atmosphere</text>
       <check_box name="kami_ssao_enable" label="Enable contact ambient occlusion" control_name="RenderDeferredSSAO" left="12" top="2730" width="465" height="22" />
       <slider name="kami_ssao_scale" label="AO contact radius" control_name="RenderSSAOScale" left="12" top="2761" width="465" height="20" label_width="245" min_val="100" max_val="800" increment="25" decimal_digits="0" />
       <slider name="kami_shadow_scale" label="Shadow resolution (uses more GPU memory)" control_name="RenderShadowResolutionScale" left="12" top="2793" width="465" height="20" label_width="245" min_val="0.25" max_val="1" increment="0.05" decimal_digits="2" />
       <check_box name="kami_fog_enable" label="Volumetric lighting / fog rays" control_name="RenderVolumetricLighting" left="12" top="2825" width="465" height="22" />
       <combo_box name="kami_fog_samples" control_name="RenderVolumetricLightingResolution" left="12" top="2853" width="465" height="24">
        <combo_box.item label="Fog: Low (8 steps)" value="8" />
        <combo_box.item label="Fog: Balanced (16 steps)" value="16" />
        <combo_box.item label="Fog: High (32 steps)" value="32" />
       </combo_box>
       <text name="kami_quality_note" left="12" top="2890" width="465" height="98" wrap="true">Avatar soft light and rim translucency are lightweight approximations on classic skinned avatars, not face-only or physically accurate skin SSS; clothing may also be affected. Contact shading uses existing SSAO/HBAO, not ray-traced contact shadows. New shader effects default off; turn down fog and ambient occlusion on weaker GPUs.</text>
'''
area+=additional
s=s[:start]+area+s[end:]
if s.count('height="2514"')!=1:
    raise RuntimeError("Scrollable Display panel geometry changed")
s=s.replace('height="2514"','height="3024"',1)
p.write_text(s,encoding="utf-8")

# Save additional controls with existing Graphics Lab "Restore" behavior.
lab="indra/newview/kombatanteffects.h"
edit(lab,
     '            "RenderScreenSpaceReflections", "RenderShadowDetail"\n',
     '            "RenderScreenSpaceReflections", "RenderShadowDetail",\n'
     '            "KAMIAvatarSoftLighting", "KAMIAvatarRimTranslucency",\n'
     '            "KAMIColorTemperature", "KAMIColorVibrance",\n'
     '            "RenderSSAOScale", "RenderShadowResolutionScale",\n'
     '            "RenderVolumetricLightingResolution", "RenderDeferredHBAO"\n')

# Native adaptive modes shed higher cost effects, and preserve user choices.
smart="indra/newview/kami_smart_engine.h"
# Note: this header gets copied by earlier smart-performance script.
edit(smart,
     '        "KombatantNativeFX"\n',
     '        "KombatantNativeFX", "KAMIAvatarSoftLighting",\n'
     '        "KAMIAvatarRimTranslucency", "RenderDeferredHBAO",\n'
     '        "RenderVolumetricLightingResolution"\n')
edit(smart,
     '            set("RenderVolumetricLighting", false);\n',
     '            set("RenderVolumetricLighting", false);\n'
     '            set("KAMIAvatarRimTranslucency", 0.f);\n'
     '            atMostU32("RenderVolumetricLightingResolution", 8);\n')
edit(smart,
     '            set("RenderDeferredSSAO", false);\n',
     '            set("RenderDeferredSSAO", false);\n'
     '            set("RenderDeferredHBAO", false);\n'
     '            set("KAMIAvatarSoftLighting", 0.f);\n')

# Ensure settings, shader integration, and UI XML are genuinely updated.
ET.parse(R/settings)
ET.parse(R/panel)
assert 'kami_avatar_soft_light' in (R/avatar).read_text(encoding="utf-8")
assert 'uniform2f(kami_balance' in (R/renderer).read_text(encoding="utf-8")
assert 'KAMIColorTemperature' in (R/settings).read_text(encoding="utf-8")
print("KAMI native performance-friendly quality features integrated; new shader costs default off.")
