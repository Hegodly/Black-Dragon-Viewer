#!/usr/bin/env python3
"""KAMI ALIVE visual profiles beta: extend existing native PBR and baked avatar paths.

Run once AFTER apply-kami-alive.py and apply-material-inspector.py.
Opt-in master toggle; manual selected-face semantics use existing PBR
materials only and never upload or alter an SL asset. Automatically handle
native/baked classic avatar skin and eyes through their dedicated shaders.
No extra geometry, material textures, post passes, or GPU synchronization.
This is NOT real subsurface scattering, tearline mesh, or gemstone ray tracing.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
V = ROOT / "indra/newview"
SHADERS = V / "app_settings/shaders/class1"

def splice(path, old, new):
    src = path.read_text(encoding="utf-8-sig")
    count = src.count(old)
    if count != 1:
        raise RuntimeError(f"KAMI ALIVE visual profiles drift {path}: {count} anchors for {old[:90]!r}")
    path.write_text(src.replace(old, new, 1), encoding="utf-8")

# Classic avatar shader support: two separate controls, master ALIVE still
# gates both through the amount uniform (and its CPU-frame-time governor).
settings = V / "app_settings/settings.xml"
entries = """  <key>KAMIAliveAvatarSkinEnabled</key>
  <map>
    <key>Comment</key><string>Optional gentle baked-avatar skin light wrap. No true SSS, no mesh changes.</string>
    <key>Persist</key><integer>1</integer>
    <key>Type</key><string>Boolean</string>
    <key>Value</key><integer>1</integer>
  </map>
  <key>KAMIAliveAvatarEyesEnabled</key>
  <map>
    <key>Comment</key><string>Optional legacy avatar eye gloss on dedicated eye shader. No cornea geometry.</string>
    <key>Persist</key><integer>1</integer>
    <key>Type</key><string>Boolean</string>
    <key>Value</key><integer>1</integer>
  </map>
"""
splice(settings, "  <key>ImporterDebugVerboseLogging</key>", entries+"  <key>ImporterDebugVerboseLogging</key>")
ET.parse(settings)

menu = V/"skins/default/xui/en/menu_viewer.xml"
tree=ET.parse(menu)
root=tree.getroot()
workspaces=[e for e in root if e.tag=="menu" and e.get("name")=="KAMI_Workspaces"]
assert len(workspaces)==1
w=workspaces[0]
names={e.get("name") for e in root.iter()}
for name, label, callback in (
    ("KAMI_Alive_Gem", "ALIVE  /  MATERIAL MODE: GEM / DIAMOND", "Kami.Alive.Gem"),
    ("KAMI_Alive_Skin", "ALIVE  /  MATERIAL MODE: SKIN", "Kami.Alive.Skin"),
    ("KAMI_Alive_Eyes", "ALIVE  /  MATERIAL MODE: EYES / CORNEA", "Kami.Alive.Eyes"),
    ("KAMI_Alive_Fabric", "ALIVE  /  MATERIAL MODE: FABRIC", "Kami.Alive.Fabric"),
    ("KAMI_Alive_Leather", "ALIVE  /  MATERIAL MODE: LEATHER", "Kami.Alive.Leather"),
):
    assert name not in names, name
    item=ET.SubElement(w, "menu_item_call", {"name":name,"label":label,"layout":"topleft"})
    ET.SubElement(item,"menu_item_call.on_click",{"function":callback})
for name,label,control in (
    ("KAMI_Alive_Avatar_Skin","ALIVE  /  BAKED AVATAR SKIN LIGHT WRAP", "KAMIAliveAvatarSkinEnabled"),
    ("KAMI_Alive_Avatar_Eyes","ALIVE  /  BAKED AVATAR EYE GLOSS", "KAMIAliveAvatarEyesEnabled"),
):
    assert name not in names, name
    item=ET.SubElement(w, "menu_item_check", {"name":name,"label":label,"layout":"topleft"})
    ET.SubElement(item,"menu_item_check.on_check",{"control":control})
    ET.SubElement(item,"menu_item_check.on_click",{"function":"ToggleControl","parameter":control})
ET.indent(tree,space=" ")
tree.write(menu,encoding="utf-8",xml_declaration=True)
ET.parse(menu)

# Upgrade the saved per-material category to include explicit semantic modes:
# -1 standard, 0 automatic metallic only, 1 polished metal, 2 gem,
# 3 skin, 4 eyes, 5 fabric, 6 leather.
mat_cpp = V/"llfetchedgltfmaterial.cpp"
splice(mat_cpp,
    "llclamp((F32)overrides[material_key].asInteger(), -1.f, 1.f);",
    "llclamp((F32)overrides[material_key].asInteger(), -1.f, 6.f);")
splice(V/"app_settings/settings.xml",
    "Local only: PBR material hash to mode (-1 standard, 0 auto, 1 polished metal). Shared identical PBR materials are affected equally; no SL server update.",
    "Local PBR material hash to profile (-1 standard, 0 auto metal, 1 metal, 2 gem, 3 skin, 4 eyes, 5 fabric, 6 leather). Shared materials share settings; never sent to Second Life.")

inspector = V/"llviewermenu.cpp"
mode_labels = """
// These manual classes must not be inferred from texture names or appearance.
static const char* kamiAliveProfileName(S32 mode)
{
    switch (mode)
    {
        case -1: return "Standard";
        case 0: return "Automatic (metal only)";
        case 1: return "Polished Metal";
        case 2: return "Gem / Diamond";
        case 3: return "Living Skin";
        case 4: return "Living Eyes";
        case 5: return "Fabric";
        case 6: return "Leather";
        default: return "Unknown";
    }
}
"""
splice(inspector, "static void kamiAliveMaterialCommand(S32 mode)",
       mode_labels+"\nstatic void kamiAliveMaterialCommand(S32 mode)")
splice(inspector, "Current beta: polished PBR response only. Skin, eyes and cloth are not yet enhanced.\\n",
       "Beta profiles: PBR metal/gem/skin/eyes/fabric/leather. Baked avatar skin/eyes use separate shader controls.\\n")
splice(inspector,
       "mode == 0 || mode == 1 || mode == -1",
       "mode >= -1 && mode <= 6")
old_report="""(mode == 99 ? (prior == 1 ? "Metal override" :
                                    prior == -1 ? "Standard override" : "Auto")
                                : (mode == 1 ? "Metal override requested" :
                                   mode == -1 ? "Standard requested" : "Auto requested"))"""
splice(inspector,old_report,
       "(mode == 99 ? kamiAliveProfileName(prior) : kamiAliveProfileName(mode))")
splice(inspector,
       """    commit.add("Kami.Alive.Standard", boost::bind(&kamiAliveMaterialCommand, -1));
