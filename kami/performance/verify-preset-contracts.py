#!/usr/bin/env python3
"""Static contract tests for all KAMI graphics presets and safe world resolution.

These tests verify *code and exact settings*, NOT promised physical FPS.
Actual FPS gains require a user-scoped, repeatable in-world benchmark.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
def source(name):
    return (ROOT/name).read_text(encoding="utf-8-sig")

h=source("indra/newview/kami_quality_presets.h")
ui=ET.parse(ROOT/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
settings=ET.parse(ROOT/"indra/newview/app_settings/settings_blackdragon.xml").getroot()
smart=source("indra/newview/kami_smart_engine.h")
mode=source("indra/newview/kami_performance_mode.h")
pipeline=source("indra/newview/pipeline.cpp")
display=source("indra/newview/llviewerdisplay.cpp")

def verify(name,condition):
    if not condition: raise AssertionError("KAMI preset contract FAILED: "+name)
    print("PASS",name)

def array(name):
    match=re.search(r"\b"+re.escape(name)+r"\[\]\s*=\s*\{([^}]+)\}",h)
    verify("five values for "+name,match is not None)
    values=[float(re.sub(r"[fFuU]$","",x.strip())) for x in match.group(1).split(",")]
    verify("exactly five presets: "+name,len(values)==5)
    return values

def nondecreasing(a):
    return all(a[i]<=a[i+1] for i in range(4))

def nonincreasing(a):
    return all(a[i]>=a[i+1] for i in range(4))

for key in ("distance","objectLOD","avatarLOD","shadowScale","textures",
            "impostorLimit","particles","lights","probes","probeDetail",
            "shadowDetail","ssrSamples"):
    a=array(key)
    verify(key+" ordered from low-cost to highest-quality",
           nondecreasing(a))
verify("low draws closer than high",array("distance")[0]<array("distance")[2])
verify("cinematic can render more avatars than low",array("impostorLimit")[4]>array("impostorLimit")[0])
verify("low renders fewer local lights",array("lights")[0]<array("lights")[2])
verify("low reduces shadow resolution",array("shadowScale")[0]<array("shadowScale")[3])
verify("ultra enables reflective screen-space quality", 'set("RenderScreenSpaceReflections", q >= 3)' in h)
verify("high enables native SSAO", 'set("RenderDeferredSSAO", q >= 2)' in h)
verify("broken bloom remains disabled at every quality level", 'set("RenderGlow", false)' in h and 'set("RenderGlowStrength",' not in h)
verify("broken FXAA avoided", 'set("RenderFSAAType", q >= 2 ? 2 : 0)' in h)

combos=[e for e in ui.iter() if e.get("name")=="kd_quality_preset"]
verify("quality combo exists exactly once",len(combos)==1)
quality=combos[0]
verify("quality combo bound to saved KAMI selection",quality.get("control_name")=="KAMIQualityPreset")
options={item.get("value"):item.get("label") for item in quality.findall("combo_box.item")}
verify("all five quality settings exist",set(options)=={"-1","0","1","2","3","4"})
verify("all quality labels visibly distinct",len(set(options.values()))==6)
verify("quality callback actually invokes implementation", "KAMIQuality::apply(chosen)" in source("indra/newview/llfloaterpreference.cpp"))
verify("quality applies no FPS cap, 4K changes or dangerous scale",all(
    f'set("{setting}"' not in h
    for setting in ("FramePerSecondLimit","KombatantRenderWidth",
                    "RenderResolutionMultiplier","RenderVSyncEnable")
))
verify("quality restores prior Smart modes before changing graphics",
       "leaveOtherModes()" in h and 'set("KAMIAutoTune", false)' in h)
verify("Performance preset actually reduces avatars, light and particles",
       all(x in mode for x in ('set("RenderAvatarMaxNonImpostors", 4)',
                             'set("RenderMaxPartCount", 512)',
                             'set("RenderLocalLightCount", 16)')))
verify("Smart Auto Tune does not crop world via unverified scaling",
       all(x not in smart for x in ('atMostF32("RenderResolutionMultiplier", 0.75f)',
                                  'atMostF32("RenderResolutionMultiplier", 0.6f)',
                                  'set("KombatantRenderWidth", 0)')))
verify("Performance preset does not erase explicit 4K",
       'set("KombatantRenderWidth", 0)' not in mode)
verify("foreground FPS remains uncapped by presets",
       all('set("FramePerSecondLimit"' not in code for code in (smart,mode,h)))
verify("native/4K/8K selection is preserved",
       {x.get("value") for e in ui.iter() if e.get("name")=="kd_resolution"
             for x in e.findall("combo_box.item")} >= {"0","3840","7680"})
verify("no 12K live rendering options",
       all(int(x.get("value","0"))<=7680
           for e in ui.iter() if e.get("control_name")=="KombatantRenderWidth"
           for x in e.findall("combo_box.item")))
verify("actual allocated GPU world buffer is measured",
       'gPipeline.mMainRT.screen.getWidth()' in display and
       'KAMILiveResolutionStatus' in display)
verify("live GPU capabilities are cached rather than synchronized each redraw",
       'static GLint texture_limit = 0, renderbuffer_limit = 0;' in pipeline)
verify("manual 4K is still selectable even in Potato Mode",
       'set("RenderResolutionMultiplier", 1.f)' not in smart)
verify("native tone mapping remains untouched by presets",
       'set("RenderTonemapType"' not in h)
print("All static preset/resolution contracts verified. Requires in-world FPS and graphics QA.")
