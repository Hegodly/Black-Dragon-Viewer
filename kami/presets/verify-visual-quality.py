#!/usr/bin/env python3
"""Validate native visible differences for the five KAMI quality presets."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
def read(name):
    return (ROOT / name).read_text(encoding="utf-8-sig")

shader = read("indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl")
pbr = read("kami/presets/pbr-fidelity.glsl")
pipeline = read("indra/newview/pipeline.cpp")
presets = read("kami/performance/kami_quality_presets.h")
compiled_presets = read("indra/newview/kami_quality_presets.h")

def check(name, ok):
    if not ok:
        raise AssertionError("KAMI visual presets: " + name)
    print("PASS", name)

check("PBR helper included once in native lighting",
      shader.count(pbr) == 1 and "uniform float kami_preset_material_quality;" in shader)
check("five distinct roughness floors and IBL strengths",
      all(f"return {v};" in pbr for v in
          ("0.34", "0.22", "0.13", "0.07", "0.02",
           "0.25", "0.55", "0.85", "1.0", "1.10")))
check("only PBR materials affected",
      shader.index("GBUFFER_FLAG_HAS_PBR") <
      shader.index("kamiQualityRoughnessFloor(kami_preset_material_quality)") <
      shader.index("kamiQualityProbeWeight(kami_preset_material_quality)") <
      shader.index("else if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_HDRI))"))
check("custom graphics quality leaves authored PBR intact",
      "if (kami_preset_material_quality >= 0.0)" in shader and
      'quality_value : -1.f' in pipeline)
check("renderer binding uses actual selected preset",
      'LLCachedControl<S32> kami_quality_index(gSavedSettings, "KAMIQualityPreset", -1)' in pipeline and
      'soften_shader.uniform1f(kami_preset_quality_uniform,' in pipeline)
check("automatic and performance controls bypass material tier",
      all(k in pipeline for k in
          ('"KAMIPerformanceMode"', '"KAMIPotatoMode"',
           '"KAMIAutoTune"', '"KAMICrowdMode"')))
check("independent GPU world resolution and FPS remain unchanged",
      all('set("' + k + '"' not in presets for k in (
          "RenderResolutionMultiplier", "KombatantRenderWidth", "FramePerSecondLimit")))
check("native preset controller keeps prior adaptive-physics integration",
      'set("RenderAvatarPhysicsLODFactor", physicsLOD[q]);' in compiled_presets and
      'static const F32 physicsLOD[]' in compiled_presets and
      'set("KAMIQualityVisualStatus", LLSD(visualStatus[q]));' in compiled_presets and
      'set("KAMIQualityVisualStatus", LLSD(visualStatus[q]));' in presets)
check("five quality levels and status messages",
      presets.count('"LOW: ') == 1 and
      presets.count('"MEDIUM: ') == 1 and
      presets.count('"HIGH: ') == 1 and
      presets.count('"ULTRA: ') == 1 and
      presets.count('"CINEMATIC: ') == 1 and
      'set("KAMIQualityVisualStatus", LLSD(visualStatus[q]));' in presets)
settings = ET.parse(ROOT / "indra/newview/app_settings/settings_blackdragon.xml").getroot()
ui = ET.parse(ROOT / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
keys = [x.text for x in settings.find("map") if x.tag == "key"]
check("one native quality status setting", keys.count("KAMIQualityVisualStatus") == 1)
named = {x.get("name"): x for x in ui.iter() if x.get("name")}
check("real live Display status bound and visible",
      named["kami_quality_live_status"].get("control_name") == "KAMIQualityVisualStatus" and
      int(named["kami_quality_live_status"].get("top")) +
      int(named["kami_quality_live_status"].get("height")) <=
      int(named["kombatant_display_quick"].get("height")) and
      int(named["gfx_scroll_panel"].get("height")) >
      int(named["kombatant_display_quick"].get("height")))
print("KAMI graphics presets now have distinct opt-in native PBR fidelity contracts.")
