#!/usr/bin/env python3
"""KAMI ALIVE: optional PBR profiles on existing SL material data.

Run after existing KAMI ALIVE/inspector stages, before compilation. Only
PBR materials; auto classification remains deliberately metallic-only.
Other classes require manual selection. No true SSS, cornea geometry or
new material asset imports. No extra sampling, FBOs or GPU readback.
"""
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
V=R/"indra/newview"
X=V/"skins/default/xui/en"
def splice(path,old,new):
    s=path.read_text(encoding="utf-8-sig")
    n=s.count(old)
    if n!=1: raise RuntimeError(f"ALIVE profiles mismatch {path} count={n} {old[:100]!r}")
    path.write_text(s.replace(old,new,1),encoding="utf-8")

settings=V/"app_settings/settings.xml"
splice(settings,"Local only: PBR material hash to mode (-1 standard, 0 auto, 1 polished metal). Shared identical PBR materials are affected equally; no SL server update.",
       "Local PBR profile by material hash: -1 standard, 0 auto, 1 polished metal, 2 gems, 3 skin, 4 eyes, 5 fabric, 6 leather. No SL server writes. Identical materials share a profile.")
ET.parse(settings)

mat=V/"llfetchedgltfmaterial.cpp"
splice(mat,"llclamp((F32)overrides[material_key].asInteger(), -1.f, 1.f);",
           "llclamp((F32)overrides[material_key].asInteger(), -1.f, 6.f);")

menu=X/"menu_viewer.xml"
tree=ET.parse(menu);root=tree.getroot()
work=[e for e in root if e.tag=="menu" and e.get("name")=="KAMI_Workspaces"]
assert len(work)==1
modes=(
 ("Gem","GEMSTONE & DIAMOND",2),
 ("Skin","LIVING SKIN",3),
 ("Eyes","LIVING EYES",4),
 ("Fabric","FABRIC",5),
 ("Leather","LEATHER",6))
for ident,title,num in modes:
    assert not any(e.get("name")=="KAMI_Alive_"+ident for e in root.iter())
    e=ET.SubElement(work[0],"menu_item_call",{"name":"KAMI_Alive_"+ident,
                    "label":"ALIVE  /  MATERIAL: "+title,"layout":"topleft"})
    ET.SubElement(e,"menu_item_call.on_click",{"function":"Kami.Alive."+ident})
ET.indent(tree,space=" ");tree.write(menu,encoding="utf-8",xml_declaration=True)

cpp=V/"llviewermenu.cpp"
splice(cpp,"static void kamiAliveMaterialCommand(S32 mode)\n{",
"""// Per-material profiles are user-assigned, not guessed from texture names.
static const char* kamiAliveModeName(S32 mode)
{
    switch (mode)
    {
      case -1: return "Standard";
      case 0: return "Automatic (smooth metal)";
      case 1: return "Polished metal";
      case 2: return "Gem/diamond approximate";
      case 3: return "Living skin approximate";
      case 4: return "Living eyes approximate";
      case 5: return "Fabric approximate";
      case 6: return "Leather approximate";
      default: return "Unknown";
    }
}
static void kamiAliveMaterialCommand(S32 mode)
{""")
splice(cpp,'    report += "Current beta: polished PBR response only. Skin, eyes and cloth are not yet enhanced.\\n";',
       '    report += "PBR beta: Metal automatic; skin, eyes, gem, fabric and leather need manual material profiles.\\n";')
splice(cpp,'(mode == 0 || mode == 1 || mode == -1)', '(mode >= -1 && mode <= 6)')
splice(cpp,'''                    (mode == 99 ? (prior == 1 ? "Metal override" :
                                    prior == -1 ? "Standard override" : "Auto")
                                : (mode == 1 ? "Metal override requested" :
                                   mode == -1 ? "Standard requested" : "Auto requested"))''',
       "                    kamiAliveModeName(mode == 99 ? prior : mode)")
splice(cpp,'prior != mode && local.size() < 512)', 'prior != mode && (local.has(key) || local.size() < 512))')
splice(cpp,'                local[key] = mode;\n                ++changed;',
       '''                if (mode == 0) local.erase(key);
                else local[key] = mode;
                ++changed;''')
