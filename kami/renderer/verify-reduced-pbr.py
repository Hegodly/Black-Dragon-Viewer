#!/usr/bin/env python3
"""Check KAMI native reduced-PBR controls, saved settings and visible warnings."""
from pathlib import Path
import argparse
import ast
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
PATCH=R/"kami/renderer/apply-reduced-pbr.py"
CXX=R/"indra/newview/llviewercontrol.cpp"
UI=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
SETTINGS=R/"indra/newview/app_settings/settings_blackdragon.xml"
PRESERVE=("RenderDisableVintageMode","RenderReflectionsEnabled",
          "RenderScreenSpaceReflections","RenderMirrors",
          "RenderHDREnabled","RenderEnableEmissiveBuffer")
BACKUPS=("KAMIPBRBackupVintage","KAMIPBRBackupReflections",
         "KAMIPBRBackupSSR","KAMIPBRBackupMirrors",
         "KAMIPBRBackupHDR","KAMIPBRBackupEmissive")

def source_check():
    s=PATCH.read_text(encoding="utf-8")
    ast.parse(s,filename=str(PATCH))
    for setting in PRESERVE + BACKUPS:
        assert '"'+setting+'"' in s, ("Missing rendering/restore setting",setting)
    for token in ("handleKAMIReducedPBR", "KAMIReducedPBR",
                  "KAMIPBRBackupValid", "Restores your exact original values",
                  "PBR/glTF material assets still render",
                  "NOT a fully non-PBR engine",
                  'existing.set("visible","false")'):
        assert token in s, token
    for token in ("RenderFarClip","RenderVolumeLODFactor",
                  "RenderAvatarLODFactor","KombatantRenderWidth",
                  "RenderResolutionDivisor"):
        assert token not in s, ("Unexpected quality downgrade",token)
    print("PASS native PBR lighting workload switch, transparent scope, no unrelated quality changes")

def integrated_check():
    cpp=CXX.read_text(encoding="utf-8")
    assert cpp.count("handleKAMIReducedPBR(const LLSD& newvalue)")==1
    assert cpp.count('"KAMIReducedPBR", handleKAMIReducedPBR')==1
    assert cpp.count('gSavedSettings.setBOOL("KAMIPBRBackupValid", true)')==1
    assert cpp.count('gSavedSettings.setBOOL("KAMIPBRBackupValid", false)')==1
    for name in PRESERVE:
        assert 'gSavedSettings.setBOOL("'+name+'"' in cpp, name
    for name in BACKUPS:
        assert 'gSavedSettings.getBOOL("'+name+'")' in cpp, name
        assert 'gSavedSettings.setBOOL("'+name+'"' in cpp, name

    root=ET.parse(SETTINGS).getroot().find("map")
    elems=list(root)
    values={}
    for i,node in enumerate(elems[:-1]):
        if node.tag=="key" and (node.text=="KAMIReducedPBR" or node.text.startswith("KAMIPBR")):
            assert elems[i+1].tag=="map"
            entries=list(elems[i+1])
            fields={entries[j].text:entries[j+1].text
                    for j in range(0,len(entries),2) if entries[j].tag=="key"}
            values[node.text]=fields
    assert len(values)==8, values.keys()
    assert values["KAMIReducedPBR"]["Value"]=="0"
    assert values["KAMIReducedPBR"]["Type"]=="Boolean"
    for backup in BACKUPS:
        assert values[backup]["Persist"]=="1", backup

    ui=ET.parse(UI).getroot()
    toggles=[x for x in ui.iter("check_box") if x.get("name")=="kami_reduced_pbr"]
    assert len(toggles)==1
    toggle=toggles[0]
    assert toggle.get("control_name")=="KAMIReducedPBR"
    assert toggle.get("initial_value")=="false"
    assert "Reduced PBR" in toggle.get("label","")
    tip=toggle.get("tool_tip","")
    assert "NOT a fully non-PBR engine" in tip
    assert "PBR/glTF material assets still render" in tip
    old=[x for x in ui.iter("check_box") if x.get("name")=="RenderDisableVintageMode"]
    assert len(old)==1 and old[0].get("visible")=="false"
    print("PASS integrated C++ signaling, original-setting backups, visual UI and truthful wording")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--integrated",action="store_true")
    args=parser.parse_args()
    source_check()
    if args.integrated:integrated_check()

if __name__=="__main__":
    main()
