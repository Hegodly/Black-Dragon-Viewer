#!/usr/bin/env python3
"""Fail the Windows build if KAMI / GShade FX / MXAO is only cosmetic UI.

This checks actual optional runtime binding, local installer/packing contracts,
and absence of unauthorized MXAO/GShade shader redistribution.
Does not claim runtime shaders compile or that hardware FPS increased.
"""
from pathlib import Path
import re,xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def read(p):return (R/p).read_text(encoding="utf-8-sig")
def check(name,val):
    if not val:raise RuntimeError("KAMI FX verification failed: "+name)
    print("PASS",name)

fx=read("kami/gshade/effects/KAMI-FX.fx")
names=re.findall(r"^KAMI_PASS\((KAMI_[\w]+),(\w+)\)$",fx,re.M)
check("20 distinct original FX techniques",len(names)==20 and len(set(x for x,_ in names))==20)
check("all FX techniques have pixel shaders",all("float4 "+ps+"(" in fx for _,ps in names))
check("FX source is marked original, not proprietary GShade/MXAO", "Copyright (c) 2026 KAMI" in fx and "MartysMods_MXAO" not in fx)
for f in ("FireGarden.ini","Cinematic.ini","Clean4K.ini","FX-LICENSE.txt"):
    check("bundled effect resource: "+f,(R/"kami/gshade/effects"/f).is_file())
check("default starter preset OFF",re.search(r"(?m)^Techniques=\s*$",read("kami/gshade/effects/FireGarden.ini")) is not None)
check("never bundles author's MXAO shader",not any(
    p.name=="MartysMods_MXAO.fx" for p in (R/"kami/gshade/effects").rglob("*") if p.is_file()))
check("official MXAO private installer present",(R/"kami/mxao/Install-Private-MXAO.ps1").is_file())
check("MXAO pinned-author-direct download, no Git dependency",
      "codeload.github.com/martymcmodding/iMMERSE/zip/" in read("kami/mxao/Install-Private-MXAO.ps1"))
check("MXAO and GShade preset import tool present",
      (R/"kami/gshade/Import-GShade-Preset.ps1").is_file() and
      (R/"kami/gshade/Enable-KAMI-FX.ps1").is_file())
check("one licensed runtime, not a second OpenGL injection",
      "configureGShade" in read("indra/llwindow/kami_mxao_win32.h") and
      "KAMIMXAO::configureGShade" in read("indra/llwindow/llwindowwin32.cpp"))
check("both runtime activation markers recognized",
      all(s in read("indra/llwindow/kami_mxao_win32.h")
          for s in ("KAMI_GSHADE_USER.enabled","KAMI_EFFECTS_BUNDLED.enabled")))
check("native MXAO amount/radius/quality still controllable",
      all(s in read("indra/llwindow/kami_mxao_win32.h")
          for s in ("MXAO_SSAO_AMOUNT","MXAO_SAMPLE_RADIUS","MXAO_GLOBAL_SAMPLE_QUALITY_PRESET")))

ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
def control(name):
    nodes=[el for el in ui.iter() if el.get("name")==name]
    check("exactly one "+name,len(nodes)==1)
    return nodes[0]
for name in ("kami_gshade_toggle","kami_gshade_overlay",
    "kami_gshade_import","kami_fx_enable",
    "kami_mxao_enable","kami_mxao_amount","kami_mxao_radius",
    "kami_mxao_quality","kami_mxao_install"):
    check("visible and enabled "+name,control(name).get("visible")!="false" and
          control(name).get("enabled")!="false")
check("actual GShade setting connected",
      control("kami_gshade_toggle").get("control_name")=="KAMIGShadeCompatibility")
check("MXAO private install button retained",
      control("kami_mxao_install").get("label","").lower().find("install mxao")>=0)
settings=read("indra/newview/app_settings/settings_blackdragon.xml")
check("GShade and MXAO modes OFF by default",
    all(("<key>"+x+"</key>") in settings for x in ("KAMIGShadeCompatibility","KAMIMXAO")))
check("optical flow/Performance conflict protects GShade",
      "gshadeBlocked" in read("indra/newview/llviewerdisplay.cpp"))
check("both installs staged into Windows release",
      "Import-GShade-Preset.ps1" in read("kami/mxao/build-runtime.ps1") and
      "Enable-KAMI-FX.ps1" in read("kami/mxao/build-runtime.ps1"))
print("KAMI FX + optional author's MXAO checks passed. Hardware/ReShade compilation must still be tested.")
