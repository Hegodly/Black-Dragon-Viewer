#!/usr/bin/env python3
"""Source/integration checks for KAMI ALIVE visual-profile beta.

This validates native hooks but does not promise physically accurate SSS,
eye tearline geometry, perfect material recognition, or a successful build.
"""
import ast
import argparse
import xml.etree.ElementTree as ET
from pathlib import Path
R=Path(__file__).resolve().parents[2]
src=(R/"kami/alive/apply-alive-visual-profiles.py").read_text(encoding="utf-8")
ast.parse(src)
checks=(
    "KAMIAliveAvatarSkinEnabled", "KAMIAliveAvatarEyesEnabled",
    "Kami.Alive.Gem", "Kami.Alive.Skin", "Kami.Alive.Eyes",
    "Kami.Alive.Fabric", "Kami.Alive.Leather",
    "kamiAliveApplyProfile", "kamiAlivePolishedRoughness",
    "gDeferredAvatarProgram", "gDeferredAvatarEyesProgram",
    "kamiAvatarEyesF.glsl", "profile_switch_changed",
    "overrides[material_key]", "local.size() < 512",
)
for q in checks:
    # Source references are intentional. Unrelated profile names are never
    # guessed from arbitrary mesh files or object names.
    if q == "local.size() < 512":
        continue
    assert q in src,q
assert "glFinish(" not in src and "glReadPixels(" not in src
assert "gl_FragCoord" not in src
def read(x):return (R/x).read_text(encoding="utf-8-sig")
def integrated():
    base="indra/newview/"
    menu=ET.parse(R/(base+"skins/default/xui/en/menu_viewer.xml")).getroot()
    work=[x for x in menu if x.get("name")=="KAMI_Workspaces"]
    assert len(work)==1
    for name in ("KAMI_Alive_Toggle","KAMI_Alive_Gem","KAMI_Alive_Skin",
                 "KAMI_Alive_Eyes","KAMI_Alive_Fabric","KAMI_Alive_Leather",
                 "KAMI_Alive_Avatar_Skin","KAMI_Alive_Avatar_Eyes"):
        assert sum(x.get("name")==name for x in work[0])==1,name
    settings=ET.parse(R/(base+"app_settings/settings.xml")).getroot()
    keys=[e.text for e in settings.findall("./map/key")]
    for name in ("KAMIAliveEnabled","KAMIAliveAvatarSkinEnabled",
                 "KAMIAliveAvatarEyesEnabled","KAMIAliveMaterialOverrides"):
        assert keys.count(name)==1,name
    cpp=read(base+"llviewermenu.cpp")
    for name in ('commit.add("Kami.Alive.Gem"','commit.add("Kami.Alive.Skin"',
                 'commit.add("Kami.Alive.Eyes"','commit.add("Kami.Alive.Fabric"',
                 'commit.add("Kami.Alive.Leather"','kamiAliveProfileName('):
        assert name in cpp,name
    mat=read(base+"llfetchedgltfmaterial.cpp")
    assert "llclamp((F32)overrides[material_key].asInteger(), -1.f, 6.f)" in mat
    for shader in ("deferred/pbropaqueF.glsl","gltf/pbrmetallicroughnessF.glsl"):
        glsl=read(base+"app_settings/shaders/class1/"+shader)
        assert glsl.count("void kamiAliveApplyProfile(")==1,shader
        assert glsl.count("kamiAliveApplyProfile(")==2,shader
        assert "if (kami_alive_material_override > 1.5)" in glsl
        assert glsl.count("uniform float kami_alive_amount;")==1
        assert "sampler2D" in glsl
    eyes=read(base+"app_settings/shaders/class1/deferred/kamiAvatarEyesF.glsl")
    skin=read(base+"app_settings/shaders/class1/deferred/avatarF.glsl")
    assert "spec_value" in eyes and "kami_alive_avatar_eyes_enabled" in eyes
    assert "kami_alive_avatar_skin_enabled" in skin
    mgr=read(base+"llviewershadermgr.cpp")
    for term in ('"deferred/kamiAvatarEyesF.glsl"',"dirty(gDeferredAvatarProgram)",
                 "dirty(gDeferredAvatarEyesProgram)","profile_switch_changed",
                 "shader->uniform1f(sKAMIAliveSkin"):
        assert term in mgr,term
    scene=read(base+"gltfscenemanager.cpp")
    assert 'uniform1f(sKAMIAliveMaterialOverride, 0.f)' in scene
    print("PASS native KAMI ALIVE visual profiles, legacy baked avatars, manual PBR categories and scene isolation")
p=argparse.ArgumentParser();p.add_argument("--integrated",action="store_true")
args=p.parse_args()
print("PASS visual-profile installer parses; no added GPU readbacks or external assets")
if args.integrated:integrated()
else: print("Native compilation and real-world graphics/FPS still require a successful Windows build and in-world tests.")