""",
       """    commit.add("Kami.Alive.Standard", boost::bind(&kamiAliveMaterialCommand, -1));
    commit.add("Kami.Alive.Gem", boost::bind(&kamiAliveMaterialCommand, 2));
    commit.add("Kami.Alive.Skin", boost::bind(&kamiAliveMaterialCommand, 3));
    commit.add("Kami.Alive.Eyes", boost::bind(&kamiAliveMaterialCommand, 4));
    commit.add("Kami.Alive.Fabric", boost::bind(&kamiAliveMaterialCommand, 5));
    commit.add("Kami.Alive.Leather", boost::bind(&kamiAliveMaterialCommand, 6));
""")

# The existing metallic roughness shader function is kept intact for the
# legacy phase 1 verifier; manually tagged other classes must NOT be treated
# as polished metal just because their profile code is positive.
profile_glsl = r"""
// KAMI ALIVE beta manual profiles, PBR material only. All lighting continues
// through original Second Life PBR light/reflection probes.
// No random glitter, new textures, additional samples or temporal flicker.
void kamiAliveApplyProfile(inout vec3 orm, inout vec3 baseColor,
                           vec3 surfaceNormal, vec3 viewPos)
{
    float strength = clamp(kami_alive_amount, 0.0, 1.0);
    float profile = kami_alive_material_override;
    if (strength <= 0.001 || profile < 1.5) return;

    vec3 n = normalize(surfaceNormal);
    vec3 v = normalize(-viewPos);
    float facing = clamp(dot(n, v), 0.0, 1.0);
    float rim = pow(1.0 - facing, 3.0);

    if (profile < 2.5)   // Gem / diamond: existing environment highlights
    {
        float facet = pow(abs(dot(n, normalize(vec3(0.38, 0.56, 0.74)))), 12.0);
        float target = clamp(orm.g * (0.68 - 0.13 * facet), 0.045, orm.g);
        orm.g = mix(orm.g, target, strength * 0.9);
        // A slight view-normal modulation, not an emissive glow or fake flash.
        baseColor *= 1.0 + 0.012 * facet * strength;
    }
    else if (profile < 3.5) // Living Skin: subtle smooth sheen/light wrap
    {
        orm.g = mix(orm.g, clamp(orm.g, 0.38, 0.72), strength * 0.4);
        baseColor = min(baseColor * (1.0 + 0.025 * rim * strength), vec3(1.0));
    }
    else if (profile < 4.5) // Living Eyes: wet corneal-like response
    {
        orm.g = mix(orm.g, min(orm.g, 0.10 + 0.04 * rim), strength * 0.8);
    }
    else if (profile < 5.5) // Fabric: soften synthetic/plastic glare
    {
        orm.g = mix(orm.g, max(orm.g, 0.82), strength * 0.75);
    }
    else if (profile < 6.5) // Leather: restrained directional gloss
    {
        orm.g = mix(orm.g, clamp(orm.g, 0.30, 0.55), strength * 0.55);
    }
}
"""
opaque=SHADERS/"deferred/pbropaqueF.glsl"
gltf=SHADERS/"gltf/pbrmetallicroughnessF.glsl"
for path in (opaque,gltf):
    splice(path,
        """    if (kami_alive_material_override > 0.5)
        metal_weight = 1.0;""",
        """    if (kami_alive_material_override > 0.5 && kami_alive_material_override < 1.5)
        metal_weight = 1.0;
    if (kami_alive_material_override > 1.5)
        metal_weight = 0.0;""")
splice(opaque, "uniform sampler2D diffuseMap;  //always in sRGB space",
       profile_glsl+"\nuniform sampler2D diffuseMap;  //always in sRGB space")
splice(gltf, "uniform int gltf_material_id;",profile_glsl+"\nuniform int gltf_material_id;")
splice(opaque,
    "    spec.g = kamiAlivePolishedRoughness(spec.g, spec.b);\n",
    "    spec.g = kamiAlivePolishedRoughness(spec.g, spec.b);\n"
    "    kamiAliveApplyProfile(spec, col, tnorm, vary_position);\n")
splice(gltf,
    "    orm.g = kamiAlivePolishedRoughness(orm.g, orm.b);\n",
    "    orm.g = kamiAlivePolishedRoughness(orm.g, orm.b);\n"
    "    kamiAliveApplyProfile(orm, basecolor.rgb, norm, vary_position);\n")

# Imported glTF asset drawing uses different bind path and cannot use selected
# Second Life material hashes. Explicitly reset the same shader uniform at
# start of that separate render path to avoid carrying over earlier selections.
gltf_scene=V/"gltfscenemanager.cpp"
splice(gltf_scene,
    "                glBindBufferBase(GL_UNIFORM_BUFFER, LLGLSLShader::UB_GLTF_MATERIALS, asset.mMaterialsUBO);\n",
    """                glBindBufferBase(GL_UNIFORM_BUFFER, LLGLSLShader::UB_GLTF_MATERIALS, asset.mMaterialsUBO);
                // This glTF-scene path does not have SL selected-face material hashes.
                // Reset previous per-material ALIVE profiles; no fallback guessing.
                static LLStaticHashedString sKAMIAliveMaterialOverride("kami_alive_material_override");
                LLGLSLShader::sCurBoundShaderPtr->uniform1f(sKAMIAliveMaterialOverride, 0.f);
