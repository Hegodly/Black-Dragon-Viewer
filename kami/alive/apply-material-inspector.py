#!/usr/bin/env python3
"""KAMI ALIVE phase 1b: selected-face material inspector + local manual overrides.

Run after apply-kami-alive.py and both modern Graphics-tab scripts.
This integrates a *real* native selection inspection command and persistent
local per-material-class controls. Only existing loaded GLTF/PBR material data
is inspected, never server asset contents or remote inventories.

Do NOT label non-PBR surfaces compatible: current enhancement is polished PBR
roughness only. A manual 'polished metal' choice never edits SL material data.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
V = ROOT / "indra/newview"
XUI = V / "skins/default/xui/en"


def splice(path, old, new):
    text = path.read_text(encoding="utf-8-sig")
    matches = text.count(old)
    if matches != 1:
        raise RuntimeError(f"KAMI ALIVE inspector integration drift {path}: {matches} anchors")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


settings = V / "app_settings/settings.xml"
setting_entry = """  <key>KAMIAliveMaterialOverrides</key>
  <map>
    <key>Comment</key>
    <string>Local only: PBR material hash to mode (-1 standard, 0 auto, 1 polished metal). Shared identical PBR materials are affected equally; no SL server update.</string>
    <key>Persist</key><integer>1</integer>
    <key>Type</key><string>LLSD</string>
    <key>Value</key><map/>
  </map>
  <key>KAMIAliveOverrideRevision</key>
  <map>
    <key>Comment</key><string>Revision counter to lazily refresh per-material cached override only when user edits it.</string>
    <key>Persist</key><integer>0</integer>
    <key>Type</key><string>S32</string>
    <key>Value</key><integer>0</integer>
  </map>
"""
splice(settings, "  <key>ImporterDebugVerboseLogging</key>", setting_entry + "  <key>ImporterDebugVerboseLogging</key>")
ET.parse(settings)

menu = XUI / "menu_viewer.xml"
tree = ET.parse(menu)
root = tree.getroot()
workspace = [x for x in root if x.tag == "menu" and x.get("name") == "KAMI_Workspaces"]
assert len(workspace) == 1, "KAMI native Workspaces not installed"
work = workspace[0]
assert not any(x.get("name") == "KAMI_Alive_Inspect" for x in root.iter())
for name, label, callback in (
    ("KAMI_Alive_Inspect", "ALIVE  /  INSPECT SELECTED PBR FACES", "Kami.Alive.Inspect"),
    ("KAMI_Alive_Auto", "ALIVE  /  MATERIAL MODE: AUTOMATIC", "Kami.Alive.Auto"),
    ("KAMI_Alive_Metal", "ALIVE  /  MATERIAL MODE: POLISHED METAL", "Kami.Alive.Metal"),
    ("KAMI_Alive_Standard", "ALIVE  /  MATERIAL MODE: STANDARD", "Kami.Alive.Standard"),
):
    item = ET.SubElement(work, "menu_item_call", {
        "name": name, "label": label, "layout": "topleft",
    })
    ET.SubElement(item, "menu_item_call.on_click", {"function": callback})
ET.indent(tree, space=" ")
tree.write(menu, encoding="utf-8", xml_declaration=True)

# Implement local-only session display and stable PBR-material signatures.
# Lookups on the render path are revision-cached per fetched material instance:
# no per-frame inventory walking, shader rebuild, GPU stalls, or new FBOs.
mat_h = V / "llfetchedgltfmaterial.h"
splice(mat_h,
    "    bool isFetching() const { return mFetching; }\n",
    """    // KAMI ALIVE local material override cache; keyed by material content hash.
    // Revised only when the user changes an override, not every render frame.
    S32 mKAMIAliveCachedRevision = -1;
    F32 mKAMIAliveManualMode = 0.f;

    bool isFetching() const { return mFetching; }
