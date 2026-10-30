#!/usr/bin/env python3
"""Contracts for three real KAMI Dark Cinematic paths (not a GPU FPS test)."""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def read(path):return (R/path).read_text(encoding="utf-8-sig")
def test(name,ok):
    if not ok:raise AssertionError("Dark Cinematic: "+name)
    print("PASS:",name)

shader=read("indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl")
pipeline=read("indra/newview/pipeline.cpp")
atmos=read("indra/newview/kami_atmosphere.h")
cine=read("indra/newview/kami_cinematic.h")
prefs=read("indra/newview/llfloaterpreference.cpp")
avatar=read("indra/newview/llvoavatar.cpp")
control=read("indra/newview/llviewercontrol.cpp")
settings=ET.parse(R/"indra/newview/app_settings/settings_blackdragon.xml").getroot().find("map")
ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
names={n.get("name"):n for n in ui.iter() if n.get("name")}
keys=[n.text for n in settings if n.tag=="key"]

test("unique opt-in material and status settings",
     keys.count("KAMICinematicMaterials")==1 and
     keys.count("KAMICinematicStatus")==1)
test("PBR enhancement reads real glTF material roughness",
     "GBUFFER_FLAG_HAS_PBR" in shader and
     "float perceptualRoughness = orm.g;" in shader and
     "clamp(perceptualRoughness * 0.93 + 0.018, 0.06, 1.0)" in shader)
test("PBR material remap bypasses non-PBR geometry",
     shader.index("GBUFFER_FLAG_HAS_PBR") <
     shader.index("float perceptualRoughness = orm.g;") <
     shader.index("else if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_HDRI))"))
test("existing deferred lighting pass binds runtime PBR intensity",
     'uniform float kami_cinematic_material;' in shader and
     'soften_shader.uniform1f(kami_material_uniform,' in pipeline and
     '"KAMICinematicMaterials"' in pipeline)
test("material effect obeys Potato/Performance",
     'gSavedSettings.getBOOL("KAMIPotatoMode")' in pipeline and
     'gSavedSettings.getBOOL("KAMIPerformanceMode")' in pipeline)
test("directional EEP is actual local viewer lighting",
     'mood=="Dark Cinematic"' in atmos and
     'ambient=color(0.31f,0.35f,0.43f);' in atmos and
     'setSkyRotation(sky,tilt,yaw);' in atmos and
     'environment.setEnvironment(LLEnvironment::ENV_LOCAL,sky)' in atmos)
test("legacy four working Atmosphere presets retained",
     all(m in atmos for m in ('Night Studio','Golden Hour',
                             'Overcast Cinema','Luxury Interior')))
test("cinematic preview uses EEP clone with restore and takeover protection",
     'KAMIAtmosphere::apply();' in cine and
     'KAMIAtmosphere::restore();' in cine and
     'KAMIAtmosphere::active()' in cine and
     'ownLocal' in atmos)
test("avatar live LOD and native crowd FPS are used",
     'gSavedSettings.setBOOL("KAMIAvatarQualityFPS", true);' in cine and
     'mAdjustedPixelArea = llmax(mAdjustedPixelArea, 262144.f);' in avatar and
     'sMaxNonImpostors = 8;' in avatar and
     'handleKAMIAvatarQualityChanged' in control)
test("preview and restoration are real native button actions",
     all(n in names and names[n].tag=="button" and
         ('"'+n+'"') in prefs
         for n in ('kami_cine_apply','kami_cine_restore')) and
     'KAMICinematic::apply();' in prefs and
     'KAMICinematic::restore();' in prefs)
quick=names["kombatant_display_quick"]
stack=names["gfx_stack"]
scroll=names["gfx_scroll_panel"]
children=[n for n in stack if n.tag=="layout_panel"]
test("cinematic UI appears after avatar and recovery, entirely visible",
     int(names["kami_cine_heading"].get("top")) >
     int(names["kami_avatar_quality_heading"].get("top")) >
     int(names["kami_recovery_status"].get("top")) and
     int(names["kami_cine_status"].get("top"))+
     int(names["kami_cine_status"].get("height")) <= int(quick.get("height")))
test("full native graphics scroll remains reachable",
     int(scroll.get("height")) >= int(quick.get("height"))+66 and
     int(stack.get("height")) >=
     sum(int(n.get("height","0")) for n in children)+64 and
     {"basic_layout_panel","advanced_layout_panel","finetuning_layout_panel"} <=
     {n.get("name") for n in children})
test("no temporal, ray, extra pass or FPS cap in cinematic controller",
     all(k not in cine for k in (
         'KombatantRayAO','KombatantUpscalerProvider',
         'FramePerSecondLimit','RenderResolutionMultiplier',
         'RenderShadowResolutionScale','RenderGlow')))
test("source and installed native cinematic engine identical",
     cine==read("kami/cinematic/kami_cinematic.h"))
test("user-facing cinematic feature manifest shipped",
     (R/"indra/newview/skins/default/KAMI-DARK-CINEMATIC.txt").is_file())
print("Dark Cinematic static checks passed. Native build and in-world image/FPS tests still needed.")
