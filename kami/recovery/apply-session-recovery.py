#!/usr/bin/env python3
"""Install native KAMI Session Recovery in the real Windows viewer.

This runs after Fire Garden, signature and atmosphere patches.
Snapshots are opt-in; chat/IM drafts and secrets intentionally excluded.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
VIEW=R/"indra/newview"
XUI=VIEW/"skins/default/xui/en"

shutil.copyfile(R/"kami/recovery/kami_recovery.h",VIEW/"kami_recovery.h")

settings=VIEW/"app_settings/settings_blackdragon.xml"
doc=ET.parse(settings);root=doc.getroot().find("map")
assert root is not None
new=[
 ("KAMIRecoveryEnabled","Boolean","integer","0",1,"Opt-in local snapshot of viewer graphics/profile/atmosphere choices every 20 seconds during in-world use."),
 ("KAMIRecoveryIncludeCamera","Boolean","integer","0",1,"Explicit opt-in to store Camera Director's saved global keyframes (potentially private locations) in the recovery file."),
 ("KAMIRecoveryStatus","String","string","Session Recovery OFF. Enable to save recovery points locally.",0,"Text shown in KAMI Session Recovery status field."),
]
for name,typ,tag,value,persist,comment in new:
    assert not any(x.tag=="key" and x.text==name for x in root),"duplicate "+name
    key=ET.Element("key");key.text=name
    body=ET.Element("map")
    for t,v in [("key","Comment"),("string",comment),("key","Persist"),("integer",str(persist)),
                ("key","Type"),("string",typ),("key","Value"),(tag,value)]:
        x=ET.SubElement(body,t);x.text=v
    root.insert(0,body);root.insert(0,key)
ET.indent(doc,space=" ")
doc.write(settings,encoding="utf-8",xml_declaration=True)

app=VIEW/"llappviewer.cpp"
s=app.read_text(encoding="utf-8-sig")
inc='#include "llappviewer.h"'
assert s.count(inc)==1
s=s.replace(inc,inc+'\n#include "kami_recovery.h"',1)
anchor='''    LLFrameTimer::updateFrameCount();
    LLEventTimer::updateClass();'''
assert s.count(anchor)==1,("app idle anchor",s.count(anchor))
s=s.replace(anchor,anchor+'''
    // Single main-thread Recovery check. Does nothing until in-world,
    // enabled, and either a 20s snapshot interval or an abnormal prior exit.
    KAMIRecovery::tick();''',1)
app.write_text(s,encoding="utf-8")

prefs=VIEW/"llfloaterpreference.cpp"
s=prefs.read_text(encoding="utf-8-sig")
inc='#include "llfloaterpreference.h"'
assert s.count(inc)==1
s=s.replace(inc,inc+'\n#include "kami_recovery.h"',1)
anchor='bool LLFloaterPreference::postBuild()\n{'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'''
    auto onRecovery=[this](const char* name,auto action) {
        if (auto* button=findChild<LLButton>(name))
            button->setClickedCallback([action](LLUICtrl*,const LLSD&) { action(); });
    };
    onRecovery("kami_recovery_save",[] { KAMIRecovery::save(); });
    onRecovery("kami_recovery_restore",[] { KAMIRecovery::restore(); });
    onRecovery("kami_recovery_discard",[] { KAMIRecovery::discard(); });
''',1)
anchor='void LLFloaterPreference::draw()\n{'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'''
    static F64 lastRecoveryStatus=0.;
    const F64 nowRecovery=KAMISignature::seconds();
    if(nowRecovery-lastRecoveryStatus>=1.)
    {
        lastRecoveryStatus=nowRecovery;
        if(auto* label=findChild<LLTextBox>("kami_recovery_status"))
            label->setText(LLStringExplicit(
                gSavedSettings.getString("KAMIRecoveryStatus")));
    }
''',1)
prefs.write_text(s,encoding="utf-8")

xml=XUI/"panel_preferences_graphics1.xml"
doc=ET.parse(xml);root=doc.getroot()
ps=[x for x in root.iter() if x.tag=="layout_panel" and x.get("name")=="kombatant_display_quick"]
assert len(ps)==1,"missing Fire Garden Display scroll"
p=ps[0];height=int(p.get("height"))
assert height>=4500,height
delta=260
p.set("height",str(height+delta))
parents=[x for x in root.iter() if x is not p and x.get("height")==str(height+4)]
assert parents,("missing Fire Garden scroll size",height)
for x in parents:x.set("height",str(height+4+delta))
def control(tag,name,offset,**kwargs):
    return ET.SubElement(p,tag,{"name":name,"left":"12","top":str(height+offset),
        **{k:str(v) for k,v in kwargs.items()}})
control("text","kami_recovery_title",8,width=465,height=25,font="SansSerifLarge",
        text_color="0.54 0.77 0.98 1").text="KAMI SESSION RECOVERY  |  Crash-safe local settings"
control("check_box","kami_recovery_enabled",41,width=465,height=23,
        label="Enable local recovery snapshots (20-second intervals)",
        control_name="KAMIRecoveryEnabled",
        tool_tip="When in-world, saves local graphics/profile and atmosphere choices. Off by default. No network transmission.")
control("check_box","kami_recovery_camera",71,width=465,height=23,
        label="Include saved Camera Director keyframes (private locations)",
        control_name="KAMIRecoveryIncludeCamera",
        tool_tip="Off by default. If enabled, camera keyframes may include private region/position data in a local plaintext file.")
control("button","kami_recovery_save",108,width=230,height=28,label="Save recovery point now")
control("button","kami_recovery_restore",140,width=230,height=28,label="Restore last snapshot")
control("button","kami_recovery_discard",172,width=230,height=28,label="Delete recovery files")
control("text","kami_recovery_status",206,width=465,height=49,wrap="true",
        text_color="0.75 0.86 0.96 1").text=(
        "OFF by default. After a detected crash, KAMI offers Restore or Keep Current. "
        "Chat, passwords, inventory and server actions are never captured.")
ET.indent(doc,space=" ")
doc.write(xml,encoding="utf-8",xml_declaration=True)

notify=XUI/"notifications.xml";doc=ET.parse(notify);root=doc.getroot()
assert not [n for n in root if n.get("name")=="KAMIRecoveryOffer"]
n=ET.SubElement(root,"notification",{"name":"KAMIRecoveryOffer",
    "type":"alertmodal","icon":"alertmodal.tga"})
n.text=("KAMI detected that the previous viewer session did not close normally.\n\n"
        "A local recovery point is available for graphics, profile and atmosphere "
        "choices (plus saved camera keyframes only if you opted in).\n\n"
        "Restore these viewer settings? No chat, account credentials, inventory "
        "or simulator actions are recovered.")
ET.SubElement(n,"usetemplate",{"name":"okcancelbuttons",
    "yestext":"Restore settings","notext":"Keep current"})
ET.indent(doc,space=" ")
doc.write(notify,encoding="utf-8",xml_declaration=True)
# Native staging evidence, not a fake control or a workaround for SL servers.
(VIEW/"skins/default/KAMI-SESSION-RECOVERY.txt").write_text(
    "KAMI Beyond Reality | Session Recovery\n"
    "Opt-in snapshots: only graphics, selected profile, atmosphere choices.\n"
    "Camera Director's stored keyframes are optional and off by default.\n"
    "SL's original last-execution markers trigger a one-time Restore/Keep Current prompt.\n"
    "No chat/IM text, login data, asset inventory, uploads, or simulator state.\n"
    "Never auto-restore world environment or move avatar/camera after login.\n",
    encoding="utf-8")

ET.parse(xml);ET.parse(notify);ET.parse(settings)
print("KAMI native Session Recovery installed: opt-in 20s local snapshots, existing SL crash marker, native Restore/Keep Current prompt and Fire Garden controls.")
