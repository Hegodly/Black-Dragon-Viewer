#!/usr/bin/env python3
"""Fail early with specific integration errors before the heavyweight Windows build."""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def read(path):return (R/path).read_text(encoding="utf-8-sig")
def verify(name,test):
    if not test:
        raise RuntimeError(f"KAMI next-build integration failed: {name}")
    print(f"PASS {name}")

for name in (
    "indra/newview/app_settings/settings_blackdragon.xml",
    "indra/newview/skins/default/xui/en/floater_preferences.xml",
    "indra/newview/skins/default/textures/textures.xml",
    "indra/newview/skins/default/colors.xml",
    "indra/newview/skins/default/xui/en/menu_viewer.xml",
    "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml",
    "indra/newview/skins/default/xui/en/panel_login.xml",
):
    ET.parse(R/name)
    print("PASS XML "+name)
verify("auto-tune target preserved", "KAMITargetFPS" in read("indra/newview/kami_smart_engine.h"))
verify("no mandatory FPS cap in Potato Mode", 'set("FramePerSecondLimit"' not in read("indra/newview/kami_smart_engine.h"))
verify("no cropping-prone 75% or 60% scale in Smart optimization", all(x not in read("indra/newview/kami_smart_engine.h") for x in ('atMostF32("RenderResolutionMultiplier", 0.75f)', 'atMostF32("RenderResolutionMultiplier", 0.6f)')))
verify("4K world render width never erased by Auto Tune", 'set("KombatantRenderWidth", 0)' not in read("indra/newview/kami_smart_engine.h"))
verify("4K stays native in Performance Mode", 'set("RenderResolutionMultiplier", 1.0)' in read("indra/newview/kami_performance_mode.h"))
verify("Performance Mode does not bypass explicit 4K/8K target", 
       'gSavedSettings.getU32("KombatantRenderWidth") == 0) return;' in read("indra/newview/pipeline.cpp") and
       'if (main_target && gSavedSettings.getBOOL("KAMIPerformanceMode")) return;' not in read("indra/newview/pipeline.cpp"))
verify("4K resolution allocator still receives manual target",
       'kombatantScreenSize(resX, resY, mRT == &mMainRT);' in read("indra/newview/pipeline.cpp"))
verify("no runtime FPS caps by default", '<key>KAMIBackgroundFPS</key>' in read("indra/newview/app_settings/settings_blackdragon.xml") and '("KAMIBackgroundFPS","S32","0"' in read("kami/performance/apply-smart-performance.py"))
verify("Performance preset offloads distant avatars", 'set("RenderAvatarMaxNonImpostors", 4)' in read("indra/newview/kami_performance_mode.h"))
verify("Performance preset reduces particle budget", 'set("RenderMaxPartCount", 512)' in read("indra/newview/kami_performance_mode.h"))
verify("Performance preset respects 4K width", 'set("KombatantRenderWidth", 0)' not in read("indra/newview/kami_performance_mode.h"))
verify("Performance preset avoids known FXAA problem", 'set("RenderFSAAType", 0)' in read("indra/newview/kami_performance_mode.h"))
verify("Performance mode no active FPS cap", 'set("FramePerSecondLimit"' not in read("indra/newview/kami_performance_mode.h"))
verify("Performance mode can suspend conflicting Smart modes", "suspendSmartModes()" in read("indra/newview/kami_performance_mode.h"))
verify("measured baseline comparison", 'KAMIPerformanceBaselineFPS' in read("indra/newview/kami_smart_engine.h"))
verify("no original KAMI grading uniforms", "kombatant_fx" not in read("indra/newview/app_settings/shaders/class1/deferred/postDeferredGammaCorrect.glsl"))
verify("live SSR buffer reallocation", "if (ssr_was_enabled != RenderScreenSpaceReflections)" in read("indra/newview/pipeline.cpp"))
verify("experimental PBR bounce shader", "kamiNearFieldBounce" in read("indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl"))
verify("AO darkening shader", "kami_ao_darkening" in read("indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl"))
verify("AO darkening bound to native pipeline", 'LLStaticHashedString ao_darkening_uniform("kami_ao_darkening")' in read("indra/newview/pipeline.cpp"))
verify("optical flow frame status", "actual_fg_status" in read("indra/newview/llviewerdisplay.cpp"))
verify("optical flow timing corrected", "std::chrono::duration<double>(now-last).count():0." in read("indra/newview/kami_framegen.h"))
verify("old FX injector no longer called", "KombatantFX::" not in read("indra/llwindow/llwindowwin32.cpp"))
verify("old FX injector header absent", not (R/"indra/llwindow/kombatantfxwin32.h").exists())