""")

# Actual classic avatar mesh uses dedicated skin/eye paths, so add conservative
# effects for existing baked avatar textures even without glTF/PBR materials.
avatar=SHADERS/"deferred/avatarF.glsl"
splice(avatar,"uniform float minimum_alpha;",
    "uniform float minimum_alpha;\nuniform float kami_alive_amount;\n"
    "uniform float kami_alive_avatar_skin_enabled;")
splice(avatar,"    frag_data[0] = vec4(diff.rgb, 0.0);",
    """    // Skin-light wrap approximation (not real volume SSS).
    // Only the dedicated built-in baked-avatar deferred program uses this.
    vec3 eye_dir = normalize(-vary_position);
    float rim = pow(1.0 - clamp(dot(normalize(vary_normal), eye_dir), 0.0, 1.0), 2.0);
    float a = clamp(kami_alive_amount * kami_alive_avatar_skin_enabled, 0.0, 1.0);
    diff.rgb = min(diff.rgb * (1.0 + 0.035 * rim * a), vec3(1.0));
    frag_data[0] = vec4(diff.rgb, 0.0);""")

# Avoid changing the generic diffuse shader (used by many unrelated surfaces).
# Create a new fragment file solely for the existing native avatar eyes shader.
original_eyes=SHADERS/"deferred/diffuseF.glsl"
eye_file=SHADERS/"deferred/kamiAvatarEyesF.glsl"
if eye_file.exists():
    raise RuntimeError("Dedicated KAMI avatar-eyes shader already installed")
eyes_src=original_eyes.read_text(encoding="utf-8-sig")
assert eyes_src.count("uniform sampler2D diffuseMap;")==1
assert eyes_src.count("    frag_data[1] = vertex_color.aaaa; // spec")==1
eyes_src=eyes_src.replace("uniform sampler2D diffuseMap;",
    "uniform sampler2D diffuseMap;\nuniform float kami_alive_amount;\n"
    "uniform float kami_alive_avatar_eyes_enabled;",1)
eyes_src=eyes_src.replace("    frag_data[1] = vertex_color.aaaa; // spec",
    """    // Small native-eye specular boost, preserving the existing iris texture.
    // Not a new corneal mesh, tearline geometry or ray-traced refraction.
    float wet = clamp(kami_alive_amount * kami_alive_avatar_eyes_enabled, 0.0, 1.0);
    float spec_value = mix(vertex_color.a, min(vertex_color.a + 0.10, 1.0), wet * 0.65);
    frag_data[1] = vec4(spec_value); // legacy deferred eye spec""",1)
