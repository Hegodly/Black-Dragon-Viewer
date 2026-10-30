#!/usr/bin/env python3
"""Fail early if any of the six real KAMI smart-physics features is missing.

Static contracts only: Windows compilation and in-world CPU/FPS verification
remain mandatory. No claims of 30/60fps during GPU-bound workloads.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def text(path):return (R/path).read_text(encoding="utf-8-sig")
def check(name,expression):
 if not expression: raise RuntimeError("KAMI Smart Physics feature failed: "+name)
 print("PASS",name)

cpp=text("indra/newview/llphysicsmotion.cpp")
hdr=text("indra/newview/llphysicsmotion.h")
perf=text("indra/newview/llfloaterperformance.cpp")
settings=ET.parse(R/"indra/newview/app_settings/settings_blackdragon.xml").getroot()
ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
floater=ET.parse(R/"indra/newview/skins/default/xui/en/floater_performance.xml").getroot()
keys={}
pairs=list(settings.find("map"))
for i in range(len(pairs)-1):
 if pairs[i].tag=="key" and pairs[i+1].tag=="map":
  values=list(pairs[i+1])
  try:
   value=values[[x.text for x in values].index("Value")+1].text
  except (ValueError,IndexError):
   value=None
  keys[pairs[i].text]=value

for control in ("KAMIPhysicsSmart","KAMIPhysicsQuality","KAMIPhysicsProtectSelf",
                "KAMIPhysicsStabilizer","KAMIPhysicsCullOffscreen","KAMIPhysicsMonitor",
                "KAMIPhysicsNearDistance"):
 check("persisted control "+control,control in keys)
check("smart physics opt-in: legacy default unaffected",keys["KAMIPhysicsSmart"]=="0")
check("monitor opt-in to minimize runtime overhead",keys["KAMIPhysicsMonitor"]=="0")
check("own avatar priority default enabled",keys["KAMIPhysicsProtectSelf"]=="1")
check("safety physics stabilizer enabled when opted in",keys["KAMIPhysicsStabilizer"]=="1")
check("offscreen reduction enabled when opted in",keys["KAMIPhysicsCullOffscreen"]=="1")
check("balanced physics quality default",keys["KAMIPhysicsQuality"]=="1")
# Linden Lab's original physics controls live in settings.xml, while
# KAMI's new opt-in controls live in settings_blackdragon.xml.
native=ET.parse(R/"indra/newview/app_settings/settings.xml").getroot()
native_keys={item.text for item in native.iter("key") if item.text}
check("original physics enable toggle retained","AvatarPhysics" in native_keys)
check("original six-level visual physics LOD retained",
      "RenderAvatarPhysicsLODFactor" in native_keys)

native={
 "kami_physics_smart":("check_box","KAMIPhysicsSmart"),
 "kami_physics_quality":("combo_box","KAMIPhysicsQuality"),
 "kami_physics_self":("check_box","KAMIPhysicsProtectSelf"),
 "kami_physics_stabilizer":("check_box","KAMIPhysicsStabilizer"),
 "kami_physics_cull":("check_box","KAMIPhysicsCullOffscreen"),
 "kami_physics_distance":("combo_box","KAMIPhysicsNearDistance"),
 "kami_physics_monitor":("check_box","KAMIPhysicsMonitor"),
}
for name,(tag,setting) in native.items():
 matches=[n for n in ui.iter() if n.get("name")==name]
 check("Fire Garden toggle "+name,len(matches)==1 and matches[0].tag==tag and
       matches[0].get("control_name")==setting)
old_toggle=[x for x in ui.iter() if x.get("control_name")=="AvatarPhysics" and x.tag=="check_box"]
old_qual=[x for x in ui.iter() if x.get("control_name")=="RenderAvatarPhysicsLODFactor" and x.tag=="combo_box"]
check("original user physics ON/OFF preserved",len(old_toggle)==1)
check("original six quality levels preserved",len(old_qual)==1 and
      len(old_qual[0].findall("combo_box.item"))==6)
check("4 smart quality levels are wired",
      len(next(n for n in ui.iter() if n.get("name")=="kami_physics_quality").findall("combo_box.item"))==4)
check("near distance tiers available",
      len(next(n for n in ui.iter() if n.get("name")=="kami_physics_distance").findall("combo_box.item"))==4)

rates=re.search(r"static const F32 fps\[4\]\[3\]\s*=\s*\{(.+?)\n\s*\};",cpp,re.S)
check("four actual 3-tier scheduling tables",rates is not None and
      len(re.findall(r"\{\s*[0-9]+\.f,\s*[0-9]+\.f,\s*[0-9]+\.f\}",rates.group(1)))==4)
check("distance uses squared meters, not costly sqrt", "offset.magVecSquared()" in cpp)
check("offscreen stops work for invisible remote avatars",
      'KAMIPhysicsCullOffscreen' in cpp and '!avatar->isVisible()' in cpp)
check("self avatar never throttled when priority enabled",
      '!(own && gSavedSettings.getBOOL("KAMIPhysicsProtectSelf"))' in cpp)
check("physics jitter protection caps time-step & work",
      'llmin(time-mLastTime, 0.12f)' in cpp and 'llmin(steps, 4u)' in cpp)
check("spring mass clamped with stabilizer", 'llmax(raw_mass,0.001f)' in cpp)
check("original physics dynamics preserved opt-out",
      'const bool smart = gSavedSettings.getBOOL("KAMIPhysicsSmart")' in cpp and
      'const bool stabilize = gSavedSettings.getBOOL("KAMIPhysicsSmart")' in cpp)
check("controller-local elapsed time avoids global avatar coupling",
      'mLastKAMISimulationTime = 0.f' in hdr)
check("no remote physics network/shape messages added",
      all(x not in cpp for x in ('KAMIPhysicsSend','KAMIPhysicsNetwork')))
check("monitor uses CPU wall clock only when enabled",
      'if (monitoring) start = KAMIPhysicsStats::Clock::now();' in cpp)
check("monitor counts unique active avatars and skipped sim ticks",
      'std::set<const LLCharacter*> unique' in cpp and
      'KAMIPhysicsStats::skip()' in cpp)
check("statistics in real native Performance floater",
      'KAMIPhysicsStats::summary()' in perf and
      any(n.get("name")=="kami_physics_metrics" for n in floater.iter()))
check("physics monitor linked from Display",
      'showInstance("performance")' in text("indra/newview/llfloaterpreference.cpp"))
check("original body physics not disabled in FPS graphics presets",
      all('set("AvatarPhysics"' not in text(src) for src in
          ("indra/newview/kami_quality_presets.h",
           "indra/newview/kami_performance_mode.h",
           "indra/newview/kami_smart_engine.h")))
print("All six native avatar physics feature contracts passed. Must verify smoothness and FPS in an actual crowded region.")