root=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
found=[n for n in root.iter() if n.get("name")=="kd_ao_strength"]
verify("AO darkening UI actually controls native AO",len(found)==1 and found[0].get("control_name")=="KAMIAODarkening")
found=[n for n in root.iter() if n.get("name")=="kami_fg_status"]
verify("optical flow status visible in UI",len(found)==1)

root=ET.parse(R/"indra/newview/skins/default/xui/en/panel_login.xml").getroot()
verify("agreed KAMI login background present", any(n.get("name")=="kami_fire_garden" and n.get("image_name")=="KAMI_FireGarden" for n in root.iter()))
verify("legacy Black Dragon login browser removed",not list(root.iter("web_browser")))
verify("approved Fire Garden artwork used in Preferences", 'image_name="KAMI_FireGardenPrefs"' in read("indra/newview/skins/default/xui/en/floater_preferences.xml"))
verify("Fire Garden button textures included", 'windows/kami_fg_button.png' in read("indra/newview/skins/default/textures/textures.xml"))
verify("Fire Garden native UI images staged", all((R / ("indra/newview/skins/default/textures/windows/"+file)).is_file() for file in ("kami_fg_window.png","kami_fg_panel.png","kami_fg_button.png","kami_fg_preferences.png")))
verify("obsolete Native Graphics Lab and Studio menu entries removed", not any(n.get("name") in {"Kombatant Effects","Kombatant Studio"} for n in ET.parse(R/"indra/newview/skins/default/xui/en/menu_viewer.xml").getroot().iter()))
verify("obsolete Native Graphics Lab and Studio XUI removed", not (R/"indra/newview/skins/default/xui/en/floater_kombatant_effects.xml").exists() and not (R/"indra/newview/skins/default/xui/en/floater_kombatant_studio.xml").exists())
verify("Fire Garden core graphics base kit present", (R/"indra/newview/skins/default/KAMI-BASE-GRAPHICS-KIT.txt").is_file())
verify("KAMI core graphics heading visible", "KAMI CORE GRAPHICS" in read("indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"))
verify("all remaining source-video toggles retained", "40 original interactive" in read("indra/newview/skins/default/KAMI-BASE-GRAPHICS-KIT.txt"))
verify("obsolete Native Graphics Lab and Studio C++ registration removed", '"kombatant_effects"' not in read("indra/newview/llviewerfloaterreg.cpp") and '"kombatant_studio"' not in read("indra/newview/llviewerfloaterreg.cpp"))
verify("all five quality presets use actual render settings", 'KAMIQuality::apply(chosen)' in read("indra/newview/llfloaterpreference.cpp") and 'RenderAvatarMaxNonImpostors' in read("indra/newview/kami_quality_presets.h"))
verify("quality presets keep selected 4K and uncapped FPS", all(x not in read("indra/newview/kami_quality_presets.h") for x in ('set("FramePerSecondLimit",', 'set("KombatantRenderWidth",', 'set("RenderResolutionMultiplier",')))
verify("low to cinematic preset choice saved", '<key>KAMIQualityPreset</key>' in read("indra/newview/app_settings/settings_blackdragon.xml"))
verify("actually allocated world buffer readout connected", 'last_world_resolution_status_time' in read("indra/newview/llviewerdisplay.cpp") and '<key>KAMILiveResolutionStatus</key>' in read("indra/newview/app_settings/settings_blackdragon.xml"))
verify("4K manual choice remains", 'value="3840"' in read("indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"))
verify("8K manual choice remains", 'value="7680"' in read("indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"))
verify("nonworking tonal sliders and reset buttons removed from Display",
       not any(n.get("control_name") in {"RenderExposure", "RenderCASSharpness", "RenderGlowStrength"}
               for n in ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot().iter()))
