#!/usr/bin/env python3
"""KAMI ALIVE v0.2 - explicit, user-triggered local material inspection.

Native menu actions inspect loaded PBR texture entries on a selected object
and allow an in-memory preview/bypass on its loaded PBR material instance.
No automatic inventory scan, new texture download, network call or render pass.
Applied after kami/alive/apply-kami-alive.py on Windows CI.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
VIEW=ROOT/"indra/newview"
XUI=VIEW/"skins/default/xui/en/menu_viewer.xml"
SHADERS=VIEW/"app_settings/shaders/class1"

def patch(path,old,new):
    text=path.read_text(encoding="utf-8-sig")
    count=text.count(old)
    if count!=1:
        raise RuntimeError(f"KAMI ALIVE material inspector anchor mismatch {path}: {count}")
    path.write_text(text.replace(old,new,1),encoding="utf-8")

# Preserve the working KAMI Workspaces menu, including the phase-1 toggle.
tree=ET.parse(XUI)
root=tree.getroot()
workspaces=[m for m in root if m.tag=="menu" and m.get("name")=="KAMI_Workspaces"]
if len(workspaces)!=1:
    raise RuntimeError("KAMI Workspaces menu must be installed before Material Inspector")
workspace=workspaces[0]
if any(x.get("name")=="KAMI_Alive_Inspect" for x in root.iter()):
    raise RuntimeError("KAMI ALIVE Material Inspector already integrated")
actions=[
    ("KAMI_Alive_Inspect","Check selected material compatibility","KAMI.AliveInspect"),
    ("KAMI_Alive_Metal","Preview selected PBR as polished metal","KAMI.AliveMetal"),
    ("KAMI_Alive_Bypass","Exclude selected PBR from ALIVE","KAMI.AliveBypass"),
    ("KAMI_Alive_Auto","Restore automatic material response","KAMI.AliveAuto"),
]
toggle=[i for i,e in enumerate(workspace) if e.get("name")=="KAMI_Alive_Toggle"]
if len(toggle)!=1:
    raise RuntimeError("KAMI ALIVE beta menu toggle missing")
position=toggle[0]+1
for name,label,callback in actions:
    item=ET.Element("menu_item_call", {"name":name,"label":label,"layout":"topleft"})
    ET.SubElement(item,"menu_item_call.on_click",{"function":callback})
    workspace.insert(position,item)
    position+=1
ET.indent(tree,space=" ")
tree.write(XUI,encoding="utf-8",xml_declaration=True)
ET.parse(XUI)

# The extra scalar is set from the material's *existing draw bind*. It is
# not a shader recompile, geometry/texture mutation or per-frame scan.
material_h=VIEW/"llfetchedgltfmaterial.h"
patch(material_h,
    "    void bind(LLViewerTexture* media_tex = nullptr);\n",
    """    void bind(LLViewerTexture* media_tex = nullptr);

    // KAMI ALIVE in-memory preview for this loaded PBR instance.
    // 0=automatic, +1=polished-metal preview, -1=exclude.
    // May affect other faces sharing this material instance. Never serialized.
    S8 mKAMIAliveOverride = 0;
""")
material_cpp=VIEW/"llfetchedgltfmaterial.cpp"
patch(material_cpp,
    "    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;\n",
    """    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    // Existing PBR batch material binding - no new draw calls/passes.
    static LLStaticHashedString sKAMIAliveOverride("kami_alive_override_mode");
    shader->uniform1f(sKAMIAliveOverride, (F32)mKAMIAliveOverride);