""")

mat_cpp = V / "llfetchedgltfmaterial.cpp"
splice(mat_cpp, '#include "llfetchedgltfmaterial.h"\n',
    '#include "llfetchedgltfmaterial.h"\n#include "llviewercontrol.h"\n')
splice(mat_cpp,
    "    LLGLTFMaterial::operator =(rhs);\n",
    """    LLGLTFMaterial::operator =(rhs);
    // A material's identity can change after assignment or fetching.
    mKAMIAliveCachedRevision = -1;
    mKAMIAliveManualMode = 0.f;
""")
splice(mat_cpp,
    "        shader->uniform1f(LLShaderMgr::METALLIC_FACTOR, mMetallicFactor);\n",
    """        shader->uniform1f(LLShaderMgr::METALLIC_FACTOR, mMetallicFactor);

        // Cheap in the normal path. Resolve expensive content hash and LLSD map
        // only when this material instance first appears or the local map changes.
        static LLStaticHashedString sKAMIAliveManualMode("kami_alive_material_override");
        const S32 revision = gSavedSettings.getS32("KAMIAliveOverrideRevision");
        if (mKAMIAliveCachedRevision != revision)
        {
            mKAMIAliveManualMode = 0.f;
            const LLSD overrides = gSavedSettings.getLLSD("KAMIAliveMaterialOverrides");
            if (overrides.isMap() && overrides.size() > 0)
            {
                const std::string material_key = getHash().asString();
                if (overrides.has(material_key))
                {
                    mKAMIAliveManualMode =
                        llclamp((F32)overrides[material_key].asInteger(), -1.f, 1.f);
                }
            }
            mKAMIAliveCachedRevision = revision;
        }
        shader->uniform1f(sKAMIAliveManualMode, mKAMIAliveManualMode);
