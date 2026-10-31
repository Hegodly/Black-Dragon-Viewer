#!/usr/bin/env python3
"""Verify actual PBR bypass, fallback and exact undo, not a vintage-only toggle.

Static checks cannot prove Windows GPU execution or material appearance.
"""
from pathlib import Path
import ast
import argparse
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
SCRIPT=R/"kami/renderer/apply-true-pbr-off.py"
VIEW=R/"indra/newview"
CHECKS={
 "llvovolume.cpp":(
  'bool is_pbr = gltf_mat != nullptr;',
  'if (!is_pbr && te && te->getGLTFRenderMaterial())',
  'facep->setTexture(LLRender::DIFFUSE_MAP, vobj->getTEImage(i))',
  'gSavedSettings.getBOOL("KAMIPBREnabled")',
  'facep->setTexture(LLRender::GLTF_NORMAL_MAP, nullptr)',
 ),
 "pipeline.cpp":(
  'LLGLTFMaterial* gltf_mat = gSavedSettings.getBOOL("KAMIPBREnabled")',
  'if (!gSavedSettings.getBOOL("KAMIPBREnabled"))',
  'return; // No glTF/PBR GPU draw in true OFF mode.'
 ),
 "lldrawpoolalpha.cpp":(
  'if (gSavedSettings.getBOOL("KAMIPBREnabled") &&',
  'rigged && mType == LLDrawPool::POOL_ALPHA_POST_WATER',
 ),
 "lldrawpoolterrain.cpp":(
  'if (use_textures || !gSavedSettings.getBOOL("KAMIPBREnabled"))',
 ),
 "llviewercontrol.cpp":(
  'handleKAMIPBREnabled(const LLSD& newvalue)',
  'gPipeline.rebuildDrawInfo()',
  'gPipeline.rebuildTerrain()',
  '"KAMIPBREnabled", handleKAMIPBREnabled',
  '"KAMIPBROffBackupValid"',
 ),
}
def source():
    s=SCRIPT.read_text(encoding="utf-8")
    ast.parse(s,filename=str(SCRIPT))
    for p,parts in CHECKS.items():
        for part in parts:
            assert part in s, (p,part)
    for forbidden in ("RenderFarClip","RenderVolumeLODFactor",
                      "RenderAvatarLODFactor","KombatantRenderWidth",
                      "RenderResolutionDivisor"):
        assert forbidden not in s, ("Off cannot change scene quality",forbidden)
    assert 'OFF is NOT the old Vintage effect' in s
    assert 'PBR-only glTF scene objects' in s
    print("PASS source: true PBR material passes bypassed; legacy fallback and caveats enforced")

def integrated():
    for path,required in CHECKS.items():
        s=(VIEW/path).read_text(encoding="utf-8-sig")
        for part in required:
            assert part in s, (path,part)
    terrain=(VIEW/"lldrawpoolterrain.cpp").read_text(encoding="utf-8-sig")
    assert 'sShader = &gDeferredTerrainProgram;' in terrain
    root=ET.parse(VIEW/"app_settings/settings_blackdragon.xml").getroot().find("map")
    pairs=list(root)
    entries={}
    for i,p in enumerate(pairs[:-1]):
        if p.tag=="key" and p.text in (
            "KAMIPBREnabled","KAMIPBROffBackupValid","KAMIPBROffVintage",
            "KAMIPBROffHDR","KAMIPBROffEmissive","KAMIPBROffProbes",
            "KAMIPBROffSSR","KAMIPBROffMirrors"):
            data=list(pairs[i+1])
            entries[p.text]={data[j].text:data[j+1].text for j in range(0,len(data),2)}
    assert len(entries)==8, entries.keys()
    assert entries["KAMIPBREnabled"]["Value"]=="1"  # opt-out, never surprise resident
    assert entries["KAMIPBREnabled"]["Type"]=="Boolean"
    ui=ET.parse(VIEW/"skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
    toggle=[n for n in ui.iter("check_box") if n.get("name")=="kami_pbr_enabled"]
    assert len(toggle)==1
    assert toggle[0].get("control_name")=="KAMIPBREnabled"
    assert toggle[0].get("initial_value")=="true"
    assert "PBR Rendering: ON / OFF"==toggle[0].get("label")
    assert "may disappear" in toggle[0].get("tool_tip","")
    assert not list(n for n in ui.iter("check_box") if n.get("name")=="kami_reduced_pbr")
    print("PASS integration: PBR material/terrain/alpha GPU path bypass, ON default, backup and native UI")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--integrated",action="store_true")
    args=p.parse_args()
    source()
    if args.integrated:integrated()

if __name__=="__main__":
    main()
