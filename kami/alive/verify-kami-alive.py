#!/usr/bin/env python3
"""Verify KAMI ALIVE phase 1 integration, without claiming a GPU benchmark."""
import argparse
import ast
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
S = R / "kami/alive/apply-kami-alive.py"


def read(name):
    return (R / name).read_text(encoding="utf-8-sig")


def source():
    s = read("kami/alive/apply-kami-alive.py")
    ast.parse(s, filename=str(S))
    for phrase in (
        "KAMIAliveEnabled", "KAMIAliveStrength", "KAMIAliveFrameBudgetMs",
        "kami_alive_amount", "kamiAlivePolishedRoughness", "KAMI_Alive_Toggle",
        "updateKAMIAliveFrameBudget", "mKAMIAliveUniformAge",
        "gDeferredPBROpaqueProgram", "gGLTFPBRMetallicRoughnessProgram",
        'if (!for_snapshot)',
    ):
        assert phrase in s, f"Missing source integration: {phrase}"
    assert "glFinish(" not in s and "glReadPixels(" not in s
    print("PASS ALIVE source integration and nonblocking governor")


def integrated():
    settings = ET.parse(R / "indra/newview/app_settings/settings.xml").getroot()
    keys = settings.findall("./map/key")
    names = [k.text for k in keys]
    for name in ("KAMIAliveEnabled", "KAMIAliveStrength", "KAMIAliveFrameBudgetMs"):
        assert names.count(name) == 1, f"Duplicate/missing setting: {name}"
    assert names.index("KAMIAliveEnabled") < names.index("ImporterDebugVerboseLogging")
    assert "<key>KAMIAliveEnabled</key>" in read("indra/newview/app_settings/settings.xml")
    menu = ET.parse(R / "indra/newview/skins/default/xui/en/menu_viewer.xml").getroot()
    menus = [m for m in menu if m.get("name") == "KAMI_Workspaces"]
    assert len(menus) == 1
    checks = [m for m in menus[0] if m.get("name") == "KAMI_Alive_Toggle"]
    assert len(checks) == 1
    assert checks[0].find("menu_item_check.on_check").get("control") == "KAMIAliveEnabled"
    assert checks[0].find("menu_item_check.on_click").get("parameter") == "KAMIAliveEnabled"
    assert len([e for e in menu.iter("menu_item_check") if e.get("name")=="Kami DX12 Live Preview"]) == 1

    for path in (
        "indra/newview/app_settings/shaders/class1/deferred/pbropaqueF.glsl",
        "indra/newview/app_settings/shaders/class1/gltf/pbrmetallicroughnessF.glsl",
    ):
        shader = read(path)
        assert shader.count("uniform float kami_alive_amount;") == 1
        assert shader.count("kamiAlivePolishedRoughness(") == 2
        assert "float metal_weight" in shader and "float smooth_weight" in shader
        assert "random(" not in shader and "gl_FragCoord" not in shader
    gltf = read("indra/newview/app_settings/shaders/class1/gltf/pbrmetallicroughnessF.glsl")
    assert gltf.index("orm.g = kamiAlivePolishedRoughness") < gltf.index("#ifndef ALPHA_BLEND")
    mgr = read("indra/newview/llviewershadermgr.cpp")
    h = read("indra/newview/llviewershadermgr.h")
    display = read("indra/newview/llviewerdisplay.cpp")
    assert "void updateKAMIAliveFrameBudget(F32 frame_seconds);" in h
    assert "LLViewerShaderMgr::updateKAMIAliveFrameBudget(F32 frame_seconds)" in mgr
    assert "shader->uniform1f(sKAMIAliveAmount, mKAMIAliveUniform);" in mgr
    assert 'LLCachedControl<bool> enabled(gSavedSettings, "KAMIAliveEnabled", false)' in mgr
    assert "mKAMIAliveUniformAge < 0.25f" in mgr
    assert "mKAMIAliveAverageFrameMs" in mgr and "0.65f" in mgr
    assert "dirty(gDeferredPBROpaqueProgram);" in mgr
    assert "dirty(gGLTFPBRMetallicRoughnessProgram);" in mgr
    assert "LLViewerShaderMgr::instance()->updateKAMIAliveFrameBudget(" in display
    print("PASS ALIVE integrated settings, native toggle, 2 real PBR shader paths, 4 Hz adaptive governor")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--integrated", action="store_true")
    args = parser.parse_args()
    source()
    if args.integrated:
        integrated()
    else:
        print("Source preflight only; compilation and real in-world GPU/frame-time QA remain pending.")