verify("upstream exposure, sharpening and glow renderer code retained",
       all(token in read("indra/newview/pipeline.cpp")
           for token in ("generateExposure(", "applyCAS(", "generateGlow(", "combineGlow(")))
verify("broken DoF / Bloom / Motion Blur controls removed from Display",
       not any(n.get("control_name") in {
           "RenderDepthOfField", "RenderDepthOfFieldAlphas",
           "RenderDepthOfFieldHighQuality", "RenderMotionBlur",
           "RenderMotionBlurStrength", "RenderGlowHDR",
           "RenderGlowResolutionPow", "RenderGlowIterations",
           "RenderGlowMinLuminance", "RenderGlowWidth"}
           for n in ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot().iter()))
verify("retired effect subheadings removed",
       not any((n.text or "").strip() in {
           "Depth of Field", "Motion Blur", "Post Processing Glow", "Glow Quality:"}
           for n in ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot().iter("text")))
verify("working render scale still available in the viewer",
       "RenderResolutionMultiplier" in read("indra/newview/pipeline.cpp") and
       "KombatantRenderWidth" in read("indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"))
verify("temporal SDKs safely quarantined until motion-vector repair",
       'return false; // KAMI QA quarantine' in read("indra/newview/kombatantrtxstate.h") and
       'Temporal upscaling unavailable: unstable history' in read("indra/newview/kombatantrtx.h"))
verify("hardware ray AO stopped until avatar artifacts are fixed",
       'Hardware Ray AO unavailable: face artifacts' in read("indra/newview/kombatantrtx.h"))
ui_quarantine = ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
retired_experiments = {"kd_upscaler", "kd_upscale_quality", "kd_ray_ao", "kd_ray_radius", "kd_ray_strength"}
experiment_widgets = [n for n in ui_quarantine.iter() if n.get("name") in retired_experiments]
verify("unsupported experimental SDK controls are explicitly disabled",
       len(experiment_widgets) == len(retired_experiments) and
       {n.get("name") for n in experiment_widgets} == retired_experiments and
       all(n.get("enabled") == "false" for n in experiment_widgets))
verify("user-tested Render Scale and contact shading remain",
       "RenderResolutionMultiplier" in read("indra/newview/pipeline.cpp") and
       any(n.get("control_name") == "KombatantRenderWidth" for n in ui_quarantine.iter()) and
       any(n.get("control_name") == "RenderDeferredSSAO" for n in ui_quarantine.iter()))
verify("unsafe adaptive resolution hidden", not any(n.get("name") in ("kami_adaptive_resolution","kami_resolution") and n.get("enabled")!="false" for n in ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot().iter()))
verify("rendering capability GL query cache", 'static GLint texture_limit = 0, renderbuffer_limit = 0;' in read("indra/newview/pipeline.cpp"))
# Catch specific C++ compile failures before entering the costly MSVC step.
import re
headers = [
    "indra/newview/kami_signature.h",
    "indra/newview/kami_atmosphere.h",
    "indra/newview/kami_performance_mode.h",
    "indra/newview/kami_quality_presets.h",
]
bad_control_handle = re.compile(r"(?:auto|LLControlVariable)\s*\*\s*\w+\s*=\s*gSavedSettings\.getControl\(")
verify("MSVC control handle compatibility", all(not bad_control_handle.search(read(path)) for path in headers))
verify("camera vector uses LLVector3d indexing", all(part not in read("indra/newview/kami_signature.h") for part in ("p.mV[", "f.mV[")))
verify("Windows PDH status definitions available", "#include <pdhmsg.h>" in read("indra/newview/kami_hardware_monitor.h"))
verify("SSR requests resize on the pipeline instance", "gPipeline.requestResizeScreenTexture();" in read("indra/newview/pipeline.cpp"))
verify("retired private GShade/MXAO installer removed from Preferences",
       "Install-Private-MXAO.ps1" not in read("indra/newview/llfloaterpreference.cpp") and
       "Enable-KAMI-FX.ps1" not in read("indra/newview/llfloaterpreference.cpp"))
verify("new cinematic UI binds both real preview and restore actions",
       'KAMICinematic::apply();' in read("indra/newview/llfloaterpreference.cpp") and
       'KAMICinematic::restore();' in read("indra/newview/llfloaterpreference.cpp"))

print("KAMI next-build integration checks passed.")
