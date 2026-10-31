#!/usr/bin/env python3
"""Check KAMI adaptive avatar physics & Windows telemetry integration.

Code/path checks, not a substitute for compilation, GPU counter testing,
user-avatar motion verification, or repeatable FPS benchmarks.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def read(path):return (R/path).read_text(encoding="utf-8-sig")
def expect(name,ok):
    if not ok:raise RuntimeError("KAMI avatar-physics/monitor check failed: "+name)
    print("PASS",name)
physics=read("indra/newview/llphysicsmotion.cpp")
header=read("indra/newview/llphysicsmotion.h")
settings=read("indra/newview/app_settings/settings_blackdragon.xml")
ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
expect("adaptive physics controller actually throttles remote spring simulation",
       'if (interval > 0.f && mKAMILastPhysicsUpdate > 0.f' in physics)
expect("two adaptive physics limiters never run simultaneously",
       'adaptive_physics && !gSavedSettings.getBOOL("KAMIPhysicsSmart")' in physics)
expect("no per-avatar pixel-area square root in physics hot path",
       'const F32 area = llmax(0.f, mCharacter->getPixelArea());' in physics and
       'sqrtf(llmax(0.f, mCharacter->getPixelArea()))' not in physics)
expect("self-avatar physics remains fully responsive",
       'dynamic_cast<LLVOAvatarSelf*>(mCharacter) == nullptr' in physics)
expect("native body physics six original springs retained",
       physics.count('addMotion(motion);')>=6)
expect("physics time accumulates rather than overwrites on skipped frames",
       physics.count('mKAMILastPhysicsUpdate = time;')==1)
expect("existing avatar physics master toggle retained",
       'static const LLCachedControl<bool> av_physics' in physics)
expect("physics elapsed time remains passed to existing solver",
       'update_visuals |= motion->onUpdate(time)' in physics)
expect("controller tracks last update without global/static shared timer",
       'F32 mKAMILastPhysicsUpdate = 0.f;' in header)
for key in ("KAMIAvatarPhysicsAdaptive","KAMIAvatarPhysicsNearPixels",
            "KAMIAvatarPhysicsDistantPixels","KAMIAvatarPhysicsMidHz","KAMIAvatarPhysicsFarHz"):
    expect("native adaptive setting "+key,("<key>"+key+"</key>") in settings)
for name,ctrl in (("AvatarPhysics","AvatarPhysics"),
                  ("kami_adaptive_avatar_physics","KAMIAvatarPhysicsAdaptive")):
    matches=[x for x in ui.iter() if x.get("name")==name]
    expect("Fire Garden toggle "+name,
           len(matches)==1 and matches[0].get("control_name")==ctrl)
expect("existing avatar physics LOD dropdown still present",
       any(x.get("control_name")=="RenderAvatarPhysicsLODFactor" and x.tag=="combo_box" for x in ui.iter()))
expect("quality presets cannot suppress visible avatar physics motion",
       'set("RenderAvatarPhysicsLODFactor"' not in read("indra/newview/kami_quality_presets.h"))
expect("Performance Mode does not throttle body motion via hidden LOD changes",
       '"RenderAvatarPhysicsLODFactor"' not in read("indra/newview/kami_performance_mode.h"))

reg=read("indra/newview/llviewerfloaterreg.cpp")
monitor=read("indra/newview/kami_hardware_monitor.h")
menu=ET.parse(R/"indra/newview/skins/default/xui/en/menu_viewer.xml").getroot()
monitorUI=ET.parse(R/"indra/newview/skins/default/xui/en/floater_kami_hardware_monitor.xml").getroot()
expect("registered hardware monitor C++ class",
       'LLFloaterReg::build<KAMIHardwareMonitor>' in reg and
       '#include "kami_hardware_monitor.h"' in reg)
expect("menu opens actual floater",any(node.get("parameter")=="kami_hardware_monitor" for node in menu.iter("menu_item_check.on_click")))
expect("monitor explicitly samples only once per second", 'mTimer.setTimerExpirySec(1.0f)' in monitor)
expect("hardware Windows CPU readings", "GetSystemTimes" in monitor)
expect("real RAM usage", "GlobalMemoryStatusEx" in monitor and "GetProcessMemoryInfo" in monitor)
expect("Windows GPU engine counters, not guessed usage", "PdhGetFormattedCounterArrayW" in monitor)
expect("DXGI video-memory allocation", "QueryVideoMemoryInfo" in monitor)
expect("true rendered FPS median", "LLStatViewer::FPS" in monitor)
for key in ("kami_hw_fps","kami_hw_frame","kami_hw_cpu","kami_hw_gpu",
            "kami_hw_ram","kami_hw_process","kami_hw_vram","kami_hw_resolution"):
    expect("metric "+key,len([n for n in monitorUI.iter() if n.get("name")==key])==1)
expect("no automatic FPS cap or world-resolution change from hardware monitor",
       not any(f'set("{k}"' in monitor for k in
               ("FramePerSecondLimit","KombatantRenderWidth","RenderResolutionMultiplier")))
print("KAMI avatar-physics and hardware monitor contracts verified. Actual performance and hardware access still require live GPU tests.")
