#!/usr/bin/env python3
"""Source/integration checks for all KAMI ALIVE PBR profiles."""
import ast,argparse
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
S=(R/"kami/alive/apply-alive-profiles.py").read_text(encoding="utf-8")
ast.parse(S)
for t in ("kamiAliveSurface","Kami.Alive.Gem","Kami.Alive.Skin","Kami.Alive.Eyes",
          "Kami.Alive.Fabric","Kami.Alive.Leather","local.erase(key)",
          "mode >= -1 && mode <= 6","mKAMIAliveManualMode","gain * 0.80"):
    assert t in S,t
assert "glFinish(" not in S and "glReadPixels(" not in S
assert "texture(" not in S.split('surface="""')[1].split('"""')[0]
def read(x):return (R/x).read_text(encoding="utf-8-sig")
def integrated():
    x=R/"indra/newview"
    root=ET.parse(x/"app_settings/settings.xml").getroot()
    assert [e.text for e in root.findall("./map/key")].count("KAMIAliveMaterialOverrides")==1
    menu=ET.parse(x/"skins/default/xui/en/menu_viewer.xml").getroot()
    cpp=read("indra/newview/llviewermenu.cpp")
    for n in ("Gem","Skin","Eyes","Fabric","Leather"):
        assert len([e for e in menu.iter("menu_item_call") if e.get("name")=="KAMI_Alive_"+n])==1
        assert 'commit.add("Kami.Alive.'+n+'"' in cpp
    assert "kamiAliveModeName" in cpp and "local.erase(key)" in cpp
    mat=read("indra/newview/llfetchedgltfmaterial.cpp")
    assert "llclamp((F32)overrides[material_key].asInteger(), -1.f, 6.f)" in mat
    for sub in ("deferred/pbropaqueF.glsl","gltf/pbrmetallicroughnessF.glsl"):
        a=read("indra/newview/app_settings/shaders/class1/"+sub)
        assert a.count("kamiAliveSurface(")==2
        assert "if (kami_alive_material_override > 0.5 && kami_alive_material_override < 1.5)" in a
        assert a.count("uniform float kami_alive_material_override;")==1
    print("PASS six PBR response modes, five manual selection actions, no new render passes")
p=argparse.ArgumentParser();p.add_argument("--integrated",action="store_true")
arg=p.parse_args()
print("PASS KAMI ALIVE profiles source preflight")
if arg.integrated:integrated()
else:print("No compiled or in-world FPS result implied")
