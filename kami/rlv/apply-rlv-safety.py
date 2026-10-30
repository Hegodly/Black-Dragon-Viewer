#!/usr/bin/env python3
"""Install KAMI's wearer-owned RLV safety UI, with native engine fail-closed.

The current Black Dragon source has obsolete RLVa XUI files but no command
handler. Do not pretend those options enable a working RLVa engine.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
VIEW=ROOT/"indra/newview"
SKIN=VIEW/"skins/default/xui/en"
APP=VIEW/"app_settings"

header=VIEW/"kami_rlv_safety.h"
shutil.copyfile(ROOT/"kami/rlv/kami_rlv_safety.h",header)

def check(cond,message):
    if not cond: raise RuntimeError("KAMI RLV fail-closed: "+message)

# KAMI future port options are only security policy storage, never a protocol
# implementation. Nothing may enable RLV until a reviewed C++ command engine
# routes *all* incoming commands through a per-source approval gate.
defs={
    "KAMIRLVEmergencyBlock":("Boolean","integer","1",1,"Master wearer-owned RLV emergency block. Active at startup."),
    "KAMIRLVRequireConsent":("Boolean","integer","1",1,"Future RLV engine: explicit per-source and per-command consent required."),
    "KAMIRLVAllowRelay":("Boolean","integer","0",1,"Future RLV: deny commands forwarded from third-party objects by default."),
    "KAMIRLVAllowTeleport":("Boolean","integer","0",1,"Future RLV: deny teleport and forced movement by default."),
    "KAMIRLVAllowInventory":("Boolean","integer","0",1,"Future RLV: deny force-wear, forced detach and inventory changes by default."),
    "KAMIRLVAllowCommunication":("Boolean","integer","0",1,"Future RLV: deny restrictions on messaging and communication by default."),
    "KAMIRLVAllowCamera":("Boolean","integer","0",1,"Future RLV: deny camera control and view restrictions by default."),
    "KAMIRLVAllowEnvironment":("Boolean","integer","0",1,"Future RLV: deny changing local EEP or environment by default."),
    "KAMIRLVStatus":("String","string","RLVa runtime not installed. All RLV commands unavailable.",0,
                    "Wearer-visible security state; never claim functionality before handler integration."),
}
settings=APP/"settings_blackdragon.xml"
doc=ET.parse(settings);mp=doc.getroot().find("map")
check(mp is not None,"Black Dragon global settings map missing")
existing={n.text for n in mp if n.tag=="key"}
check(not (existing & set(defs)), "Duplicate RLV policy settings")
for k,(typ,tag,value,persist,desc) in defs.items():
    key=ET.Element("key");key.text=k
    contents=ET.Element("map")
    for elem,txt in (("key","Comment"),("string",desc),("key","Persist"),
                     ("integer",str(persist)),("key","Type"),("string",typ),
                     ("key","Value"),(tag,value)):
        child=ET.SubElement(contents,elem);child.text=txt
    mp.insert(0,contents);mp.insert(0,key)
ET.indent(doc,space=" ")
doc.write(settings,encoding="utf-8",xml_declaration=True)

# The old RLVa settings file is not currently included in settings.xml; harden
# its defaults anyway, so future accidental reactivation cannot start ON.
legacy=APP/"settings_rlva.xml"
doc=ET.parse(legacy);mp=doc.getroot().find("map")
check(mp is not None,"Legacy RLVa defaults missing")
children=list(mp)
harden={
    "RestrainedLove":"0",
    "RLVaEnableTemporaryAttachments":"0",
    "RLVaExperimentalCommands":"0",
    "RLVaEnableSharedWear":"0",
    "RLVaEnableIMQuery":"0",
    "RLVaTopLevelMenu":"0",
    "RestrainedLoveNoSetEnv":"1",
    "RestrainedLoveForbidGiveToRLV":"1",
}
for k,v in harden.items():
    found=[i for i,n in enumerate(children) if n.tag=="key" and n.text==k]
    check(len(found)==1,"Legacy RLVa setting missing "+k)
    m=children[found[0]+1]
    ns=list(m)
    values=[ns[i+1] for i,e in enumerate(ns[:-1]) if e.tag=="key" and e.text=="Value"]
    check(len(values)==1,"Bad RLVa value setting "+k)
    values[0].text=v
ET.indent(doc,space=" ")
doc.write(legacy,encoding="utf-8",xml_declaration=True)

panel=SKIN/"panel_preferences_kami_rlv_safety.xml"
shutil.copyfile(ROOT/"kami/rlv/panel_preferences_kami_rlv_safety.xml",panel)
ET.parse(panel)

prefs=SKIN/"floater_preferences.xml"
doc=ET.parse(prefs);root=doc.getroot()
tabs=[n for n in root.iter("tab_container")]
check(len(tabs)==1,"Expected one native Preferences tab group")
t=tabs[0]
check(not any(n.get("name")=="kami_rlv_safety" for n in t),"Duplicate RLV safety tab")
after=next((i for i,n in enumerate(t) if n.get("name")=="viewer"),None)
check(after is not None,"Viewer tab missing")
entry=ET.Element("panel",{"class":"panel_preference",
    "filename":"panel_preferences_kami_rlv_safety.xml",
    "label":"RLV Safety","layout":"topleft","name":"kami_rlv_safety"})
t.insert(after+1,entry)
ET.indent(doc,space=" ")
doc.write(prefs,encoding="utf-8",xml_declaration=True)

# The safety button is real; it is not an inert 'disable' checkbox.
prefcpp=VIEW/"llfloaterpreference.cpp"
s=prefcpp.read_text(encoding="utf-8-sig")
inc='#include "llfloaterpreference.h"'
check(s.count(inc)==1,"Preferences header insertion point changed")
s=s.replace(inc,inc+'\n#include "kami_rlv_safety.h"',1)
anchor='bool LLFloaterPreference::postBuild()\n{'
check(s.count(anchor)==1,"Preferences post-build hook missing")
s=s.replace(anchor,anchor+'''
    // Always accessible in Preferences, even without an RLVa engine.
    if (auto* release=findChild<LLButton>("kami_rlv_emergency_release"))
        release->setClickedCallback([](LLUICtrl*,const LLSD&) {
            KAMIRLVSafety::emergencyRelease();
        });
''',1)
# Status text reflects emergency releases invoked from the top menu as well.
# Poll only with the existing once-per-second Preferences refresh.
draw='''void LLFloaterPreference::draw()
{
    if (mUpdateTimer.getElapsedTimeF32() > 1.f)
    {
        mUpdateTimer.reset();'''
# Black Dragon's C++ indents this block with tabs, not spaces.
draw_actual='''\tif (mUpdateTimer.getElapsedTimeF32() > 1.f)
\t{
\t\tmUpdateTimer.reset();'''
check(s.count(draw_actual)==1,"Preferences status update anchor changed")
s=s.replace(draw_actual,draw_actual+'''
        if (auto* rlvStatus=findChild<LLTextBox>("kami_rlv_status"))
            rlvStatus->setText(LLStringExplicit(
                gSavedSettings.getString("KAMIRLVStatus")));''',1)
prefcpp.write_text(s,encoding="utf-8")

# Separate native top-level emergency menu, not buried in the old (commented)
# RLVa menu, so users never need to depend on a scripted escape/safeword.
menu=SKIN/"menu_viewer.xml"
doc=ET.parse(menu);root=doc.getroot()
me=[n for n in root if n.tag=="menu" and n.get("name")=="Me"]
check(len(me)==1,"KAMI native menu missing")
m=ET.Element("menu_item_call",{
    "label":"RLV EMERGENCY RELEASE - BLOCK ALL",
    "name":"KAMI RLV Emergency Release",
    "shortcut":"control|alt|shift|R"})
cb=ET.SubElement(m,"menu_item_call.on_click")
cb.set("function","KAMI.RLV.EmergencyRelease")
me[0].insert(0,m)
ET.indent(doc,space=" ")
doc.write(menu,encoding="utf-8",xml_declaration=True)

menucpp=VIEW/"llviewermenu.cpp"
s=menucpp.read_text(encoding="utf-8-sig")
inc='#include "llviewermenu.h"'
check(s.count(inc)==1,"Native menu include changed")
s=s.replace(inc,inc+'\n#include "kami_rlv_safety.h"',1)
anchor='void initialize_menus()\n{'
check(s.count(anchor)==1,"Menu registration entry changed")
new='''// KAMI native user-only emergency action. This lives OUTSIDE any RLV command
// handler and may not be intercepted, hidden or disabled by a script.
class KAMIRLVEmergencyReleaseMenu : public view_listener_t
{
    bool handleEvent(const LLSD&) override
    {
        KAMIRLVSafety::emergencyRelease();
        return true;
    }
};

'''
s=s.replace(anchor,new+anchor,1)
hook='    view_listener_t::addMenu(new LLShowHelp(), "ShowHelp");'
check(s.count(hook)==1,"Generic native menu registration changed")
s=s.replace(hook,
    '    view_listener_t::addMenu(new KAMIRLVEmergencyReleaseMenu(), "KAMI.RLV.EmergencyRelease");\n'+hook,1)
menucpp.write_text(s,encoding="utf-8")

# Untrusted settings or command-line overrides cannot enable a leftover
# legacy RLV setting when a logged-in session starts. This current build
# has no RLVa dispatch processor, so hard-blocking is safe and explicit.
app=VIEW/"llappviewer.cpp"
s=app.read_text(encoding="utf-8-sig")
anchor='''#if AL_VIEWER_EVENT_RECORDER
    if (clp.hasOption("logevents"))'''
check(s.count(anchor)==1,"Viewer post-command-line anchor missing")
s=s.replace(anchor,'''
    // KAMI RLV safe startup: all file, session AND command-line overrides
    // have been applied. Nothing can silently revive the missing RLVa engine.
    if (auto* legacy=gSavedSettings.getControl("RestrainedLove"))
        legacy->setValue(false);
    if (auto* blocked=gSavedSettings.getControl("KAMIRLVEmergencyBlock"))
        blocked->setValue(true);

'''+anchor,1)
app.write_text(s,encoding="utf-8")

for p in (settings,legacy,prefs,menu,panel):ET.parse(p)
print("KAMI RLV safety installed: no engine, safe startup, wearer panic menu and Preferences control.")
