#!/usr/bin/env python3
"""KAMI ALIVE v0.2 inspector source preflight and applied-native checks."""
import argparse
import ast
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
src=(R/"kami/alive/apply-material-inspector.py").read_text(encoding="utf-8")
ast.parse(src)
for term in ("KAMI_Alive_Inspect","KAMI.AliveInspect","KAMI.AliveMetal",
             "KAMI.AliveBypass","KAMI.AliveAuto","kamiAliveAdaptPBR",
             "mKAMIAliveOverride","seen_faces<64","mMetallicRoughnessTexture",
             "getGLTFRenderMaterial()","LLNotificationsUtil::add"):
    assert term in src,term
assert "glFinish(" not in src and "glReadPixels(" not in src

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--integrated",action="store_true")
    args=p.parse_args()
    if args.integrated:
        tree=ET.parse(R/"indra/newview/skins/default/xui/en/menu_viewer.xml")
        root=tree.getroot()
        menus=[n for n in root if n.tag=="menu" and n.get("name")=="KAMI_Workspaces"]
        assert len(menus)==1
        for name,handler in (("KAMI_Alive_Inspect","KAMI.AliveInspect"),
                             ("KAMI_Alive_Metal","KAMI.AliveMetal"),
                             ("KAMI_Alive_Bypass","KAMI.AliveBypass"),
                             ("KAMI_Alive_Auto","KAMI.AliveAuto")):
            item=[n for n in menus[0] if n.get("name")==name]
            assert len(item)==1,name
            assert item[0].find("menu_item_call.on_click").get("function")==handler
        for suffix in ("deferred/pbropaqueF.glsl",
                       "gltf/pbrmetallicroughnessF.glsl"):
            glsl=(R/"indra/newview/app_settings/shaders/class1"/suffix).read_text()
            assert glsl.count("uniform float kami_alive_override_mode;")==1
            assert glsl.count("void kamiAliveAdaptPBR(inout vec3 orm)")==1
            assert glsl.count("kamiAliveAdaptPBR(")==2
            assert "if (gain < 0.001 || kami_alive_override_mode < -0.5)" in glsl
        header=(R/"indra/newview/llfetchedgltfmaterial.h").read_text()
        impl=(R/"indra/newview/llfetchedgltfmaterial.cpp").read_text()
        menu=(R/"indra/newview/llviewermenu.cpp").read_text()
        assert "S8 mKAMIAliveOverride = 0;" in header
        assert 'sKAMIAliveOverride("kami_alive_override_mode")' in impl
        assert "shader->uniform1f(sKAMIAliveOverride, (F32)mKAMIAliveOverride);" in impl
        assert "void kami_alive_material_action(const std::string& action)" in menu
        assert "LLObjectSelectionHandle selection" in menu
        assert 'mat->mKAMIAliveOverride=1;' in menu
        assert 'mat->mKAMIAliveOverride=-1;' in menu
        assert 'mat->mKAMIAliveOverride=0;' in menu
        assert menu.count('commit.add("KAMI.Alive')==4
        print("PASS v0.2 integrated material inspection, local override and real PBR draw uniforms")
    else:
        print("PASS v0.2 installer Python syntax, native integration anchors and source safety checks")
