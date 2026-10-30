#!/usr/bin/env python3
"""Install an opt-in, native Dark Cinematic rendering preset.

Three independently real components: material-aware PBR roughness shading in
the existing deferred pass; local directional EEP; live optimized avatar LOD.
No GShade/runtime injector, new GPU pass, temporal jitter or hidden FPS cap.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
V=R/"indra/newview"
shutil.copyfile(R/"kami/cinematic/kami_cinematic.h",V/"kami_cinematic.h")

shader=V/"app_settings/shaders/class3/deferred/softenLightF.glsl"
s=shader.read_text(encoding="utf-8-sig")
a="uniform int classic_mode;"
b="""uniform int classic_mode;
// KAMI: optional PBR-only material response. No new textures/shader pass.
uniform float kami_cinematic_material;"""
assert s.count(a)==1,"Soften shader uniform anchor changed"
s=s.replace(a,b,1)
a="        float perceptualRoughness = orm.g;"
b="""        float perceptualRoughness = orm.g;
        // Existing glTF ORM and normal maps remain source-of-truth. This
        // shallow perceptual remap enhances authored grazing highlights
        // without inventing detail or changing non-PBR/legacy surfaces.
        perceptualRoughness = mix(perceptualRoughness,
            clamp(perceptualRoughness * 0.93 + 0.018, 0.06, 1.0),
            clamp(kami_cinematic_material, 0.0, 1.0));"""
assert s.count(a)==1,"PBR roughness decode anchor changed"
s=s.replace(a,b,1)
shader.write_text(s,encoding="utf-8")

pipeline=V/"pipeline.cpp"
s=pipeline.read_text(encoding="utf-8-sig")
a="""            bindDeferredShader(soften_shader);
"""
b=a+"""
            // One scalar uniform on the existing deferred lighting pass;
            // disabled in Potato or Performance Mode to protect real FPS.
            static LLStaticHashedString kami_material_uniform("kami_cinematic_material");
            static LLCachedControl<bool> cinematic_materials(
                gSavedSettings, "KAMICinematicMaterials", false);
            const bool lightweight_materials =
                cinematic_materials && !gSavedSettings.getBOOL("KAMIPotatoMode") &&
                !gSavedSettings.getBOOL("KAMIPerformanceMode");
            soften_shader.uniform1f(kami_material_uniform,
                lightweight_materials ? 1.f : 0.f);
