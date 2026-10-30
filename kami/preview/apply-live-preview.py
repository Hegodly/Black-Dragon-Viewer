#!/usr/bin/env python3
"""Expose current-scene live graphics preview without a second scene render."""
from pathlib import Path
import xml.etree.ElementTree as ET
import shutil
R=Path(__file__).resolve().parents[2]
def edit(path,old,new):
 p=R/path;s=p.read_text(encoding='utf-8-sig')
 if s.count(old)!=1: raise RuntimeError(f'{path}: expected one anchor: {old[:80]}')
 p.write_text(s.replace(old,new,1),encoding='utf-8')
settings='indra/newview/app_settings/settings_blackdragon.xml'
edit(settings,'<map>\n  <key>KAMIFrameGeneration</key>','''<map>
  <key>KAMILiveGraphicsPreview</key>
  <map><key>Comment</key><string>Temporarily fade Preferences to see live graphics edits in the current world scene. Session only; resets whenever Preferences opens or closes.</string><key>Persist</key><integer>0</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>0</integer></map>
  <key>KAMIPreviewOpacity</key>
  <map><key>Comment</key><string>Preferences opacity during live scene preview. Also fades its text and controls. No extra scene rendering or blur pass.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.55</real></map>
  <key>KAMIFrameGeneration</key>''')
shutil.copyfile(R/'kami/preview/kami_preview_opacity.h', R/'indra/newview/kami_preview_opacity.h')
p='indra/newview/llfloaterpreference.cpp'
edit(p, '#include "llfloaterpreference.h"', '#include "llfloaterpreference.h"\n#include "kami_preview_opacity.h"')
edit(p,'    // Presets change real renderer settings; individual controls remain editable.','''    mCommitCallbackRegistrar.add("Pref.KAMIEndPreview", [](LLUICtrl*, const LLSD&) {
        gSavedSettings.setBOOL("KAMILiveGraphicsPreview", false);
    });
    // Presets change real renderer settings; individual controls remain editable.''')
edit(p,'void LLFloaterPreference::onOpen(const LLSD& key)\n{','''void LLFloaterPreference::onOpen(const LLSD& key)
{
    gSavedSettings.setBOOL("KAMILiveGraphicsPreview", false);''')
edit(p,'void LLFloaterPreference::onClose(bool app_quitting)\n{','''void LLFloaterPreference::onClose(bool app_quitting)
{
    gSavedSettings.setBOOL("KAMILiveGraphicsPreview", false);''')
# Local draw context affects only this Preferences window. The rendered world
# remains unmodified, at full opacity, and continues to react to bound controls.
edit(p,'\tLLFloater::draw();\n}\n\nvoid LLFloaterPreference::saveSettings()', '''    const bool preview = gSavedSettings.getBOOL("KAMILiveGraphicsPreview");
    const F32 preview_alpha = preview
        ? llclamp(gSavedSettings.getF32("KAMIPreviewOpacity"), 0.25f, 1.f) : 1.f;
    KAMIPreviewOpacityScope preview_policy(this, preview);
    LLViewDrawContext preview_context(preview_alpha);
    LLFloater::draw();
}

void LLFloaterPreference::saveSettings()''')
p='indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml'
s=(R/p).read_text();start=s.index('<layout_panel name="kombatant_display_quick"');end=s.index('</layout_panel>',start)
a=s[start:end]
assert 'height="4100" auto_resize="false"' in a
# Insert at the top, so preview controls are immediately accessible. Shift all
# existing controls together; retain their exact renderer setting bindings.
import re
a=re.sub(r'top="(\d+)"',lambda m:f'top="{int(m[1])+150}"',a)
a=a.replace('height="4100" auto_resize="false"','height="4250" auto_resize="false"',1)
anchor=a.index('>')+1
a=a[:anchor]+'''
       <check_box name="kami_live_preview" label="Live scene preview (fade settings window)" control_name="KAMILiveGraphicsPreview" left="12" top="8" width="465" height="22" />
       <slider name="kami_preview_opacity" label="Preview panel opacity" control_name="KAMIPreviewOpacity" left="12" top="38" width="465" height="20" label_width="210" min_val="0.25" max_val="1" increment="0.05" decimal_digits="2" />
       <button name="kami_end_preview" label="End preview" left="12" top="66" width="145" height="24"><button.commit_callback function="Pref.KAMIEndPreview" /></button>
       <text name="kami_preview_help" left="12" top="98" width="465" height="48" wrap="true">Change graphics controls to preview your current scene live. Text and controls fade too; raise opacity for readability. Apply/OK keeps edits; Cancel uses normal Preferences rollback. Previewing may briefly rebuild rendering resources.</text>
''' +a[anchor:]
s=s[:start]+a+s[end:];assert s.count('height="4104"')==1
s=s.replace('height="4104"','height="4254"',1);(R/p).write_text(s)
for p in [settings,p]:ET.parse(R/p)
print('Live current-scene preview integrated; controls retain existing renderer bindings')
