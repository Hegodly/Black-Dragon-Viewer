#!/usr/bin/env python3
"""Performance-friendly native graphics controls for KAMI Viewer.

Run after add-native-bounce.py and apply-smart-performance.py.
Uses genuine built-in SSAO/HBAO, grading, shadows and volumetrics. Adds a
single cheap lighting/normal approximation for legacy baked avatars, NOT
physically correct skin subsurface scattering and NOT rigged mesh heads.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]

def edit(path, before, after, count=1):
    p = ROOT/path
    s = p.read_text(encoding="utf-8-sig")
    hits=s.count(before)
    if hits!=count:
        raise RuntimeError(f"{path}: expected {count} occurrences for {before[:75]!r}, found {hits}")
    p.write_text(s.replace(before,after,count),encoding="utf-8")
    print("Updated",path)

settings="indra/newview/app_settings/settings_blackdragon.xml"
settings_block='''  <key>KAMIAvatarFillStrength</key>
  <map><key>Comment</key><string>Cheap per-fragment frontal fill on the classic baked avatar shader. Not PBR SSS or mesh-head lighting. Zero disables.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.0</real></map>
  <key>KAMIAvatarSoftShading</key>
  <map><key>Comment</key><string>Subtle view-normal smoothing on classic baked avatars only, not full subsurface scattering. Zero disables.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.0</real></map>
'''
edit(settings,'  <key>KAMIAutoTune</key>',settings_block+'  <key>KAMIAutoTune</key>')

avatar="indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl"
edit(avatar,'uniform float minimum_alpha;\n',
    '''uniform float minimum_alpha;
// Shader effects are gated when their uniform is zero.
// Legacy baked avatar surface only; not an all-material skin detector.
uniform float kami_avatar_fill;
uniform float kami_avatar_soft_shading;
''')
edit(avatar,
'''    frag_data[0] = vec4(diff.rgb, 0.0);
    frag_data[1] = vec4(0,0,0,0);
    vec3 nvn = normalize(vary_normal);
    frag_data[2] = encodeNormal(nvn.xyz, 0, GBUFFER_FLAG_HAS_ATMOS);''',
'''    vec3 nvn = normalize(vary_normal);
    vec3 albedo = diff.rgb;
    if (kami_avatar_fill > 0.001)
    {
        // Maximum 6.25 percent frontal diffuse increase.
        float face = max(dot(nvn, normalize(-vary_position)), 0.0);
        albedo = min(diff.rgb * (1.0 + 0.25 * clamp(kami_avatar_fill, 0.0, 0.25) * face), vec3(1.0));
    }
    if (kami_avatar_soft_shading > 0.001)
    {
        // Cheap legacy avatar lighting approximation; no secondary passes.
        nvn = normalize(mix(nvn, normalize(-vary_position),
                       0.12 * clamp(kami_avatar_soft_shading, 0.0, 0.5)));
    }
    frag_data[0] = vec4(albedo, 0.0);
    frag_data[1] = vec4(0,0,0,0);
    frag_data[2] = encodeNormal(nvn.xyz, 0, GBUFFER_FLAG_HAS_ATMOS);''')

drawing="indra/newview/lldrawpoolavatar.cpp"
edit(drawing,
'''    sVertexProgram->bind();
    sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
    sDiffuseChannel = sVertexProgram->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
    gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::endDeferredSkinned()''',
'''    sVertexProgram->bind();
    sVertexProgram->setMinimumAlpha(LLDrawPoolAvatar::sMinimumAlpha);
    // Legacy skinned avatar light fill: tiny GPU-only uniform change.
    // Disable this in Potato/Smart Perf stage to preserve weak GPUs.
    static LLStaticHashedString kami_fill("kami_avatar_fill");
    static LLStaticHashedString kami_soft("kami_avatar_soft_shading");
    const bool cheap_mode = gSavedSettings.getBOOL("KAMIPotatoMode");
    sVertexProgram->uniform1f(kami_fill, cheap_mode ? 0.f : llclamp(gSavedSettings.getF32("KAMIAvatarFillStrength"), 0.f, 0.25f));
    sVertexProgram->uniform1f(kami_soft, cheap_mode ? 0.f : llclamp(gSavedSettings.getF32("KAMIAvatarSoftShading"), 0.f, 0.5f));
    sDiffuseChannel = sVertexProgram->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
    gGL.getTexUnit(0)->activate();
}

void LLDrawPoolAvatar::endDeferredSkinned()''')

# Keep the graphics lab window usable on 768p screens; put extra options
# in the existing vertically scrollable Display panel instead.
panel="indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
p=ROOT/panel
s=p.read_text(encoding="utf-8-sig")
a=s.index('<layout_panel name="kombatant_display_quick"')
b=s.index('</layout_panel>',a)
area=s[a:b]
assert 'height="2670" auto_resize="false"' in area, "Smart Display panel layout changed"
extras=''' <text name="kami_budget_heading" left="24" top="852" width="512" height="24" font="SansSerifLarge">Performance-friendly graphics</text>
 <check_box name="kami_grade_enable" label="Native color grading" control_name="KombatantNativeFX" left="24" top="880" width="512" height="22" />
 <slider name="kami_color_sat" label="Color saturation" control_name="KombatantSaturation" left="24" top="913" width="512" height="20" label_width="195" min_val="0.7" max_val="1.3" increment="0.025" decimal_digits="2" />
 <slider name="kami_color_contrast" label="Filmic contrast" control_name="KombatantContrast" left="24" top="945" width="512" height="20" label_width="195" min_val="0.75" max_val="1.25" increment="0.025" decimal_digits="2" />
 <check_box name="kami_contact_ssao" label="Contact shading (screen-space ambient occlusion)" control_name="RenderDeferredSSAO" left="24" top="985" width="512" height="22" />
 <slider name="kami_contact_intensity" label="Contact shading strength" control_name="RenderSSAOFactor" left="24" top="1013" width="512" height="20" label_width="195" min_val="0.025" max_val="0.175" increment="0.005" decimal_digits="3" />
 <text name="kami_shadowbudget" left="24" top="1056" width="512" height="24" font="SansSerifLarge">Shadows and atmosphere (optional)</text>
 <slider name="kami_shadow_scale" label="Shadow resolution scaling" control_name="RenderShadowResolutionScale" left="24" top="1086" width="512" height="20" label_width="195" min_val="0.25" max_val="1.0" increment="0.05" decimal_digits="2" />
 <check_box name="kami_fog_enable" label="Volumetric lighting / fog (GPU intensive)" control_name="RenderVolumetricLighting" left="24" top="1120" width="512" height="22" />
 <text name="kami_fog_label" left="24" top="1150" width="200" height="20">Volumetric sampling quality</text>
 <combo_box name="kami_fog_quality" control_name="RenderVolumetricLightingResolution" left="238" top="1146" width="295" height="24">
  <combo_box.item label="Low (8)" value="8" />
  <combo_box.item label="Balanced (16)" value="16" />
  <combo_box.item label="High (32)" value="32" />
 </combo_box>
 <text name="kami_avatarlight_heading" left="24" top="1190" width="512" height="24" font="SansSerifLarge">Legacy avatar lighting (experimental)</text>
 <slider name="kami_legacy_fill" label="Frontal avatar fill" control_name="KAMIAvatarFillStrength" left="24" top="1220" width="512" height="20" label_width="195" min_val="0" max_val="0.25" increment="0.025" decimal_digits="3" />
 <slider name="kami_legacy_soft" label="Soft avatar shading" control_name="KAMIAvatarSoftShading" left="24" top="1250" width="512" height="20" label_width="195" min_val="0" max_val="0.5" increment="0.025" decimal_digits="3" />
 <text name="kami_avatarlight_warning" left="24" top="1283" width="512" height="110" wrap="true">Avatar fill and soft shading are cheap, optional approximations for classic baked avatars. They do not implement physically based skin subsurface scattering or necessarily affect fitted mesh heads. Contact shading uses the real SSAO/HBAO pipeline, which can be expensive on integrated GPUs. Keep volumetrics off and shadows reduced on weak PCs. All advanced features remain optional.</text>
'''
import re
extras = re.sub(r'\btop="(\d+)"', lambda m: 'top="'+str(int(m.group(1))+1840)+'"', extras)
extras = extras.replace('left="24"','left="12"').replace('width="512"','width="465"')
extras = extras.replace('left="238"','left="215"').replace('width="295"','width="262"')
area=area.replace('height="2670" auto_resize="false"', 'height="3300" auto_resize="false"', 1)
s=s[:a]+area+extras+s[b:]
assert s.count('height="2674"')==1
s=s.replace('height="2674"','height="3304"',1)
p.write_text(s,encoding="utf-8")
for file in [settings,panel]:
    ET.parse(ROOT/file)
for setting in ("RenderSSAOFactor","RenderShadowResolutionScale","RenderVolumetricLightingResolution"):
    assert setting in (ROOT/panel).read_text(encoding="utf-8")
assert "kami_avatar_fill" in (ROOT/avatar).read_text(encoding="utf-8")
assert "uniform1f(kami_fill" in (ROOT/drawing).read_text(encoding="utf-8")
print("KAMI: native color grading, contact shading, shadows, volumetrics and legacy avatar fill integrated.")
