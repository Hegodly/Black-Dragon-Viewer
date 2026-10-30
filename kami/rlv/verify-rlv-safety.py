#!/usr/bin/env python3
"""Fail CI if KAMI ships a pretend or bypassable RLVa integration.

Static tests guard the current no-engine safety foundation. A future RLVa
implementation requires separate runtime tests with consent and panic during
real command execution; tests here are not proof of RLVa compatibility.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
V=R/"indra/newview"
def text(p):return (R/p).read_text(encoding="utf-8-sig")
def check(label,ok):
    if not ok:raise AssertionError("KAMI RLV safety: "+label)
    print("PASS:",label)

def settings_map(path):
    mp=ET.parse(R/path).getroot().find("map")
    assert mp is not None
    children=list(mp)
    result={}
    for i,ch in enumerate(children[:-1]):
        if ch.tag!="key":continue
        spec=children[i+1]
        if spec.tag!="map":continue
        fields=list(spec);values={}
        for k in range(len(fields)-1):
            if fields[k].tag=="key":values[fields[k].text]=fields[k+1].text
        result[ch.text]=values
    return result

k=settings_map("indra/newview/app_settings/settings_blackdragon.xml")
legacy=settings_map("indra/newview/app_settings/settings_rlva.xml")
check("master RLV deny switch defaults ON",k["KAMIRLVEmergencyBlock"]["Value"]=="1")
check("user permission required by default",k["KAMIRLVRequireConsent"]["Value"]=="1")
check("every unsafe category DENIED by default",
      all(k["KAMIRLVAllow"+category]["Value"]=="0" for category in
          ("Relay","Teleport","Inventory","Communication","Camera","Environment")))
check("legacy processor disabled by default",legacy["RestrainedLove"]["Value"]=="0")
check("legacy high-risk auto-features disabled",
      all(legacy[key]["Value"]=="0" for key in
          ("RLVaEnableTemporaryAttachments","RLVaExperimentalCommands",
           "RLVaEnableSharedWear","RLVaEnableIMQuery","RLVaTopLevelMenu")))
check("legacy protection switches enabled by default",
      all(legacy[key]["Value"]=="1" for key in
          ("RestrainedLoveNoSetEnv","RestrainedLoveForbidGiveToRLV")))
check("core setting does not actually load the legacy RLVa package",
      "<string>settings_rlva.xml</string>" not in text("indra/newview/app_settings/settings.xml").replace(
         "<!--<string>settings_rlva.xml</string>-->",""))
header=text("indra/newview/kami_rlv_safety.h")
check("native fail-closed header installed from reviewed source",
      header==text("kami/rlv/kami_rlv_safety.h"))
# LLControlGroup::getControl returns LLControlVariablePtr, not a raw pointer.
# MSVC C3535/C2440 arises if future RLV patches reintroduce 'auto*'.
invalid_control_pointer = re.compile(r"auto\\s*\\*\\s*\\w+\\s*=\\s*gSavedSettings\\.getControl\\s*\\(")
check("RLV smart-pointer type is preserved across native header and source installer",
      not invalid_control_pointer.search(header) and
      not invalid_control_pointer.search(text("kami/rlv/apply-rlv-safety.py")))
check("command gate unconditionally refuses requests",
      "inline bool allowInboundCommand()" in header and
      "return false;" in header.split("inline bool allowInboundCommand()",1)[1] and
      "kNativeRLVaEngineInstalled = false" in header)
check("native emergency resets all future permissions and legacy RLV",
      all(('gSavedSettings.getControl("'+key+'")') in header
          for key in ("RestrainedLove","KAMIRLVEmergencyBlock","KAMIRLVAllowRelay",
                      "KAMIRLVAllowTeleport","KAMIRLVAllowInventory",
                      "KAMIRLVAllowCommunication","KAMIRLVAllowCamera",
                      "KAMIRLVAllowEnvironment")))
app=text("indra/newview/llappviewer.cpp")
check("startup after overrides forces fail-closed state",
      app.index('loadSettingsFromDirectory("UserSession");') <
      app.index('tempSetControl(name, value)') <
      app.index('gSavedSettings.getControl("RestrainedLove")') and
      'gSavedSettings.getControl("KAMIRLVEmergencyBlock")' in app)
prefsxml=ET.parse(V/"skins/default/xui/en/floater_preferences.xml").getroot()
tabs=[n for n in prefsxml.iter("panel")]
check("visible native RLV Safety tab, old broken enable tab remains hidden",
      any(x.get("name")=="kami_rlv_safety" and
          x.get("filename")=="panel_preferences_kami_rlv_safety.xml"
          for x in tabs) and
      not any(x.get("filename")=="panel_preferences_rlva.xml" for x in tabs))
panel=ET.parse(V/"skins/default/xui/en/panel_preferences_kami_rlv_safety.xml").getroot()
check("clearly states engine unavailable and does not expose enable checkbox",
      "RLVa engine unavailable" in "".join(panel.itertext()) and
      not any(x.tag=="check_box" for x in panel.iter()))
check("panic button exists in native Preferences",
      any(x.tag=="button" and x.get("name")=="kami_rlv_emergency_release"
          for x in panel.iter()))
menu=ET.parse(V/"skins/default/xui/en/menu_viewer.xml").getroot()
item=[n for n in menu.iter("menu_item_call") if n.get("name")=="KAMI RLV Emergency Release"]
check("always-visible top-level KAMI panic menu and dedicated shortcut",
      len(item)==1 and item[0].get("shortcut")=="control|alt|shift|R" and
      len([n for n in item[0] if n.get("function")=="KAMI.RLV.EmergencyRelease"])==1 and
      any(menuItem is item[0] for parent in menu if parent.get("name")=="Me"
          for menuItem in parent))
cpp=text("indra/newview/llviewermenu.cpp")
prefs=text("indra/newview/llfloaterpreference.cpp")
check("panic menu registers actual native callback",
      'view_listener_t::addMenu(new KAMIRLVEmergencyReleaseMenu(), "KAMI.RLV.EmergencyRelease");' in cpp and
      'KAMIRLVSafety::emergencyRelease();' in cpp)
check("Preferences emergency button dispatches actual native code",
      'findChild<LLButton>("kami_rlv_emergency_release")' in prefs and
      'KAMIRLVSafety::emergencyRelease();' in prefs)
check("wearer can see status from native menu emergency",
      'findChild<LLTextBox>("kami_rlv_status")' in prefs and
      'gSavedSettings.getString("KAMIRLVStatus")' in prefs)
# A newly introduced RLVa parser without comprehensive authorization is a
# SECURITY REGRESSION. Block that build until consent tests and gateway exist.
processors=[p for p in V.iterdir() if p.is_file() and p.suffix in (".h",".cpp")
            and p.name.lower().startswith("rlv")]
check("no unreviewed RLVa command handler added",not processors)
check("future consent/security design is in-tree",
      (R/"kami/rlv/SECURITY.md").is_file() and
      all(word in text("kami/rlv/SECURITY.md").lower()
          for word in ("object uuid","emergency release","relay","consent")))
print("RLV wearer safety foundation validated. RLVa protocol execution is NOT installed.")
