#!/usr/bin/env python3
"""Integrate KAMI ALIVE phase 1 into the native viewer.

This is intentionally an opt-in, conservative metallic-PBR response, not an
avatar material classifier, skin simulation, or new render backend. No extra
passes, textures, random temporal noise, GPU readbacks or pipeline stalls.
Run AFTER the KAMI Workspaces integration and BEFORE native compilation.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"
SHADERS = VIEW / "app_settings/shaders/class1"


def replace_once(path, old, new):
    source = path.read_text(encoding="utf-8-sig")
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"KAMI ALIVE integration anchor mismatch: {path} ({count} matches)")
    path.write_text(source.replace(old, new, 1), encoding="utf-8")


settings = VIEW / "app_settings/settings.xml"
settings_anchor = "  <key>ImporterDebugVerboseLogging</key>"
settings_entries = """  <key>KAMIAliveEnabled</key>
  <map>
    <key>Comment</key>
    <string>Opt-in local PBR material enhancement. Phase 1 selectively refines smooth metallic roughness. No SL asset or server modifications.</string>
    <key>Persist</key><integer>1</integer>
    <key>Type</key><string>Boolean</string>
    <key>Value</key><integer>0</integer>
  </map>
  <key>KAMIAliveStrength</key>
  <map>
    <key>Comment</key><string>Upper bound for KAMI ALIVE metallic response, from 0 to 1. Frame-time governor may reduce effective strength.</string>
    <key>Persist</key><integer>1</integer>
    <key>Type</key><string>F32</string>
    <key>Value</key><real>0.75</real>
  </map>
  <key>KAMIAliveFrameBudgetMs</key>
  <map>
    <key>Comment</key><string>Sustained CPU frame-time budget in milliseconds. Effect fades down smoothly beyond this budget. Not a GPU-time measurement or FPS guarantee.</string>
    <key>Persist</key><integer>1</integer>
    <key>Type</key><string>F32</string>
    <key>Value</key><real>16.7</real>
  </map>
"""
replace_once(settings, settings_anchor, settings_entries + settings_anchor)
ET.parse(settings)

menu = XUI / "menu_viewer.xml"
tree = ET.parse(menu)
bar = tree.getroot()
workspace = [e for e in bar if e.tag == "menu" and e.get("name") == "KAMI_Workspaces"]
if len(workspace) != 1:
    raise RuntimeError("Expected the single, installed native KAMI Workspaces menu")
workspace = workspace[0]
if any(e.get("name") == "KAMI_Alive_Toggle" for e in bar.iter()):
    raise RuntimeError("KAMI ALIVE menu already integrated")
toggle = ET.Element("menu_item_check", {
    "label": "KAMI ALIVE  |  MATERIAL RESPONSE (BETA)",
    "name": "KAMI_Alive_Toggle",
})
ET.SubElement(toggle, "menu_item_check.on_check", {"control": "KAMIAliveEnabled"})
ET.SubElement(toggle, "menu_item_check.on_click", {
    "function": "ToggleControl", "parameter": "KAMIAliveEnabled",
})
after = next((i for i, e in enumerate(workspace)
              if e.get("name") == "Kami.Hardware"), None)
if after is None:
    raise RuntimeError("KAMI Hardware entry missing from Workspaces")
workspace.insert(after + 1, toggle)
ET.indent(tree, space=" ")
tree.write(menu, encoding="utf-8", xml_declaration=True)
ET.parse(menu)

# Shared shader-side classifier, intentionally only PBR materials whose
# *existing material data* says metallic and smooth: no mistaken skin tags.
# No high-frequency noise, no frame time uniform and no additional samples.
polish = """
// KAMI ALIVE phase 1: deterministic microfacet response for polished metals.
uniform float kami_alive_amount;
float kamiAlivePolishedRoughness(float roughness, float metallic)
{
    float metal_weight = clamp((metallic - 0.68) * 3.125, 0.0, 1.0);
    float smooth_weight = clamp((0.48 - roughness) * 3.3333, 0.0, 1.0);
    float blend = metal_weight * smooth_weight * clamp(kami_alive_amount, 0.0, 1.0);
    float polished = min(roughness, max(0.07, roughness * 0.72));
    return mix(roughness, polished, blend);
}
"""
opaque = SHADERS / "deferred/pbropaqueF.glsl"
replace_once(opaque,
    "// deferred opaque implementation\n",
    "// deferred opaque implementation\n" + polish + "\n")
replace_once(opaque,
    "    spec.b *= metallicFactor;\n",
    "    spec.b *= metallicFactor;\n"
    "    spec.g = kamiAlivePolishedRoughness(spec.g, spec.b);\n")

gltf = SHADERS / "gltf/pbrmetallicroughnessF.glsl"
replace_once(gltf,
    "// GLTF pbrMetallicRoughness implementation\n",
    "// GLTF pbrMetallicRoughness implementation\n" + polish + "\n")
replace_once(gltf,
    "    orm.b *= metallicFactor;\n",
    "    orm.b *= metallicFactor;\n"
    "    orm.g = kamiAlivePolishedRoughness(orm.g, orm.b);\n")

shader_mgr_h = VIEW / "llviewershadermgr.h"
replace_once(shader_mgr_h,
    "    void setShaders();\n",
    "    void setShaders();\n"
    "    void updateKAMIAliveFrameBudget(F32 frame_seconds);\n")
replace_once(shader_mgr_h,
    "    std::vector<LLGLSLShader *> mShaderList;\n",
    """    std::vector<LLGLSLShader *> mShaderList;
    F32 mKAMIAliveAverageFrameMs = 16.7f;
    F32 mKAMIAliveGain = 0.f;
    F32 mKAMIAliveUniform = 0.f;
    F32 mKAMIAliveUniformAge = 0.f;
