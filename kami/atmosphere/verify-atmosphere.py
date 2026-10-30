#!/usr/bin/env python3
"""Validate native KAMI Atmosphere Engine integration (static contracts only)."""
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
def text(p): return (R/p).read_text(encoding="utf-8-sig")
def gate(name,passed):
 if not passed: raise AssertionError("Atmosphere Engine failed: "+name)
 print("PASS",name)

header=text("indra/newview/kami_atmosphere.h")
prefs=text("indra/newview/llfloaterpreference.cpp")
ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
settings=ET.parse(R/"indra/newview/app_settings/settings_blackdragon.xml").getroot()
keys={}
entries=list(settings.find("map"))
for index in range(len(entries)-1):
 if entries[index].tag=="key" and entries[index+1].tag=="map":
  fields=list(entries[index+1])
  for i,x in enumerate(fields):
   if x.tag=="key" and x.text=="Value":
    keys[entries[index].text]=fields[i+1].text
    break
for key in ("KAMIAtmosphereMood","KAMIAtmosphereStrength","KAMIAtmosphereFade",
            "KAMIAtmosphereBloom","KAMIAtmosphereStatus"):
 gate("native persistent setting "+key,key in keys)
gate("bloom disabled by default",keys["KAMIAtmosphereBloom"]=="0")
gate("mood not automatically applied", "KAMIAtmosphere::apply();" in prefs and
     not "KAMIAtmosphere::apply();" in text("indra/newview/llviewerdisplay.cpp"))
gate("no parcel / estate mutation",
     all(s not in header for s in ('updateParcel(', 'updateRegion(',
          'setExperienceEnvironment(', 'setSharedEnvironment(', 'setAgentPosition(', 'sendMessage(')))
     and 'ENV_PARCEL' not in header and 'ENV_REGION' not in header)
gate("actual local EEP update", 'setEnvironment(LLEnvironment::ENV_LOCAL,sky)' in header)
gate("actual sky color controls", all(x in header for x in (
 'setAmbientColor(', 'setSunlightColor(', 'setBlueHorizon(', 'setCloudColor(',
 'setHazeDensity(', 'setCloudShadow(', 'setStarBrightness(', 'setSunRotation(')))
gate("four distinct moods",all(m in header for m in
 ('Night Studio','Golden Hour','Overcast Cinema','Luxury Interior')))
gate("intensity blends cloned source sky", 's.sourceSky->buildClone()' in header and 'amount()' in header)
gate("prior personal EEP selection retained",all(x in header for x in
 ('getSelectedEnvironment()','s.previous','s.hadLocal','s.dayLength','s.dayOffset',
  's.fixed','setSelectedEnvironment(s.previous')))
gate("previous local sky and water restored", 'environment.setEnvironment(LLEnvironment::ENV_LOCAL,s.fixed)' in header)
gate("previous day cycle restored", 'environment.setEnvironment(LLEnvironment::ENV_LOCAL,' in header and 's.day,s.dayLength,s.dayOffset' in header)
gate("do not overwrite other EEP editor takeover", 'ownLocal' in header and '==s.activeSky' in header)
gate("native glow is optional and restored",all(x in header for x in
 ('KAMIAtmosphereBloom','s.hadGlow','s.glowStrength','applyBloom(false)')))
gate("frame cap, render resolution and network untampered",not any(
   x in header for x in ('FramePerSecondLimit','KombatantRenderWidth',
    'RenderResolutionMultiplier','RenderResolutionDivisor',
    'RenderVSyncEnable','llhttp','HttpRequest')))
names={n.get("name"):n for n in ui.iter() if n.get("name")}
for name,setting in [
 ("kami_atmo_mood","KAMIAtmosphereMood"),("kami_atmo_strength","KAMIAtmosphereStrength"),
 ("kami_atmo_fade","KAMIAtmosphereFade"),("kami_atmo_bloom","KAMIAtmosphereBloom")]:
 gate("Fire Garden native widget "+name,name in names and
     names[name].get("control_name")==setting)
for name in ("kami_atmo_apply","kami_atmo_restore"):
 gate("functional button "+name,name in names and names[name].tag=="button" and
      f'"{name}"' in prefs)
gate("4 mood dropdown items",len(names["kami_atmo_mood"].findall("combo_box.item"))==4)
gate("staged real C++ engine equals versioned source",
    header==text("kami/atmosphere/kami_atmosphere.h"))
gate("packaged engine manifest",
    (R/"indra/newview/skins/default/KAMI-ATMOSPHERE-ENGINE.txt").exists())
print("All KAMI Atmosphere Engine source/UI/EEP contracts passed. Compiled and in-world tests still required.")
