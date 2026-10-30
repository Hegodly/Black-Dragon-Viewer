#!/usr/bin/env python3
"""Expose and budget genuine existing renderer features, not fake COD options.

Shadow filtering: native blur sample count / radius, not ray tracing.
Reflections: existing SSR and reflection probes, not hardware RT.
Screen-space lighting: existing SSAO/HBAO + experimental near-field GI.
Hardware optimization: existing adaptive FPS/VRAM engine, not GPU benchmarks.
Shader cache: viewer's existing OpenGL program-binary cache; no fake preloader.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
def edit(path, old, new):
    p=R/path
    s=p.read_text(encoding="utf-8-sig")
    if s.count(old)!=1:
        raise RuntimeError(f"{path}: expected exactly one anchor, found {s.count(old)}: {old[:80]!r}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")
    print("Updated",path)

# Extend the scrollable Display panel after the prior quality script.
panel="indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
p=R/panel
s=p.read_text(encoding="utf-8-sig")
start=s.index('<layout_panel name="kombatant_display_quick"')
end=s.index('</layout_panel>',start)
area=s[start:end]
if 'height="3300" auto_resize="false"' not in area:
    raise RuntimeError("KAMI Display panel changed; refusing to create inaccessible UI")
area=area.replace('height="3300" auto_resize="false"','height="3910" auto_resize="false"',1)
# These settings already exist in the Black Dragon / SL render pipeline.
# Defaults are preserved, especially on weak GPUs.
extra='''       <text name="kami_mw_heading" left="12" top="3330" width="465" height="24" font="SansSerifLarge">Advanced quality / competitive performance</text>
       <text name="kami_mw_shadow_heading" left="12" top="3364" width="465" height="24" font="SansSerifLarge">Soft shadows (native filtering)</text>
       <check_box name="kami_mw_shadow_blur" label="Enable deferred shadow softening" control_name="RenderDeferredBlurLight" left="12" top="3396" width="465" height="22" />
       <slider name="kami_mw_shadow_soft" label="Shadow softness" control_name="RenderShadowBlurSize" left="12" top="3426" width="465" height="20" label_width="210" min_val="0.25" max_val="2.5" increment="0.05" decimal_digits="2" />
       <combo_box name="kami_mw_shadow_samples" control_name="RenderShadowBlurSamples" left="12" top="3457" width="465" height="24">
        <combo_box.item label="Shadow filtering: Fast (2 samples)" value="2" />
        <combo_box.item label="Shadow filtering: Balanced (4 samples)" value="4" />
        <combo_box.item label="Shadow filtering: High (6 samples)" value="6" />
        <combo_box.item label="Shadow filtering: Ultra (8 samples)" value="8" />
       </combo_box>
       <text name="kami_mw_reflect_heading" left="12" top="3494" width="465" height="24" font="SansSerifLarge">Reflections and screen-space lighting</text>
       <check_box name="kami_mw_ssr" label="Screen-space reflections (optional GPU cost)" control_name="RenderScreenSpaceReflections" left="12" top="3524" width="465" height="22" />
       <combo_box name="kami_mw_probes" control_name="RenderReflectionProbeDetail" left="12" top="3553" width="465" height="24">
        <combo_box.item label="Reflection probes: Disabled" value="-1" />
        <combo_box.item label="Reflection probes: Static" value="0" />
        <combo_box.item label="Reflection probes: Static + Dynamic" value="1" />
        <combo_box.item label="Reflection probes: Realtime (expensive)" value="2" />
       </combo_box>
       <check_box name="kami_mw_hbao" label="Higher-quality ambient occlusion (HBAO)" control_name="RenderDeferredHBAO" left="12" top="3585" width="465" height="22" />
       <check_box name="kami_mw_gi" label="Experimental PBR near-field color bounce" control_name="KAMINearFieldGI" left="12" top="3615" width="465" height="22" />
       <text name="kami_mw_perf_heading" left="12" top="3651" width="465" height="24" font="SansSerifLarge">Shader caching and automatic optimization</text>
       <check_box name="kami_mw_shader_cache" label="Enable existing GPU shader binary cache (experimental)" control_name="RenderShaderCacheEnabled" left="12" top="3683" width="465" height="22" />
       <check_box name="kami_mw_auto" label="Automatically adjust quality toward target FPS" control_name="KAMIAutoTune" left="12" top="3713" width="465" height="22" />
       <check_box name="kami_mw_vram" label="Automatic texture / VRAM budget" control_name="KAMISmartTextures" left="12" top="3743" width="465" height="22" />
       <text name="kami_mw_explain" left="12" top="3780" width="465" height="108" wrap="true">These controls connect to the actual viewer renderer. Shader cache stores supported compiled GPU programs; it is not preloading. Adaptive quality responds to measured FPS, not a one-click GPU benchmark. Reflections and GI are screen-space approximations, not hardware ray tracing. Increase shadow samples gradually; disable HBAO, SSR and GI on integrated graphics.</text>
'''
area+=extra
s=s[:start]+area+s[end:]
if s.count('height="3304"')!=1:
    raise RuntimeError("Unexpected scroll height")
s=s.replace('height="3304"','height="3914"',1)
p.write_text(s,encoding="utf-8")

# Preserve user-selected expensive settings when Smart Performance adapts.
# Lower shadow filter cost before touching high-priority avatar detail.
header="kami/performance/kami_smart_engine.h"
edit(header,
     '        "RenderFSAAType", "RenderShadowDetail", "RenderShadowResolutionScale",\n',
     '        "RenderFSAAType", "RenderShadowDetail", "RenderShadowResolutionScale",\n'
     '        "RenderShadowBlurSamples", "RenderShadowBlurSize",\n')
edit(header,
     '                set("RenderScreenSpaceReflections", false);\n',
     '                set("RenderScreenSpaceReflections", false);\n'
     '                if (shadow) atMostU32("RenderShadowBlurSamples", 4);\n')
edit(header,
     '                atMostF32("RenderShadowResolutionScale", 0.75f);\n',
     '                atMostF32("RenderShadowResolutionScale", 0.75f);\n'
     '                atMostU32("RenderShadowBlurSamples", 2);\n')
# Prior smart patch copies the header before this step; sync it into viewer.
shutil.copy2(R/header, R/"indra/newview/kami_smart_engine.h")

ET.parse(R/panel)
for key in ("RenderShadowBlurSamples","RenderShadowBlurSize","RenderDeferredBlurLight",
            "RenderScreenSpaceReflections","RenderReflectionProbeDetail",
            "RenderDeferredHBAO","KAMINearFieldGI","RenderShaderCacheEnabled",
            "KAMIAutoTune","KAMISmartTextures"):
    assert ('control_name="'+key+'"') in (R/panel).read_text(encoding="utf-8"), key
    assert ('<key>'+key+'</key>') in (
        (R/"indra/newview/app_settings/settings.xml").read_text(encoding="utf-8")+
        (R/"indra/newview/app_settings/settings_blackdragon.xml").read_text(encoding="utf-8")
    ), "Missing live renderer setting: "+key
print("KAMI: native shadow filter, SSR/probes, AO/GI, shader binary cache, adaptive GPU/VRAM controls validated.")
