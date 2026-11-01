#!/usr/bin/env python3
"""KAMI ALIVE inspector source and integrated validation.

Does not claim runtime compilation, FPS benchmark, or perfect material
recognition: metadata classification can miss per-pixel ORM values.
"""
import ast
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
file = ROOT / "kami/alive/apply-material-inspector.py"
s = file.read_text(encoding="utf-8-sig")
ast.parse(s)
for p in (
    "KAMIAliveMaterialOverrides", "KAMIAliveOverrideRevision",
    "Kami.Alive.Inspect", "Kami.Alive.Metal", "Kami.Alive.Standard",
    "Kami.Alive.Auto", "kami_alive_material_override", "getHash()",
    "mKAMIAliveCachedRevision", "const S32 max_faces = 96",
    "local.size() < 512", 'LLNotificationsUtil::add("SystemMessage", args)',
):
    assert p in s, f"missing ALIVE inspector implementation: {p}"

def read(path):
    return (ROOT / path).read_text(encoding="utf-8-sig")

def integrated():
    settings = ET.parse(ROOT / "indra/newview/app_settings/settings.xml").getroot()
    keys = [k.text for k in settings.findall("./map/key")]
    for k in ("KAMIAliveEnabled", "KAMIAliveMaterialOverrides",
              "KAMIAliveOverrideRevision"):
        assert keys.count(k) == 1, k
    menu = ET.parse(ROOT / "indra/newview/skins/default/xui/en/menu_viewer.xml").getroot()
    workspaces = [x for x in menu if x.get("name") == "KAMI_Workspaces"]
    assert len(workspaces) == 1
    for name, key in (("KAMI_Alive_Inspect","Kami.Alive.Inspect"),
                       ("KAMI_Alive_Auto","Kami.Alive.Auto"),
                       ("KAMI_Alive_Metal","Kami.Alive.Metal"),
                       ("KAMI_Alive_Standard","Kami.Alive.Standard")):
        items = [x for x in workspaces[0] if x.get("name") == name]
        assert len(items) == 1, name
        assert items[0].find("menu_item_call.on_click").get("function") == key
    cpp = read("indra/newview/llviewermenu.cpp")
    for code in ('static void kamiAliveMaterialCommand(S32 mode)',
                 'commit.add("Kami.Alive.Inspect"',
                 'commit.add("Kami.Alive.Auto"',
                 'commit.add("Kami.Alive.Metal"',
                 'commit.add("Kami.Alive.Standard"',
                 'local.size() < 512', 'const S32 max_faces = 96'):
        assert code in cpp, code
    mat = read("indra/newview/llfetchedgltfmaterial.cpp")
    hdr = read("indra/newview/llfetchedgltfmaterial.h")
    assert "mKAMIAliveCachedRevision = -1" in hdr
    assert "mKAMIAliveCachedRevision != revision" in mat
    assert "shader->uniform1f(sKAMIAliveManualMode, mKAMIAliveManualMode)" in mat
    for p in ("indra/newview/app_settings/shaders/class1/deferred/pbropaqueF.glsl",
              "indra/newview/app_settings/shaders/class1/gltf/pbrmetallicroughnessF.glsl"):
        shader = read(p)
        assert shader.count("uniform float kami_alive_material_override;") == 1, p
        assert "kami_alive_material_override > 0.5" in shader
        assert "kami_alive_material_override < -0.5" in shader
    assert "glFinish(" not in cpp and "glReadPixels(" not in cpp
    print("PASS native KAMI ALIVE inspector: menu, settings, loaded PBR faces, per-draw shader mode")

if __name__ == "__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--integrated",action="store_true")
    args=parser.parse_args()
    print("PASS KAMI ALIVE inspector script syntax/source anchors")
    if args.integrated:
        integrated()
    else:
        print("Source preflight only; runtime visuals and frame-time benchmarking await compiled viewer.")