""")

shader_mgr = VIEW / "llviewershadermgr.cpp"
replace_once(shader_mgr,
    '#include "llviewershadermgr.h"\n',
    '#include "llviewershadermgr.h"\n#include <cmath>\n')

governor = """
// KAMI ALIVE frame-time governor. This samples CPU frame intervals, not
// GPU completion time; no glFinish, glReadPixels, fences or extra passes.
// Reduce effect more quickly than recovery. Limit uniform refresh to 4 Hz
// and only mark shaders that actually consume it.
void LLViewerShaderMgr::updateKAMIAliveFrameBudget(F32 frame_seconds)
{
    if (!std::isfinite(frame_seconds) || frame_seconds <= 0.f)
    {
        return;
    }

    static LLCachedControl<bool> enabled(gSavedSettings, "KAMIAliveEnabled", false);
    static LLCachedControl<F32> intensity(gSavedSettings, "KAMIAliveStrength", 0.75f);
    static LLCachedControl<F32> frame_budget(gSavedSettings, "KAMIAliveFrameBudgetMs", 16.7f);

    // Large hitches are capped for recovery stability; not hidden from the
    // next few frames of the moving average.
    const F32 dt = llclamp(frame_seconds, 0.f, 0.10f);
    const F32 frame_ms = dt * 1000.f;
    const F32 smoothing = llclamp(dt / 0.65f, 0.f, 1.f);
    mKAMIAliveAverageFrameMs +=
        (frame_ms - mKAMIAliveAverageFrameMs) * smoothing;

    const F32 limit = llclamp((F32)frame_budget, 8.f, 45.f);
    const F32 headroom = llclamp(
        (limit * 1.27f - mKAMIAliveAverageFrameMs) / (limit * 0.27f),
        0.f, 1.f);
    const F32 wanted = enabled ? headroom : 0.f;

    // Hysteretic rate limiter: back off in ~0.6s, recover over ~2.2s.
    const F32 speed = wanted < mKAMIAliveGain ? 1.8f : 0.45f;
    const F32 step = dt * speed;
    mKAMIAliveGain += llclamp(wanted - mKAMIAliveGain, -step, step);

    mKAMIAliveUniformAge += dt;
    const F32 applied = enabled
        ? mKAMIAliveGain * llclamp((F32)intensity, 0.f, 1.f)
        : 0.f;

    if ((mKAMIAliveUniformAge < 0.25f && enabled) ||
        (std::fabs(applied - mKAMIAliveUniform) < 0.025f &&
         !( !enabled && mKAMIAliveUniform > 0.f )))
    {
        return;
    }

    mKAMIAliveUniformAge = 0.f;
    mKAMIAliveUniform = applied;

    auto dirty = [](LLGLSLShader& shader) {
        shader.mUniformsDirty = true;
        if (shader.mRiggedVariant)
        {
            shader.mRiggedVariant->mUniformsDirty = true;
        }
        for (auto& variant : shader.mGLTFVariants)
        {
            variant.mUniformsDirty = true;
        }
    };
    dirty(gDeferredPBROpaqueProgram);
    dirty(gGLTFPBRMetallicRoughnessProgram);
}

"""
replace_once(shader_mgr,
    "void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)\n",
    governor + "void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)\n")
replace_once(shader_mgr,
    "    LLEnvironment::instance().updateShaderUniforms(shader);\n",
    """    LLEnvironment::instance().updateShaderUniforms(shader);
    static LLStaticHashedString sKAMIAliveAmount("kami_alive_amount");
    shader->uniform1f(sKAMIAliveAmount, mKAMIAliveUniform);
""")

display = VIEW / "llviewerdisplay.cpp"
replace_once(display,
    '    LLPerfStats::RecordSceneTime T (LLPerfStats::StatType_t::RENDER_DISPLAY);',
    '''    if (!for_snapshot)
    {
        LLViewerShaderMgr::instance()->updateKAMIAliveFrameBudget(
            gFrameIntervalSeconds.value());
    }

    LLPerfStats::RecordSceneTime T (LLPerfStats::StatType_t::RENDER_DISPLAY);''')

print("KAMI ALIVE phase 1 integrated: PBR metal response, opt-in menu, 4 Hz adaptive frame-time governor.")