""")

# A deterministic, bounded material response; non-PBR remains untouched.
# Phase-1 automatic factors are still respected. Opt-in override can preview
# a metal response even if factors would make automatic matching ambiguous.
old_shader="""uniform float kami_alive_amount;
float kamiAlivePolishedRoughness(float roughness, float metallic)
{
    float metal_weight = clamp((metallic - 0.68) * 3.125, 0.0, 1.0);
    float smooth_weight = clamp((0.48 - roughness) * 3.3333, 0.0, 1.0);
    float blend = metal_weight * smooth_weight * clamp(kami_alive_amount, 0.0, 1.0);
    float polished = min(roughness, max(0.07, roughness * 0.72));
    return mix(roughness, polished, blend);
}
"""
new_shader="""uniform float kami_alive_amount;
uniform float kami_alive_override_mode;
void kamiAliveAdaptPBR(inout vec3 orm)
{
    float gain = clamp(kami_alive_amount, 0.0, 1.0);
    // -1 means no ALIVE treatment, even when automatic factors match.
    if (gain < 0.001 || kami_alive_override_mode < -0.5)
        return;

    float forced_metal = step(0.5, kami_alive_override_mode);
    float auto_metal = clamp((orm.b - 0.68) * 3.125, 0.0, 1.0);
    float auto_smooth = clamp((0.48 - orm.g) * 3.3333, 0.0, 1.0);
    float weight = gain * mix(auto_metal * auto_smooth, 1.0, forced_metal);
    float polished = min(orm.g, max(0.07, orm.g * 0.72));
    orm.g = mix(orm.g, polished, weight);
    // Only the user-directed preview can nudge the original metallic
    // response; default detection never changes the metallic channel.
    orm.b = mix(orm.b, max(orm.b, 0.82), forced_metal * gain * 0.9);
}
"""
for file_name in ("deferred/pbropaqueF.glsl","gltf/pbrmetallicroughnessF.glsl"):
    path=SHADERS/file_name
    patch(path,old_shader,new_shader)
    if "pbropaque" in file_name:
        patch(path,"    spec.g = kamiAlivePolishedRoughness(spec.g, spec.b);\n",
              "    kamiAliveAdaptPBR(spec);\n")
    else:
        patch(path,"    orm.g = kamiAlivePolishedRoughness(orm.g, orm.b);\n",
              "    kamiAliveAdaptPBR(orm);\n")

menu=VIEW/"llviewermenu.cpp"
patch(menu,
    '#include "llfloaterreg.h"\n',
    '#include "llfloaterreg.h"\n#include "llfetchedgltfmaterial.h" // KAMI ALIVE local material inspector\n')

# The inspector is triggered by user interaction only; capped at 8 selected
# objects/64 faces to protect busy regions. TE factors are inexpensive CPU
# metadata, but an ORM texture can vary per pixel: do not say "guaranteed."
inspector=r"""
// KAMI ALIVE v0.2. Local, selection-driven material inspector.
// Deliberately no inventory traversal, GPU readback, texture download or
// simulator updates. An override is held only on the fetched material in RAM.
void kami_alive_material_action(const std::string& action)
{
    LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
    S32 seen_objects=0, seen_faces=0, pbr_faces=0, candidates=0;
    S32 uncertain=0, standard=0, changed=0, pending=0;
    std::string report = "KAMI ALIVE | Selected Material Inspector\n";

    for (LLObjectSelection::valid_iterator it=selection->valid_begin();
         it!=selection->valid_end() && seen_objects<8 && seen_faces<64; ++it)
    {
        LLSelectNode* node=*it;
        LLViewerObject* object=node ? node->getObject() : nullptr;
        if (!object || object->isDead()) continue;
        ++seen_objects;
        const S32 nfaces=llmin((S32)object->getNumTEs(), 255);
        bool has_face_mask=false;
        for (S32 face=0;face<nfaces;++face)
        {
            if (node->isTESelected((U8)face)) {has_face_mask=true;break;}
        }
        for (S32 face=0;face<nfaces && seen_faces<64;++face)
        {
            if (has_face_mask && !node->isTESelected((U8)face)) continue;
            ++seen_faces;
            LLTextureEntry* te=object->getTE((U8)face);
            LLFetchedGLTFMaterial* mat=te
                ? dynamic_cast<LLFetchedGLTFMaterial*>(te->getGLTFRenderMaterial())
                : nullptr;
            if (!mat)
            {
                if (object->getRenderMaterialID((U8)face).notNull()) ++pending;
                else ++standard;
                continue;
            }

            ++pbr_faces;
            const bool orm=mat->mMetallicRoughnessTexture.notNull();
            const bool metal_factor=(mat->mMetallicFactor>0.68f);
            const bool smooth_factor=(mat->mRoughnessFactor<0.48f);
            // ORM textures can change roughness/metal per texel. Factor-only
            // screening cannot guarantee a particular pixel will qualify.
            const bool candidate=(metal_factor && smooth_factor);
            if (candidate) ++candidates;
            else if (metal_factor && orm) ++uncertain;

            if (action=="metal") {mat->mKAMIAliveOverride=1;++changed;}
            else if (action=="bypass") {mat->mKAMIAliveOverride=-1;++changed;}
            else if (action=="auto") {mat->mKAMIAliveOverride=0;++changed;}

            if (action=="inspect" && pbr_faces<=12)
            {
                const char* eligibility =
                    mat->mKAMIAliveOverride<0 ? "Excluded manually" :
                    mat->mKAMIAliveOverride>0 ? "Metal preview selected" :
                    candidate ? (orm ? "Candidate (ORM texel dependent)" : "Eligible by factors") :
                    (metal_factor && orm ? "Possible (ORM-dependent)" : "Standard PBR");
                report += llformat("Face %d: metal %.2f / rough %.2f | %s\n",
                     face, mat->mMetallicFactor, mat->mRoughnessFactor,
                     eligibility);
            }
        }
    }
    if (seen_faces==0)
    {
        report += "Select an object or individual face in Edit first.\n";
    }
    else
    {
        report += llformat("Inspected %d faces (%d PBR, %d standard, %d loading).\n",
                           seen_faces,pbr_faces,standard,pending);
        report += llformat("Auto factor candidates: %d; other possible: %d.\n",
                           candidates,uncertain);
        if (action!="inspect")
        {
            report += llformat("Updated %d PBR faces in viewer memory only.\n",changed);
            if (changed==0) report += "No loaded PBR materials to override.\n";
        }
        report += "ORM maps may change metal/roughness per pixel.\n";
        report += "Preview affects all faces sharing the same loaded material.\n";
        report += "No SL asset edits. Overrides reset when the viewer restarts.";
    }
    LLSD args; args["MESSAGE"]=report;
    LLNotificationsUtil::add("SystemMessage",args);
}

"""
patch(menu,"void handle_test_male()\n",inspector+"void handle_test_male()\n")
patch(menu,
    '    commit.add("Object.Inspect", boost::bind(&handle_object_inspect));\n',
    '''    commit.add("Object.Inspect", boost::bind(&handle_object_inspect));
    commit.add("KAMI.AliveInspect", boost::bind(&kami_alive_material_action, std::string("inspect")));
    commit.add("KAMI.AliveMetal", boost::bind(&kami_alive_material_action, std::string("metal")));
    commit.add("KAMI.AliveBypass", boost::bind(&kami_alive_material_action, std::string("bypass")));
    commit.add("KAMI.AliveAuto", boost::bind(&kami_alive_material_action, std::string("auto")));
''')
print("PASS KAMI ALIVE v0.2 inspector: native selected-face PBR metadata, local preview/bypass/auto, bounded 64-face scan.")
