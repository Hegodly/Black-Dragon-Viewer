#!/usr/bin/env python3
"""KAMI avatar-physics readiness detector contract; --integrated after patching.

This checks source wiring, not whether an arbitrary SL mesh body is properly
weighted or whether a physics wearable exists in a user's inventory.
"""
from pathlib import Path
import argparse
import ast
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
PATCH=R/"kami/physics/fix-visible-physics-detection.py"
ADAPTIVE=R/"kami/performance/apply-adaptive-avatar-physics.py"
CPP=R/"indra/newview/llphysicsmotion.cpp"
UI=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
PANEL=R/"indra/newview/skins/default/xui/en/floater_performance.xml"
SETTINGS=R/"indra/newview/app_settings/settings_blackdragon.xml"

def source():
    s=PATCH.read_text(encoding="utf-8")
    adaptive=ADAPTIVE.read_text(encoding="utf-8")
    ast.parse(s,filename=str(PATCH))
    ast.parse(adaptive,filename=str(ADAPTIVE))
    for token in (
        'gAgentWearables.getWearableCount(LLWearableType::WT_PHYSICS)',
        'gSavedSettings.getBOOL("AvatarPhysics")',
        'gSavedSettings.getF32("RenderAvatarPhysicsLODFactor")',
        "No nonzero MaxEffect",
        "hasConfiguredEffect()",
        "self_visual_updates",
        "recordSelfEffects",
        "Physics OFF in viewer settings",
        "NO PHYSICS LAYER WORN",
        "Physics layer worn; fitted mesh collision-bone rigging required",
        "KAMIPhysicsMonitor",
        "getParamValue(MAX_EFFECT)",
        "AvatarPhysics",
        "Physics Status / Diagnostics",
        'if (lod_factor > 0.f &&',
        'gSavedSettings.getBOOL("KAMIPhysicsProtectSelf")',
    ):
        assert token in s, ("Missing native physics readiness detector", token)
    assert '"KAMIAvatarPhysicsAdaptive", false' in adaptive
    assert '("KAMIAvatarPhysicsAdaptive","Boolean","integer","0",' in adaptive
    for forbidden in ("RenderAvatarLODFactor","KombatantRenderWidth",
                      "RenderResolutionMultiplier","RenderShadowDetail"):
        assert forbidden not in s, ("Physics diagnostics must not degrade rendering",forbidden)
    assert "getWearableCount" not in adaptive
    print("PASS physics detector: original six springs, worn layer, true self visual changes, no hidden graphics downgrade")

def integrated():
    cpp=CPP.read_text(encoding="utf-8-sig")
    for token in (
        '#include "llagentwearables.h"',
        'bool hasConfiguredEffect()',
        'getParamValue(MAX_EFFECT) > 0.0001f',
        'gAgentWearables.getWearableCount(LLWearableType::WT_PHYSICS)',
        'if (lod_factor > 0.f &&',
        'gSavedSettings.getBOOL("KAMIPhysicsProtectSelf")',
        'KAMIPhysicsStats::recordSelfEffects(configured_effects, update_visuals)',
        'current.self_visual_updates',
        'NO PHYSICS LAYER WORN',
        "Physics quality 0 (no visible physics)",
    ):
        assert token in cpp, token
    assert cpp.count('addMotion(motion);')>=6
    assert 'if (!av_physics)' in cpp
    assert 'gSavedSettings.getBOOL("KAMIPhysicsSmart")' in cpp

    root=ET.parse(UI).getroot()
    master=[x for x in root.iter("check_box") if x.get("control_name")=="AvatarPhysics"]
    assert len(master)==1 and "physics movement" in master[0].get("label","")
    note=[x for x in root.iter("text") if x.get("name")=="kami_physics_note"]
    assert len(note)==1 and "Physics clothing layer" in (note[0].text or "")
    link=[x for x in root.iter("button") if x.get("name")=="kami_physics_open_monitor"]
    assert len(link)==1 and "Diagnostics" in link[0].get("label","")
    perf=ET.parse(PANEL).getroot()
    stats=[x for x in perf.iter("text") if x.get("name")=="kami_physics_metrics"]
    assert len(stats)==1 and int(stats[0].get("height"))>=95
    settings=ET.parse(SETTINGS).getroot().find("map")
    lst=list(settings)
    defaults={}
    for i,e in enumerate(lst[:-1]):
        if e.tag=="key" and e.text=="KAMIAvatarPhysicsAdaptive":
            fields=list(lst[i+1])
            defaults={fields[j].text: fields[j+1].text for j in range(0,len(fields),2)}
    assert defaults.get("Value")=="0", "Remote physics optimization must be opt-in for visible physics"
    print("PASS integrated source + real UI: physics mode, layer awareness, motion results and safe optimizer default")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--integrated",action="store_true")
    opts=parser.parse_args()
    source()
    if opts.integrated:integrated()

if __name__=="__main__":
    main()
