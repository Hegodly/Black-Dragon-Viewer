#!/usr/bin/env python3
"""Experimental KAMI TRUE PBR OFF world-material routing, not a vintage effects preset.

Switch FULL PBR -> LEGACY surfaces by bypassing glTF material draw batches
and rendering the original texture entry's pre-PBR diffuse/material fallback.
PBR-only glTF scenes with no old mesh representation can't be fabricated:
they are not drawn in OFF mode. PBR-only terrain uses old texture shader
and may look blank without legacy terrain detail textures.

This script runs during KAMI's Windows build. Never runs on users' PCs.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
VIEW=R/"indra/newview"
SETTINGS=VIEW/"app_settings/settings_blackdragon.xml"
CTRL=VIEW/"llviewercontrol.cpp"
VOLUME=VIEW/"llvovolume.cpp"
PIPE=VIEW/"pipeline.cpp"
ALPHA=VIEW/"lldrawpoolalpha.cpp"
TERRAIN=VIEW/"lldrawpoolterrain.cpp"
UI=VIEW/"skins/default/xui/en/panel_preferences_graphics1.xml"

def replace_one(path,a,b):
    content=path.read_text(encoding="utf-8-sig")
    n=content.count(a)
    if n!=1: raise RuntimeError(f"{path}: anchor count {n}: {a[:100]}")
    path.write_text(content.replace(a,b,1),encoding="utf-8")

def replace_n(path,a,b,count):
    content=path.read_text(encoding="utf-8-sig")
    n=content.count(a)
    if n!=count: raise RuntimeError(f"{path}: expected {count} occurrences, got {n}: {a[:100]}")
    path.write_text(content.replace(a,b),encoding="utf-8")

# New persisted ON default; previous reduced mode script is deliberately not
# installed in this branch. Eight backups preserve HDR, reflection choices.
root=ET.parse(SETTINGS)
settings=root.getroot().find("map")
if settings is None: raise RuntimeError("Expected settings map")
entries=[
("KAMIPBREnabled","1","KAMI: true PBR glTF material drawing ON (default) or legacy fallback OFF."),
("KAMIPBROffBackupValid","0","An earlier PBR-on GPU setting snapshot is available."),
("KAMIPBROffVintage","1","Saved modern lighting preference."),
("KAMIPBROffHDR","1","Saved HDR framebuffer preference."),
("KAMIPBROffEmissive","1","Saved emissive Gbuffer preference."),
("KAMIPBROffProbes","1","Saved reflection probe preference."),
("KAMIPBROffSSR","0","Saved screen space reflection preference."),
("KAMIPBROffMirrors","0","Saved mirror pass preference."),
]
present={e.text for e in settings if e.tag=="key"}
for key,value,description in entries:
    if key in present: raise RuntimeError(f"Setting already exists {key}")
    ET.SubElement(settings,"key").text=key
    m=ET.SubElement(settings,"map")
    for key0,val,tag in (("Comment",description,"string"),
                         ("Persist","1","integer"),
                         ("Type","Boolean","string"),
                         ("Value",value,"integer")):
        ET.SubElement(m,"key").text=key0
        ET.SubElement(m,tag).text=val
ET.indent(root,space=" ")
root.write(SETTINGS,encoding="utf-8",xml_declaration=True)

# Every subsequent scene batch uses the toggled PBR preference. OFF is NOT
# implemented by hiding a rendered image or replacing it with a filter.
replace_one(VOLUME,
'''    if (facep->getTextureEntry()->getGLTFRenderMaterial() != nullptr)
    { // PBR materials break indexed texture batching''',
'''    if (gSavedSettings.getBOOL("KAMIPBREnabled") &&
        facep->getTextureEntry()->getGLTFRenderMaterial() != nullptr)
    { // PBR materials break indexed texture batching''')
replace_one(VOLUME,
'''    auto* gltf_mat = (LLFetchedGLTFMaterial*)te->getGLTFRenderMaterial();
    llassert(gltf_mat == nullptr || dynamic_cast<LLFetchedGLTFMaterial*>(te->getGLTFRenderMaterial()) != nullptr);''',
'''    auto* gltf_mat = gSavedSettings.getBOOL("KAMIPBREnabled") ?
        (LLFetchedGLTFMaterial*)te->getGLTFRenderMaterial() : nullptr;
    llassert(gltf_mat == nullptr || dynamic_cast<LLFetchedGLTFMaterial*>(te->getGLTFRenderMaterial()) != nullptr);''')
replace_one(VOLUME,
'''                    gltf_mat = (LLFetchedGLTFMaterial*)te->getGLTFRenderMaterial();
                } // if not te, continue?
                bool is_pbr = gltf_mat != nullptr;

                if (is_pbr)''',
'''                    gltf_mat = gSavedSettings.getBOOL("KAMIPBREnabled") ?
                        (LLFetchedGLTFMaterial*)te->getGLTFRenderMaterial() : nullptr;
                } // if not te, continue?
                bool is_pbr = gltf_mat != nullptr;

                if (!is_pbr && te && te->getGLTFRenderMaterial())
                {
                    // This face had a glTF material before the mode switch.
                    // Rebind the *original* SL diffuse texture, not a PBR
                    // basecolor, without mutating TE or simulator materials.
                    facep->setTexture(LLRender::DIFFUSE_MAP, vobj->getTEImage(i));
                    facep->setTexture(LLRender::BASECOLOR_MAP, nullptr);
                    facep->setTexture(LLRender::GLTF_NORMAL_MAP, nullptr);
                    facep->setTexture(LLRender::METALLIC_ROUGHNESS_MAP, nullptr);
                    facep->setTexture(LLRender::EMISSIVE_MAP, nullptr);
                }
                if (is_pbr)''')
# In OFF, use pre-PBR alpha, fullbright, normal and specular pool selection.
replace_n(VOLUME,
'''LLGLTFMaterial* gltf_mat = te->getGLTFRenderMaterial();''',
'''LLGLTFMaterial* gltf_mat = gSavedSettings.getBOOL("KAMIPBREnabled") ?
                                te->getGLTFRenderMaterial() : nullptr;''',2)

replace_one(PIPE,
'''    LLGLTFMaterial* gltf_mat = te->getGLTFRenderMaterial();

    bool color_alpha =''',
'''    LLGLTFMaterial* gltf_mat = gSavedSettings.getBOOL("KAMIPBREnabled") ?
        te->getGLTFRenderMaterial() : nullptr;

    bool color_alpha =''')
replace_one(PIPE,
'''void LLPipeline::renderGLTFObjects(U32 type, bool texture, bool rigged)
{
    assertInitialized();''',
'''void LLPipeline::renderGLTFObjects(U32 type, bool texture, bool rigged)
{
    if (!gSavedSettings.getBOOL("KAMIPBREnabled"))
        return; // No glTF/PBR GPU draw in true OFF mode.
    assertInitialized();''')
replace_one(ALPHA,
'''    if (rigged && mType == LLDrawPool::POOL_ALPHA_POST_WATER)
    { // draw GLTF scene to depth buffer before rigged alpha''',
'''    if (gSavedSettings.getBOOL("KAMIPBREnabled") &&
        rigged && mType == LLDrawPool::POOL_ALPHA_POST_WATER)
    { // draw GLTF scene to depth buffer before rigged alpha''')
# Legacy terrain fallback path, even when the sim only has PBR terrain.
# Some regions will have no old textures and this can look blank, which is
# preferable to secretly continuing to shade PBR despite user OFF.
replace_one(TERRAIN,
'''    if (use_textures)
    {
        // Use textures''',
'''    if (use_textures || !gSavedSettings.getBOOL("KAMIPBREnabled"))
    {
        // Use pre-PBR textures if available (PBR-only terrain can have none).''')

# Runtime OFF genuinely changes material paths. Save all adjacent renderer
# values, preserve previous state, flush FBO/shaders using existing handlers,
# and invalidate visible geometry for legacy and GLTF materials.
const_marker="static bool handleEnableHDR(const LLSD& newvalue)\n{"
cpp=r'''// KAMI: TRUE PBR OFF means no glTF material draw passes. We preserve every
// original user material in the simulator and rebuild legacy TE batches.
static bool handleKAMIPBREnabled(const LLSD& newvalue)
{
    const bool on = newvalue.asBoolean();
    if (!on)
    {
        if (!gSavedSettings.getBOOL("KAMIPBROffBackupValid"))
        {
            gSavedSettings.setBOOL("KAMIPBROffVintage",
                gSavedSettings.getBOOL("RenderDisableVintageMode"));
            gSavedSettings.setBOOL("KAMIPBROffHDR",
                gSavedSettings.getBOOL("RenderHDREnabled"));
            gSavedSettings.setBOOL("KAMIPBROffEmissive",
                gSavedSettings.getBOOL("RenderEnableEmissiveBuffer"));
            gSavedSettings.setBOOL("KAMIPBROffProbes",
                gSavedSettings.getBOOL("RenderReflectionsEnabled"));
            gSavedSettings.setBOOL("KAMIPBROffSSR",
                gSavedSettings.getBOOL("RenderScreenSpaceReflections"));
            gSavedSettings.setBOOL("KAMIPBROffMirrors",
                gSavedSettings.getBOOL("RenderMirrors"));
            gSavedSettings.setBOOL("KAMIPBROffBackupValid", true);
        }
        gSavedSettings.setBOOL("RenderScreenSpaceReflections", false);
        gSavedSettings.setBOOL("RenderMirrors", false);
        gSavedSettings.setBOOL("RenderReflectionsEnabled", false);
        gSavedSettings.setBOOL("RenderDisableVintageMode", false);
        gSavedSettings.setBOOL("RenderHDREnabled", false);
        gSavedSettings.setBOOL("RenderEnableEmissiveBuffer", false);
    }
    else if (gSavedSettings.getBOOL("KAMIPBROffBackupValid"))
    {
        gSavedSettings.setBOOL("RenderDisableVintageMode",
            gSavedSettings.getBOOL("KAMIPBROffVintage"));
        gSavedSettings.setBOOL("RenderHDREnabled",
            gSavedSettings.getBOOL("KAMIPBROffHDR"));
        gSavedSettings.setBOOL("RenderEnableEmissiveBuffer",
            gSavedSettings.getBOOL("KAMIPBROffEmissive"));
        gSavedSettings.setBOOL("RenderReflectionsEnabled",
            gSavedSettings.getBOOL("KAMIPBROffProbes"));
        gSavedSettings.setBOOL("RenderScreenSpaceReflections",
            gSavedSettings.getBOOL("KAMIPBROffSSR"));
        gSavedSettings.setBOOL("RenderMirrors",
            gSavedSettings.getBOOL("KAMIPBROffMirrors"));
        gSavedSettings.setBOOL("KAMIPBROffBackupValid", false);
    }
    if (gPipeline.isInit())
    {
        // Reclassifies the actual visible mesh batches into legacy glTF-free
        // passes and updates terrain. Requires a full rebuild both directions.
        gPipeline.rebuildDrawInfo();
        gPipeline.rebuildTerrain();
    }
    return true;
}

static bool handleEnableHDR(const LLSD& newvalue)
{'''
replace_one(CTRL,const_marker,cpp)
replace_one(CTRL,
'''    setting_setup_signal_listener(gSavedSettings, "RenderDisableVintageMode", handleDisableVintageMode);''',
'''    setting_setup_signal_listener(gSavedSettings, "RenderDisableVintageMode", handleDisableVintageMode);
    setting_setup_signal_listener(gSavedSettings, "KAMIPBREnabled", handleKAMIPBREnabled);''')

ui=ET.parse(UI)
u=ui.getroot()
vintage=[n for n in u.iter("check_box") if n.get("name")=="RenderDisableVintageMode"]
if len(vintage)!=1:raise RuntimeError("No vintage checkbox anchor")
parent=[p for p in u.iter() if vintage[0] in list(p)]
if len(parent)!=1:raise RuntimeError("Bad vintage UI parent")
p=parent[0];index=list(p).index(vintage[0])
p.insert(index,ET.Element("check_box", {
    "name":"kami_pbr_enabled", "control_name":"KAMIPBREnabled",
    "initial_value":"true", "height":"25", "layout":"topleft",
    "left":"10", "top_pad":"5", "width":"430",
    "label":"PBR Rendering: ON / OFF",
    "tool_tip":(
      "ON: original full glTF/PBR material rendering. "
      "OFF: bypass glTF PBR surface draw paths and use pre-PBR diffuse/legacy "
      "materials on existing prim and mesh faces. Disable PBR reflections, HDR "
      "and emissive buffers. OFF is NOT the old Vintage effect. "
      "PBR-only glTF scene objects with no old mesh counterpart may disappear. "
      "PBR-only terrain may lack old textures. Geometry and materials are "
      "never changed on the simulator. On restores original settings. "
      "GPU FPS improvement depends on content and hardware."),
}))
vintage[0].set("visible","false")
ET.indent(ui,space=" ")
ui.write(UI,encoding="utf-8",xml_declaration=True)
ET.parse(UI)
print("TRUE PBR OFF is now a glTF material/terrain render-route switch, not vintage effects.")
