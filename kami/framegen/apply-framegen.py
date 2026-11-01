#!/usr/bin/env python3
"""Install opt-in OpenGL optical-flow interpolation after native graphics patches."""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
def edit(path,old,new):
 p=R/path;s=p.read_text(encoding='utf-8-sig')
 if s.count(old)!=1: raise RuntimeError(f'{path}: expected unique anchor: {old[:70]}')
 p.write_text(s.replace(old,new,1),encoding='utf-8')
shutil.copyfile(R/'kami/framegen/kami_framegen.h',R/'indra/newview/kami_framegen.h')
# Header is included after viewer headers so GL declarations and log macros exist.
p='indra/newview/llviewerdisplay.cpp'
s=(R/p).read_text();pos=s.index('bool gSnapshot = false;')
s=s[:pos]+'#if LL_WINDOWS\n#include "kami_framegen.h"\n#endif\n\n'+s[pos:];(R/p).write_text(s)
edit(p,'''    if (gDisplaySwapBuffers)
    {
        gViewerWindow->getWindow()->swapBuffers();
    }''','''    if (gDisplaySwapBuffers)
    {
#if LL_WINDOWS
        gGL.flush();
        auto* window = gViewerWindow->getWindow();
        const bool enabled = gSavedSettings.getBOOL("KAMIFrameGeneration") &&
            !gSnapshot && !gSavedSettings.getBOOL("KAMIPerformanceMode") && window->getVisible() && !window->getMinimized();
        if (!KAMIFrameGen::generator().present(enabled,
            gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw(),
            [window]() { window->swapBuffers(); }))
#endif
        gViewerWindow->getWindow()->swapBuffers();
    }''')
p='indra/newview/llviewerwindow.cpp'
edit(p,'void LLViewerWindow::stopGL()','''#if LL_WINDOWS
#include "kami_framegen.h"
#endif
void LLViewerWindow::stopGL()''')
edit(p,'        LL_INFOS() << "Shutting down GL..." << LL_ENDL;','''        LL_INFOS() << "Shutting down GL..." << LL_ENDL;
#if LL_WINDOWS
        KAMIFrameGen::generator().release();
#endif''')
p='indra/newview/app_settings/settings_blackdragon.xml'
# Keep frame-generation settings independent of experimental avatar lighting.
# That retired feature is no longer installed; use the stable KAMI auto-tune
# settings entry instead of referring to a deleted control.
edit(p,'  <key>KAMIAutoTune</key>','''  <key>KAMIFrameGeneration</key>
  <map><key>Comment</key><string>Experimental native optical-flow frame interpolation. Not DLSS or FSR. Adds latency and may distort moving UI. Off by default; bypassed below 30 real FPS, on stalls, and above 4096 pixels per axis.</string><key>Persist</key><integer>0</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>0</integer></map>
  <key>KAMIAutoTune</key>''')
p='indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml'
s=(R/p).read_text();start=s.index('<layout_panel name="kombatant_display_quick"');end=s.index('</layout_panel>',start)
a=s[start:end].replace('height="3910" auto_resize="false"','height="4100" auto_resize="false"')
a+='''       <text name="kami_fg_heading" left="12" top="3920" width="465" height="24" font="SansSerifLarge">Experimental native frame generation</text>
       <check_box name="kami_fg" label="Optical-flow interpolation (adds latency / GPU cost)" control_name="KAMIFrameGeneration" left="12" top="3952" width="465" height="22" />
       <text name="kami_fg_help" left="12" top="3982" width="465" height="108" wrap="true">Generates one intermediate image using optical-flow block matching. Not NVIDIA DLSS or AMD FSR Frame Generation. Moving UI and fast motion can distort. Requires 30+ real FPS; disabled after restart. Adds presentation delay. FPS counters show real rendered frames. Start at 1080p; switch off if pacing or image quality worsens.</text>
'''
s=s[:start]+a+s[end:];s=s.replace('height="3914"','height="4104"',1);(R/p).write_text(s)
for p in ['indra/newview/app_settings/settings_blackdragon.xml','indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml']: ET.parse(R/p)
print('KAMI optical-flow frame-generation integration applied')
