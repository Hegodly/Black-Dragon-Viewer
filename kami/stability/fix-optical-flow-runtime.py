#!/usr/bin/env python3
"""Report actual (not just toggled) optical-flow interpolation state in Display."""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
settings=R/"indra/newview/app_settings/settings_blackdragon.xml"
s=settings.read_text(encoding="utf-8-sig")
anchor="<key>KAMIMXAO</key>"
assert s.count(anchor)==1, "Expected optional MXAO to have been integrated"
s=s.replace(anchor,'''<key>KAMIFrameGenerationStatus</key>
  <map><key>Comment</key><string>Actual last optical-flow presentation state; not a frame-rate promise.</string><key>Persist</key><integer>0</integer><key>Type</key><string>String</string><key>Value</key><string>Off</string></map>
  <key>KAMIMXAO</key>''',1)
settings.write_text(s,encoding="utf-8")

display=R/"indra/newview/llviewerdisplay.cpp"
s=display.read_text(encoding="utf-8-sig")
old='''        gViewerWindow->getWindow()->swapBuffers();
    }
'''
assert s.count(old)==1, "Expected one final present inside the display frame"
new='''        gViewerWindow->getWindow()->swapBuffers();
#if LL_WINDOWS
        // Update text only on status changes; a per-frame settings write would
        // emit unnecessary commit signals and harm FPS.
        const std::string actual_fg_status =
            gSavedSettings.getBOOL("KAMIFrameGeneration") &&
            gSavedSettings.getBOOL("KAMIPerformanceMode")
                ? "Paused: legacy Performance Mode disables optical flow."
                : KAMIFrameGen::generator().status();
        if (gSavedSettings.getString("KAMIFrameGenerationStatus") != actual_fg_status)
            gSavedSettings.setString("KAMIFrameGenerationStatus", actual_fg_status);
#endif
    }
'''
s=s.replace(old,new,1)
display.write_text(s,encoding="utf-8")

prefs=R/"indra/newview/llfloaterpreference.cpp"
s=prefs.read_text(encoding="utf-8-sig")
old='''    if (auto* text = findChild<LLTextBox>("kami_mxao_status"))
        text->setText(LLStringExplicit(gSavedSettings.getString("KAMIMXAOStatus")));'''
assert s.count(old)==1
s=s.replace(old,old+'''
    if (auto* text = findChild<LLTextBox>("kami_fg_status"))
        text->setText(LLStringExplicit(gSavedSettings.getString("KAMIFrameGenerationStatus")));''',1)
prefs.write_text(s,encoding="utf-8")

ui=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(ui)
root=tree.getroot()
help_widgets=[x for x in root.iter() if x.get("name")=="kami_fg_help"]
assert len(help_widgets)==1
help=help_widgets[0]
old_top=int(help.get("top"))
help.set("top",str(old_top+32))
help.set("height","56")
help.text=("Optical-flow interpolation inserts an intermediate display image. It is not "
           "NVIDIA DLSS Frame Generation. Real simulation FPS remains unchanged. "
           "Interpolation adds latency and GPU cost; disable for competitive play.")
# Place readout before description in the same scrolling group.
for parent in root.iter():
    if help in list(parent):
        index=list(parent).index(help)
        status=ET.Element("text",name="kami_fg_status",left="12",top=str(old_top),
                          width="465",height="30",wrap="true",mouse_opaque="false")
        status.text="Off"
        parent.insert(index,status)
        break
else:
    raise RuntimeError("Optical-flow status placement failed")
ET.indent(tree,space=" ")
tree.write(ui,encoding="utf-8",xml_declaration=True)
ET.parse(settings);ET.parse(ui)
assert "actual_fg_status" in display.read_text(encoding="utf-8")
assert "kami_fg_status" in prefs.read_text(encoding="utf-8")
print("Optical-flow runtime status now shows Active/Warming/Paused/Unavailable rather than only the checkbox.")
