#!/usr/bin/env python3
"""Performance-friendly native graphics controls for KAMI Viewer.

Run after add-native-bounce.py and apply-smart-performance.py.
Uses existing SSAO/HBAO, grading, shadows and volumetrics only.
Do not add custom legacy avatar-lighting shaders or controls.
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
# Retain the ordinary graphics settings, but do not inject legacy avatar
# fill/shading preferences or per-avatar shader uniforms.
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
 <text name="kami_performance_info" left="24" top="1190" width="512" height="85" wrap="true">Contact shading uses the existing SSAO/HBAO pipeline, which can be expensive on integrated GPUs. Keep volumetrics off and shadows reduced on weak PCs. Advanced graphics effects remain optional.</text>
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
# Reject accidental reintroduction of the removed legacy baked-avatar
# lighting path during viewer source integration.
for relative in (
    settings,
    panel,
    "indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl",
    "indra/newview/lldrawpoolavatar.cpp",
):
    text = (ROOT/relative).read_text(encoding="utf-8-sig")
    for old_key in ("KAMIAvatarFillStrength", "KAMIAvatarSoftShading",
                    "kami_avatar_fill", "kami_avatar_soft_shading",
                    "kami_legacy_fill", "kami_legacy_soft",
                    "kami_avatarlight_heading"):
        assert old_key not in text, f"Removed legacy avatar lighting remains in {relative}: {old_key}"

print("KAMI: contact shading, shadows and volumetrics integrated; custom legacy avatar lighting removed.")
