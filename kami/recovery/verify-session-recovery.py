#!/usr/bin/env python3
"""KAMI native Session Recovery contracts. Runtime tests still required."""
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
def read(path):return (R/path).read_text(encoding="utf-8-sig")
def check(name,ok):
 if not ok:raise AssertionError("KAMI Recovery: "+name)
 print("PASS:",name)
code=read("indra/newview/kami_recovery.h")
settings=ET.parse(R/"indra/newview/app_settings/settings_blackdragon.xml").getroot()
pairs=list(settings.find("map"))
controls={}
for i in range(len(pairs)-1):
 if pairs[i].tag=="key" and pairs[i+1].tag=="map":
  elements=list(pairs[i+1])
  for j,child in enumerate(elements):
   if child.tag=="key" and child.text=="Value":
    controls[pairs[i].text]=elements[j+1].text
    break
for n in ("KAMIRecoveryEnabled","KAMIRecoveryIncludeCamera","KAMIRecoveryStatus"):
 check("settings key: "+n,n in controls)
check("snapshots opt-in/off by default",controls["KAMIRecoveryEnabled"]=="0")
check("private camera positions not stored by default",controls["KAMIRecoveryIncludeCamera"]=="0")
check("native C++ recovery header staged",code==read("kami/recovery/kami_recovery.h"))
check("original SL last execution markers (not inferred)",'gLastExecEvent != LAST_EXEC_NORMAL' in code and 'isSecondInstance()' in code)
check("real login gating", "STATE_STARTED" in code)
check("user chooses before overwriting older crash snapshot",
      "offerPending=true" in code and "if(offerPending)return;" in code)
check("20s spaced snapshots, no per-frame disk IO","now-lastSample>=20." in code)
check("snapshot has max-size bound and restore schema", '131072' in code and
      'data["schema"].asInteger()==1' in code)
check("snapshot rollback backup copy",all(x in code for x in
 ('file()+".bak"','LLFile::rename(path,backup','LLFile::rename(temp,path')))
check("no secrets, chat text, inventory, avatar network states captured",
      all(x not in code for x in ('getIMText','getChatText','gInventory','PerAccountSettings',
          'getPassword','getAgentSessionId','sendReliableMessage(')))
check("capture only existing native profile/sky and camera settings",
     all(k in code for k in ('KAMISignature::snapshot()','KAMIProfileSelected',
           'KAMIAtmosphereMood','KAMICameraStart','KAMICameraEnd',
           'KAMIRecoveryIncludeCamera')))
check("no unsolicited camera move, teleport or shared environment modification",
      all(k not in code for k in ('setCameraPosAndFocusGlobal',
        'teleport','setEnvironment(','updateRegion(','updateParcel(')))
check("conflicting adaptive performance modes block restore",
      "KAMISignature::conflictingMode()" in code and 'KAMIWorldSmart' in code)
check("real application idle tick",
      read("indra/newview/llappviewer.cpp").count("KAMIRecovery::tick();")==1)
ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
ui_by_name={x.get("name"):x for x in ui.iter() if x.get("name")}
for name,key in (("kami_recovery_enabled","KAMIRecoveryEnabled"),
                 ("kami_recovery_camera","KAMIRecoveryIncludeCamera")):
 check("Fire Garden checkbox "+name, name in ui_by_name and
       ui_by_name[name].get("control_name")==key)
prefs=read("indra/newview/llfloaterpreference.cpp")
for name,action in (("kami_recovery_save","save"),("kami_recovery_restore","restore"),
                    ("kami_recovery_discard","discard")):
 check("real native action button "+name,name in ui_by_name and
       ui_by_name[name].tag=="button" and f'"{name}"' in prefs and
       "KAMIRecovery::"+action+"();" in prefs)
notif=ET.parse(R/"indra/newview/skins/default/xui/en/notifications.xml").getroot()
prompts=[x for x in notif if x.get("name")=="KAMIRecoveryOffer"]
check("Restore/Keep Current built-in SL modal",len(prompts)==1 and
    prompts[0].find("usetemplate") is not None and
    prompts[0].find("usetemplate").get("notext")=="Keep current")
check("snapshot stored locally under user settings",
      'LL_PATH_USER_SETTINGS' in code)
check("shipped documentation",
      (R/"indra/newview/skins/default/KAMI-SESSION-RECOVERY.txt").exists())
print("Session Recovery source contracts passed; Windows compile and crash-restart tests still required.")