splice(cpp,'    commit.add("Kami.Alive.Standard", boost::bind(&kamiAliveMaterialCommand, -1));',
       '''    commit.add("Kami.Alive.Standard", boost::bind(&kamiAliveMaterialCommand, -1));
    commit.add("Kami.Alive.Gem", boost::bind(&kamiAliveMaterialCommand, 2));
    commit.add("Kami.Alive.Skin", boost::bind(&kamiAliveMaterialCommand, 3));
    commit.add("Kami.Alive.Eyes", boost::bind(&kamiAliveMaterialCommand, 4));
    commit.add("Kami.Alive.Fabric", boost::bind(&kamiAliveMaterialCommand, 5));
    commit.add("Kami.Alive.Leather", boost::bind(&kamiAliveMaterialCommand, 6));''')

# Shared deterministic fragment routine. ALIVE's original metal function
# continues to run for auto/metal modes. No per-frame texture classification.
surface="""
// Profiles on existing PBR properties only; no added texture samples.
// Gem/eye glints and skin softness are approximations, not true light
// scattering/cornea/caustics. Camera-dependent glints are intentionally faint.
void kamiAliveSurface(inout vec3 col, inout vec3 orm,
                      inout vec3 n, inout vec3 em, vec3 eyePos)
{
    float gain = clamp(kami_alive_amount, 0.0, 1.0);
    float mode = kami_alive_material_override;
    if (gain <= 0.0001 || mode < 1.5) return;
    if (mode < 2.5) // diamond and gems
    {
        orm.g = mix(orm.g, clamp(orm.g * 0.58, 0.06, 0.28), gain);
        vec3 v = normalize(-eyePos);
        float rim = 1.0 - clamp(dot(normalize(n), v), 0.0, 1.0);
        em += col * (0.014 * gain * pow(rim, 20.0));
    }
    else if (mode < 3.5) // living skin, soft diffuse approximation
    {
        orm.b = mix(orm.b, 0.0, gain * 0.85);
        orm.g = mix(orm.g, clamp(orm.g, 0.40, 0.64), gain * 0.65);
        col = mix(col, col * vec3(1.008,1.004,0.995) +
                         vec3(0.003,0.0015,0.001), gain * 0.36);
    }
    else if (mode < 4.5) // corneal sheen approximation for eyes
    {
        orm.b = mix(orm.b, 0.0, gain);
        orm.g = mix(orm.g, clamp(orm.g * 0.32, 0.055, 0.18), gain);
        vec3 v = normalize(-eyePos);
        em += vec3(0.008 * gain *
            pow(max(dot(normalize(n), v), 0.0), 52.0));
    }
    else if (mode < 5.5) // cloth
    {
        orm.b = mix(orm.b, 0.0, gain);
        orm.g = mix(orm.g, max(orm.g, 0.74), gain * 0.80);
    }
    else if (mode < 6.5) // leather
    {
        orm.b = mix(orm.b, 0.0, gain);
        orm.g = mix(orm.g, clamp(orm.g, 0.24, 0.54), gain * 0.72);
        col *= 1.0 - 0.017 * gain;
    }
}
"""
for sub in ("deferred/pbropaqueF.glsl","gltf/pbrmetallicroughnessF.glsl"):
    path=V/"app_settings/shaders/class1"/sub
    splice(path,
       'if (kami_alive_material_override > 0.5)\n        metal_weight = 1.0;',
       'if (kami_alive_material_override > 0.5 && kami_alive_material_override < 1.5)\n        metal_weight = 1.0;')
    splice(path,'    return mix(roughness, polished, blend);\n}',
       '    return mix(roughness, polished, blend);\n}\n'+surface)

p=V/"app_settings/shaders/class1/deferred/pbropaqueF.glsl"
splice(p,'    emissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);\n\n    tnorm *= gl_FrontFacing ? 1.0 : -1.0;',
       '''    emissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);
    kamiAliveSurface(col, spec, tnorm, emissive, vary_position);

    tnorm *= gl_FrontFacing ? 1.0 : -1.0;''')
p=V/"app_settings/shaders/class1/gltf/pbrmetallicroughnessF.glsl"
splice(p,'    orm.g = kamiAlivePolishedRoughness(orm.g, orm.b);\n#endif',
       '''    orm.g = kamiAlivePolishedRoughness(orm.g, orm.b);
    kamiAliveSurface(basecolor.rgb, orm, norm, emissive, pos);
#endif''')
print("PASS: KAMI ALIVE metal, gems, skin, eyes, fabric, leather native shader profiles installed.")
