#!/usr/bin/env python3
"""Add real, viewer-local KAMI Atmosphere Engine to KAMI's native Fire Garden UI."""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
V=R/"indra/newview"
SKIN=V/"skins/default/xui/en"

shutil.copyfile(R/"kami/atmosphere/kami_atmosphere.h",V/"kami_atmosphere.h")
settings=V/"app_settings/settings_blackdragon.xml"
doc=ET.parse(settings)
root=doc.getroot().find("map")
assert root is not None
configs=[
 ("KAMIAtmosphereMood","String","string","Night Studio",1,"Chosen local EEP atmospheric color mood; does not alter the region."),
 ("KAMIAtmosphereStrength","F32","real","0.75",1,"How strongly to apply the KAMI mood to a clone of the current sky."),
 ("KAMIAtmosphereFade","F32","real","3",1,"Seconds to crossfade the viewer-local sky when applying/restoring."),
 ("KAMIAtmosphereBloom","Boolean","integer","0",0,"Retired nonworking KAMI bloom setting. Permanently inactive; retained for backward-compatible code paths."),
 ("KAMIAtmosphereStatus","String","string","Choose a mood, then Preview. All changes stay viewer-local.",0,"Status of the in-memory, reversible KAMI local sky preview."),
]
for name,ty,vtag,value,persist,comment in configs:
    assert not any(n.tag=="key" and n.text==name for n in root),"duplicate setting "+name
    k=ET.Element("key");k.text=name
    d=ET.Element("map")
    for tag,txt in [("key","Comment"),("string",comment),("key","Persist"),("integer",str(persist)),
                    ("key","Type"),("string",ty),("key","Value"),(vtag,value)]:
        child=ET.SubElement(d,tag);child.text=txt
    root.insert(0,d);root.insert(0,k)
ET.indent(doc,space=" ")
doc.write(settings,encoding="utf-8",xml_declaration=True)

ui=SKIN/"panel_preferences_graphics1.xml"
doc=ET.parse(ui)
root=doc.getroot()
panels=[n for n in root.iter() if n.tag=="layout_panel" and n.get("name")=="kombatant_display_quick"]
assert len(panels)==1, "KAMI quick graphics scroll missing"
panel=panels[0]
h=int(panel.get("height"))
assert h>=4500, "Previous Fire Garden sections unexpectedly absent"
delta=290
panel.set("height",str(h+delta))
parents=[n for n in root.iter() if n is not panel and n.get("height")==str(h+4)]
assert parents,("Missing expanded outer graphics scroll area",h)
for parent in parents:parent.set("height",str(h+4+delta))
def control(tag,name,offset,**attrs):
    return ET.SubElement(panel,tag,{"name":name,"left":"12","top":str(h+offset),
                                 **{k:str(v) for k,v in attrs.items()}})
control("text","kami_atmo_title",8,width=465,height=25,font="SansSerifLarge",
    text_color="0.54 0.77 0.98 1").text="KAMI ATMOSPHERE ENGINE  |  Cinematic local EEP"
combo=control("combo_box","kami_atmo_mood",40,width=465,height=27,
    control_name="KAMIAtmosphereMood",
    tool_tip="Only modifies your own viewer-local sky. Does not change parcel or estate environment.")
for name in ["Night Studio","Golden Hour","Overcast Cinema","Luxury Interior","Dark Cinematic"]:
    ET.SubElement(combo,"combo_box.item",{"label":name,"value":name})
strength=control("combo_box","kami_atmo_strength",75,width=465,height=26,
    control_name="KAMIAtmosphereStrength",tool_tip="Choose 25-100% of the desired look, based on the original current sky.")
for label,value in [("Subtle atmosphere — 25%","0.25"),("Balanced — 50%","0.5"),
                    ("Cinematic — 75%","0.75"),("Full atmosphere — 100%","1")]:
    ET.SubElement(strength,"combo_box.item",{"label":label,"value":value})
duration=control("combo_box","kami_atmo_fade",110,width=465,height=26,
    control_name="KAMIAtmosphereFade",tool_tip="Smooth viewer-local EEP transition, 0-10 seconds.")
for label,value in [("Instant cut","0"),("2-second blend","2"),("3-second blend","3"),
                    ("5-second blend","5"),("10-second blend","10")]:
    ET.SubElement(duration,"combo_box.item",{"label":label,"value":value})
control("button","kami_atmo_apply",145,width=223,height=29,
    label="Preview atmosphere")
control("button","kami_atmo_restore",178,width=223,height=29,
    label="Restore my environment")
control("text","kami_atmo_status",213,width=465,height=42,wrap="true",
    text_color="0.8 0.91 1 1").text="Choose a mood and preview it locally; previous local EEP can be restored."
control("text","kami_atmo_note",253,width=465,height=34,wrap="true",
    text_color="0.65 0.76 0.86 1").text=("SL-compatible: no estate/parcel edits, "
    "no custom server lighting, no extra GPU render pass or 4K/FPS changes.")
ET.indent(doc,space=" ")
doc.write(ui,encoding="utf-8",xml_declaration=True)

prefs=V/"llfloaterpreference.cpp"
s=prefs.read_text(encoding="utf-8-sig")
anchor='#include "llfloaterpreference.h"'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'\n#include "kami_atmosphere.h"',1)
anchor="bool LLFloaterPreference::postBuild()\n{"
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'''
    if(auto* preview=findChild<LLButton>("kami_atmo_apply"))
        preview->setClickedCallback([](LLUICtrl*,const LLSD&) {
            KAMIAtmosphere::apply();
        });
    if(auto* reset=findChild<LLButton>("kami_atmo_restore"))
        reset->setClickedCallback([](LLUICtrl*,const LLSD&) {
            KAMIAtmosphere::restore();
        });
''',1)
anchor="void LLFloaterPreference::draw()\n{"
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'''
    // A status-only refresh. No scene render, shader work, or EEP update.
    static F64 lastAtmosphereUI=0.;
    const F64 nowAtmosphere=KAMISignature::seconds();
    if(nowAtmosphere-lastAtmosphereUI>=1.)
    {
        lastAtmosphereUI=nowAtmosphere;
        if(auto* text=findChild<LLTextBox>("kami_atmo_status"))
            text->setText(LLStringExplicit(
                gSavedSettings.getString("KAMIAtmosphereStatus")));
    }
''',1)
prefs.write_text(s,encoding="utf-8")

# Physical portable staging evidence, no shaders or user-specific files.
SKIN.parent.parent.joinpath("KAMI-ATMOSPHERE-ENGINE.txt").write_text(
    "KAMI Beyond Reality | Atmosphere Engine | viewer-side EEP local preview\n"
    "Night Studio, Golden Hour, Overcast Cinema, Luxury Interior, Dark Cinematic.\n"
    "User-selected EEP intensity and transition duration; broken optional bloom removed.\n"
    "Preview/Restore preserve prior ENV_LOCAL day or fixed sky/water and selection.\n"
    "No parcel/region mutation, asset uploads, FPS cap, 4K changes, or extra passes.\n",
    encoding="utf-8")

ET.parse(settings);ET.parse(ui)
print("Native KAMI Atmosphere Engine implemented in Fire Garden Display (5 local EEP moods, blend, restore; broken bloom control retired).")