""")

shader_snippet = """uniform float kami_alive_amount;
uniform float kami_alive_material_override;
float kamiAlivePolishedRoughness(float roughness, float metallic)
{
    float metal_weight = clamp((metallic - 0.68) * 3.125, 0.0, 1.0);
    if (kami_alive_material_override > 0.5)
        metal_weight = 1.0;
    if (kami_alive_material_override < -0.5)
        metal_weight = 0.0;
"""
for path in (
    V / "app_settings/shaders/class1/deferred/pbropaqueF.glsl",
    V / "app_settings/shaders/class1/gltf/pbrmetallicroughnessF.glsl",
):
    splice(path,
        """uniform float kami_alive_amount;
float kamiAlivePolishedRoughness(float roughness, float metallic)
{
    float metal_weight = clamp((metallic - 0.68) * 3.125, 0.0, 1.0);
""", shader_snippet)

# Inspector actions are intentionally on-demand menu clicks (not a
# per-frame scanner), work on a bounded number of selected loaded PBR faces,
# and use SystemMessage rather than a new native floater/third-party UI.
code = r"""
// KAMI ALIVE: conservative client-side material inspection / mode override.
// A material hash identifies a *material*, not an item of clothing; the same
// content hash on multiple objects will share the selected local override.
static void kamiAliveMaterialCommand(S32 mode)
{
    LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
    if (selection.isNull())
    {
        LLSD args;
        args["MESSAGE"] = "KAMI ALIVE: Select an object or its face in Edit mode first.";
        LLNotificationsUtil::add("SystemMessage", args);
        return;
    }

    LLSD local = gSavedSettings.getLLSD("KAMIAliveMaterialOverrides");
    if (!local.isMap()) local = LLSD::emptyMap();

    std::string report = "KAMI ALIVE  |  Selected Material Inspector\n";
    report += "Current beta: polished PBR response only. Skin, eyes and cloth are not yet enhanced.\n";
    S32 inspected = 0;
    S32 eligible = 0;
    S32 changed = 0;
    S32 non_pbr = 0;
    S32 capped = 0;
    // Limit menu work so selecting thousands of objects doesn't hitch.
    const S32 max_faces = 96;
    for (LLObjectSelection::valid_iterator iter = selection->valid_begin();
         iter != selection->valid_end() && inspected + non_pbr < max_faces; ++iter)
    {
        LLSelectNode* node = *iter;
        LLViewerObject* object = node ? node->getObject() : nullptr;
        if (!object || object->isDead()) continue;
        const S32 te_count = llmin((S32)object->getNumTEs(), max_faces);
        bool face_selected = false;
        for (S32 face = 0; face < te_count; ++face)
        {
            if (node->isTESelected((U8)face)) face_selected = true;
        }
        for (S32 face = 0; face < te_count && inspected + non_pbr < max_faces; ++face)
        {
            if (face_selected && !node->isTESelected((U8)face)) continue;
            LLTextureEntry* te = object->getTE((U8)face);
            if (!te || !te->getGLTFRenderMaterial())
            {
                ++non_pbr;
                continue;
            }
            LLGLTFMaterial* material = te->getGLTFRenderMaterial();
            ++inspected;
            const bool could_be_metal =
                material->mMetallicFactor > 0.68f &&
                material->mRoughnessFactor < 0.48f;
            if (could_be_metal) ++eligible;

            const std::string key = material->getHash().asString();
            const S32 prior = local.has(key) ? local[key].asInteger() : 0;
            if (mode != 99 && (mode == 0 || mode == 1 || mode == -1) &&
                prior != mode && local.size() < 512)
            {
                local[key] = mode;
                ++changed;
            }
            if (inspected <= 8)
            {
                report += llformat("Face %d: PBR %s | metallic factor %.2f, roughness factor %.2f | %s\n",
                    face,
                    could_be_metal ? "candidate" : "limited / unknown",
                    material->mMetallicFactor,
                    material->mRoughnessFactor,
                    (mode == 99 ? (prior == 1 ? "Metal override" :
                                    prior == -1 ? "Standard override" : "Auto")
                                : (mode == 1 ? "Metal override requested" :
                                   mode == -1 ? "Standard requested" : "Auto requested")));
            }
        }
    }
    if (inspected + non_pbr >= max_faces) capped = 1;
    if (mode != 99 && changed > 0)
    {
        gSavedSettings.setLLSD("KAMIAliveMaterialOverrides", local);
        gSavedSettings.setS32("KAMIAliveOverrideRevision",
                             gSavedSettings.getS32("KAMIAliveOverrideRevision") + 1);
    }

    if (inspected == 0 && non_pbr == 0)
        report += "No selected faces found. Select an object or a face and retry.\n";
    report += llformat("PBR faces: %d  |  Candidate by scalar factors: %d  |  Non-PBR/unloaded: %d\n",
                       inspected, eligible, non_pbr);
    report += llformat("Local override changes: %d%s\n", changed,
                       capped ? "  |  Scan capped at 96 faces" : "");
    report += "A texture ORM map may change metallic/roughness per pixel. ";
    report += "A candidate is not a guarantee of a visible ALIVE change.\n";
    report += "Changes are local to this viewer, shared by identical PBR materials, ";
    report += "and need KAMI ALIVE enabled in Graphics.";
    LLSD args;
    args["MESSAGE"] = report;
    LLNotificationsUtil::add("SystemMessage", args);
}
"""
menu_cpp = V / "llviewermenu.cpp"
splice(menu_cpp, "void handle_selected_material_info()\n{", code + "\nvoid handle_selected_material_info()\n{")
splice(menu_cpp,
    '    commit.add("Object.Inspect", boost::bind(&handle_object_inspect));\n',
    """    commit.add("Object.Inspect", boost::bind(&handle_object_inspect));
    commit.add("Kami.Alive.Inspect", boost::bind(&kamiAliveMaterialCommand, 99));
    commit.add("Kami.Alive.Auto", boost::bind(&kamiAliveMaterialCommand, 0));
    commit.add("Kami.Alive.Metal", boost::bind(&kamiAliveMaterialCommand, 1));
    commit.add("Kami.Alive.Standard", boost::bind(&kamiAliveMaterialCommand, -1));
""")

print("KAMI ALIVE material inspection integrated: 4 native menu commands, "
      "persisted local per-material mode, 96-face scan cap, revision-cached shader override.")
