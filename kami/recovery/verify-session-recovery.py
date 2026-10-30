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
# Audit calls, not comments: the documentation explicitly mentions that
# teleport is unsupported, which is not evidence of a teleport call.
check("no unsolicited camera move, teleport or shared environment modification",
      all(k not in code for k in ('setCameraPosAndFocusGlobal(',
        'teleportVia','teleportHome(', 'teleportRequest(', 'setEnvironment(',
        'updateRegion(', 'updateParcel(')))
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
# Session Recovery is the last KAMI quick section, but native Black Dragon
# Basic/Advanced/Post graphics panels continue below it. Neither the inner
# layout stack nor outer scroll view may cut them off.
quick=ui_by_name.get("kombatant_display_quick")
stack=ui_by_name.get("gfx_stack")
scroll=ui_by_name.get("gfx_scroll_panel")
check("complete native Display scroll hierarchy exists",
      quick is not None and stack is not None and scroll is not None)
stack_panels=[n for n in stack if n.tag=="layout_panel"]
check("original Black Dragon graphics panels retained after Recovery",
      {"basic_layout_panel","advanced_layout_panel","finetuning_layout_panel"}
      <= {n.get("name") for n in stack_panels})
check("full graphics layout can fit all native and KAMI panels",
      int(stack.get("height"))>=sum(int(n.get("height","0")) for n in stack_panels)+64)
check("scroll content includes KAMI quick panel and native title bars",
      int(scroll.get("height"))>=int(quick.get("height"))+66)
recovery_status=ui_by_name["kami_recovery_status"]
check("Session Recovery content is not clipped",
      int(recovery_status.get("top"))+int(recovery_status.get("height"))
      <= int(quick.get("height")))
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
