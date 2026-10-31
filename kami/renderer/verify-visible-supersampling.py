#!/usr/bin/env python3
"""KAMI post-processing source contract and real-resolution resolve policy tests.

--integrated inspects the final build-time-patched viewer source.
Shader GPU compilation/execution is separately tested with Mesa.
"""
import argparse
import py_compile
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
SCRIPT = R / "kami/renderer/fix-visible-supersampling.py"
SHADER = R / "indra/newview/app_settings/shaders/class1/deferred/postDeferredNoDoFF.glsl"

def grid(source_width, source_height, window_width, window_height):
    ratio = min(source_width / window_width, source_height / window_height)
    return 1 if ratio < 1.05 else (4 if ratio >= 2.75 else (3 if ratio >= 1.75 else 2))

def integrated():
    cpp = (R / "indra/newview/pipeline.cpp").read_text(encoding="utf-8")
    disp = (R / "indra/newview/llviewerdisplay.cpp").read_text(encoding="utf-8")
    for snippet in ("kami_present_resolution", "kami_resolve_grid",
                    "uniform1i(kami_grid, kami_samples_per_axis)",
                    "uniform2f(kami_present_res", "kami_ratio >= 2.75f",
                    "sourceBuffer->getWidth() / kami_out_width"):
        assert snippet in cpp, snippet
    assert "gDeferredPostNoDoFNoiseProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, sourceBuffer)" in cpp
    first = disp.index("gPipeline.mRT->deferredScreen.bindTarget();")
    after = disp.index("gPipeline.mRT->deferredScreen.clear();", first)
    assert first < disp.index("gGLViewport[2] = gPipeline.mRT->deferredScreen.getWidth();", first) < after
    assert first < disp.index("gGLViewport[3] = gPipeline.mRT->deferredScreen.getHeight();", first) < after
    restore = disp.index("Return all HUD, UI and monitor code to window-native pixel coordinates.")
    assert restore > disp.index("gPipeline.copyScreenSpaceReflections(&gPipeline.mRT->screen, &gPipeline.mSceneMap);")
    assert restore < disp.index("scene_monitor->capture();")
    assert "SSAA resolve." in disp and "One-to-one window output." in disp
    root = ET.parse(R / "indra/newview/app_settings/settings_blackdragon.xml").getroot().find("map")
    elements = list(root)
    modes = [elements[i+1] for i,el in enumerate(elements[:-1]) if el.tag=="key" and el.text=="KombatantRenderWidth"]
    assert len(modes)==1
    entry=list(modes[0])
    vals=[entry[i+1] for i,el in enumerate(entry[:-1]) if el.tag=="key" and el.text=="Value"]
    assert len(vals)==1 and vals[0].text=="2560", "RTX 5070 fresh default should be true QHD"
    gui = ET.parse(R / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
    selectors=[e for e in gui.iter() if e.get("name")=="kd_resolution"]
    assert len(selectors)==1
    options={e.get("value"):e.get("label") for e in selectors[0] if e.tag=="combo_box.item"}
    assert set(options)=={"0","2560","3840","7680"}
    assert "1440" in options["2560"] and "4K" in options["3840"] and "8K" in options["7680"]
    print("PASS actual viewport/FBO shader wiring, UI readout and QHD/4K/8K modes")

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--integrated", action="store_true")
    args=parser.parse_args()
    py_compile.compile(str(SCRIPT), doraise=True)
    source=SHADER.read_text()
    assert "uniform vec2 kami_present_resolution;" in source
    assert "uniform int kami_resolve_grid;" in source
    assert "vec4 diff = kamiResolveSupersampledWorld(vary_fragcoord.xy);" in source
    assert "vec2 jitter =" in source and "float(kami_resolve_grid)" in source
    assert grid(1920,1080,1920,1080)==1
    assert grid(2560,1440,2560,1440)==1
    assert grid(2560,1440,1920,1080)==2
    assert grid(3840,2160,1920,1080)==3
    assert grid(3840,2160,2560,1440)==2
    assert grid(7680,4320,1920,1080)==4
    assert grid(7680,4320,2560,1440)==4
    assert grid(1920,1080,2560,1440)==1
    print("PASS 1:1, 1440p, 4K and 8K supersample resolve grid policy")
    if args.integrated: integrated()
    print("GPU driver quality and full scene performance still require Windows tests")

if __name__ == "__main__":
    main()