eye_file.write_text(eyes_src,encoding="utf-8")
mgr=V/"llviewershadermgr.cpp"
splice(mgr,
    'gDeferredAvatarEyesProgram.mShaderFiles.push_back(make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER));',
    'gDeferredAvatarEyesProgram.mShaderFiles.push_back(make_pair("deferred/kamiAvatarEyesF.glsl", GL_FRAGMENT_SHADER));')
splice(mgr,
    "    dirty(gGLTFPBRMetallicRoughnessProgram);\n",
    "    dirty(gGLTFPBRMetallicRoughnessProgram);\n"
    "    dirty(gDeferredAvatarProgram);\n"
    "    dirty(gDeferredAvatarEyesProgram);\n")
# Refresh control state even at stable frame-time when the user changes a menu
# toggle. Other than option changes, keep the original 4 Hz uniform governor.
splice(mgr,
    "    mKAMIAliveUniformAge += dt;\n",
    """    static bool last_skin = gSavedSettings.getBOOL("KAMIAliveAvatarSkinEnabled");
    static bool last_eyes = gSavedSettings.getBOOL("KAMIAliveAvatarEyesEnabled");
    const bool skin = gSavedSettings.getBOOL("KAMIAliveAvatarSkinEnabled");
    const bool eyes = gSavedSettings.getBOOL("KAMIAliveAvatarEyesEnabled");
    const bool profile_switch_changed = skin != last_skin || eyes != last_eyes;
    last_skin = skin;
    last_eyes = eyes;
    mKAMIAliveUniformAge += dt;
""")
splice(mgr,
    """    if ((mKAMIAliveUniformAge < 0.25f && enabled) ||
        (std::fabs(applied - mKAMIAliveUniform) < 0.025f &&
         !( !enabled && mKAMIAliveUniform > 0.f )))""",
    """    if (!profile_switch_changed &&
        ((mKAMIAliveUniformAge < 0.25f && enabled) ||
         (std::fabs(applied - mKAMIAliveUniform) < 0.025f &&
          !(!enabled && mKAMIAliveUniform > 0.f))))""")
splice(mgr,
    '    shader->uniform1f(sKAMIAliveAmount, mKAMIAliveUniform);\n',
    """    shader->uniform1f(sKAMIAliveAmount, mKAMIAliveUniform);
    static LLStaticHashedString sKAMIAliveSkin("kami_alive_avatar_skin_enabled");
    static LLStaticHashedString sKAMIAliveEyes("kami_alive_avatar_eyes_enabled");
    shader->uniform1f(sKAMIAliveSkin,
        gSavedSettings.getBOOL("KAMIAliveAvatarSkinEnabled") ? 1.f : 0.f);
    shader->uniform1f(sKAMIAliveEyes,
        gSavedSettings.getBOOL("KAMIAliveAvatarEyesEnabled") ? 1.f : 0.f);
""")
print("Installed ALIVE beta profiles: manual PBR gem/skin/eye/fabric/leather, baked avatar skin and eyes, scene uniform isolation.")
