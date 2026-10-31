#!/usr/bin/env python3
"""Verify that KAMI ships shader resources for external GShade/ReShade only.

Static checks cannot validate effects under a user's runtime in live Second Life.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def read(p): return (R/p).read_text(encoding="utf-8-sig")
def check(name, condition):
    if not condition: raise RuntimeError("KAMI external FX contract failed: "+name)
    print("PASS", name)

fx=read("kami/gshade/effects/KAMI-FX.fx")
names=re.findall(r"^KAMI_PASS\((KAMI_[\w]+),(\w+)\)$",fx,re.M)
check("20 distinct original KAMI FX techniques",
      len(names)==20 and len(set(n for n,_ in names))==20)
check("techniques have shader implementations",
      all("float4 "+shader+"(" in fx for _,shader in names))
check("KAMI FX is original and does not bundle proprietary MXAO",
      "Copyright (c) 2026 KAMI" in fx and "MartysMods_MXAO" not in fx)
for filename in ("FireGarden.ini","Cinematic.ini","Clean4K.ini","FX-LICENSE.txt"):
    check("retained external FX resource "+filename,
          (R/"kami/gshade/effects"/filename).is_file())
check("starter effects disabled by default",
      re.search(r"(?m)^Techniques=\s*$",read("kami/gshade/effects/FireGarden.ini")) is not None)
check("no proprietary MXAO shader in KAMI FX",
      not list((R/"kami/gshade/effects").rglob("MartysMods_MXAO.fx")))
workflow=read(".github/workflows/kombatant-windows.yaml")
check("no embedded runtime build/integration",
      all(name not in workflow for name in (
          "apply-private-mxao.py","apply-gshade-compat.py","build-runtime.ps1")))
stage=read("kami/packaging/stage-portable-data.py")
check("optional FX source staged directly as part of portable build",
      "stage-portable-data.py" in workflow and
      "fx_src = R / 'kami/gshade/effects'" in stage and
      "fx_dst = out / 'KAMI-FX'" in stage)
check("no GShade/FX user feature installer in portable release",
      "stage-external-assets.ps1" not in workflow and
      "Install-GShade-Assets.ps1" not in workflow and
      "Install-GShade-Assets.ps1" in stage and
      "for old_script in" in stage and
      "finalize-kami-standalone.py" in workflow)
check("viewer does not load a GShade/MXAO runtime",
      "configurePrivateMXAO" not in read("indra/llwindow/llwindow.h") and
      "configureGShade" not in read("indra/llwindow/llwindow.h") and
      "KAMIMXAO::" not in read("indra/llwindow/llwindowwin32.cpp") and
      "configureGShade(" not in read("indra/newview/llviewerdisplay.cpp"))
ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
removed={
    "kami_mxao_heading","kami_mxao_enable","kami_mxao_amount",
    "kami_mxao_radius","kami_mxao_quality","kami_mxao_overlay",
    "kami_mxao_status","kami_mxao_install","kami_mxao_help",
    "kami_gshade_heading","kami_gshade_toggle","kami_gshade_overlay",
    "kami_gshade_import","kami_fx_enable","kami_gshade_status",
    "kami_gshade_help",
}
check("no broken internal MXAO/GShade controls",
      not any(node.get("name") in removed for node in ui.iter()))
check("GShade-compatible source and presets staged by Python only",
      all(name in stage for name in
          ("KAMI-FX.fx","FireGarden.ini","Cinematic.ini","Clean4K.ini","FX-LICENSE.txt")))
check("private and embedded runtime flags not in final settings",
      "<key>KAMIMXAO</key>" not in read("indra/newview/app_settings/settings_blackdragon.xml") and
      "<key>KAMIGShadeCompatibility</key>" not in read("indra/newview/app_settings/settings_blackdragon.xml"))
print("Optional external FX resources verified as source-only; native KAMI needs no GShade injector or feature installers.")