"""
assert s.count(a)==1,"Deferred shader binding anchor changed"
s=s.replace(a,b,1)
pipeline.write_text(s,encoding="utf-8")

settings=V/"app_settings/settings_blackdragon.xml"
doc=ET.parse(settings);root=doc.getroot().find("map")
assert root is not None
specs=[
("KAMICinematicMaterials","Boolean","integer","0",0,
 "Opt-in PBR-only roughness tuning in existing deferred lighting; auto bypassed in Potato/Performance."),
("KAMICinematicStatus","String","string","Off: ready to preview in-world.",0,
 "Status for reversible Dark Cinematic local EEP, material and avatar-FPS preview."),
]
for key,typ,tag,val,persist,comment in specs:
    assert not any(n.tag=="key" and n.text==key for n in root),key
    k=ET.Element("key");k.text=key
    value=ET.Element("map")
    for t,v in (("key","Comment"),("string",comment),("key","Persist"),
                ("integer",str(persist)),("key","Type"),("string",typ),
                ("key","Value"),(tag,val)):
        e=ET.SubElement(value,t);e.text=v
    root.insert(0,value);root.insert(0,k)
ET.indent(doc,space=" ")
doc.write(settings,encoding="utf-8",xml_declaration=True)

ui=V/"skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(ui);root=tree.getroot()
panels=[x for x in root.iter("layout_panel") if x.get("name")=="kombatant_display_quick"]
assert len(panels)==1,"Expected KAMI Fire Garden Display quick panel"
panel=panels[0]
assert any(x.get("name")=="kami_avatar_quality_toggle" for x in panel), (
    "Dark Cinematic must be added after live Avatar Quality")
height=int(panel.get("height"));delta=181
panel.set("height",str(height+delta))
def widget(tag,name,offset,h,**args):
    data={"name":name,"left":"12","top":str(height+offset),
          "height":str(h),"width":"465"}
    data.update({k:str(v) for k,v in args.items()})
    return ET.SubElement(panel,tag,data)
widget("text","kami_cine_heading",8,26,font="SansSerifLarge").text=(
    "KAMI DARK CINEMATIC  |  Three live graphics enhancements")
widget("text","kami_cine_features",38,54,wrap="true").text=(
    "PBR materials: refined authored roughness highlights. Lighting: real viewer-local directional EEP. "
    "Avatar: high self mesh detail + distant impostors for FPS. No extra render pass or temporal blur.")
widget("button","kami_cine_apply",99,28,label="Apply all three (preview)")
widget("button","kami_cine_restore",131,28,label="Restore prior graphics")
widget("text","kami_cine_status",163,18,wrap="true").text="Off: ready to preview in-world."
# Expand actual scroll view and the full stack including original BD tabs.
scroll=[n for n in root.iter("panel") if n.get("name")=="gfx_scroll_panel"]
stacks=[n for n in root.iter("layout_stack") if n.get("name")=="gfx_stack"]
assert len(scroll)==len(stacks)==1
scroll[0].set("height",str(int(panel.get("height"))+66))
stack=stacks[0];children=[x for x in stack if x.tag=="layout_panel"]
assert panel in children
stack.set("height",str(max(int(stack.get("height","0")),
    sum(int(x.get("height","0")) for x in children)+96)))
ET.indent(tree,space=" ")
tree.write(ui,encoding="utf-8",xml_declaration=True)

prefs=V/"llfloaterpreference.cpp"
s=prefs.read_text(encoding="utf-8-sig")
a='#include "kami_atmosphere.h"'
assert s.count(a)==1,"Atmosphere C++ include anchor changed"
s=s.replace(a,a+'\n#include "kami_cinematic.h"',1)
a='bool LLFloaterPreference::postBuild()\n{'
assert s.count(a)==1,"Preference postBuild anchor changed"
s=s.replace(a,a+"""
    if (auto* btn=findChild<LLButton>("kami_cine_apply"))
        btn->setClickedCallback([](LLUICtrl*,const LLSD&) {
            KAMICinematic::apply();
        });
    if (auto* btn=findChild<LLButton>("kami_cine_restore"))
        btn->setClickedCallback([](LLUICtrl*,const LLSD&) {
            KAMICinematic::restore();
        });
""",1)
a='void LLFloaterPreference::draw()\n{'
assert s.count(a)==1,"Preference draw anchor changed"
s=s.replace(a,a+"""
    // Status-only refresh once a second. Rendering runs entirely in native
    // shader and avatar LOD paths, not in Preferences::draw().
    static F64 lastCinematicStatus=0.;
    const F64 cinematicTime=KAMISignature::seconds();
    if (cinematicTime-lastCinematicStatus>=1.)
    {
        lastCinematicStatus=cinematicTime;
        if (auto* text=findChild<LLTextBox>("kami_cine_status"))
            text->setText(LLStringExplicit(
                gSavedSettings.getString("KAMICinematicStatus")));
    }
""",1)
prefs.write_text(s,encoding="utf-8")

manifest=V/"skins/default/KAMI-DARK-CINEMATIC.txt"
manifest.write_text(
    "KAMI Dark Cinematic: optional preview, user-initiated.\n"
    "1. Native glTF PBR roughness response only on existing PBR materials.\n"
    "2. Reversible local EEP Dark Cinematic mood with cool directional sky.\n"
    "3. Live Avatar Quality/FPS prioritizes self LOD and caps distant avatar rendering.\n"
    "No new GL pass, downloads, shadow-map increase, post-process blur, ray AO, upscaling or FPS cap.\n"
    "Toggle OFF restores saved values in the current session. Actual FPS still requires in-world testing.\n",
    encoding="utf-8")
ET.parse(settings);ET.parse(ui)
print("Dark Cinematic installed: native PBR highlight response, local EEP directional light and Avatar Quality/FPS.")
