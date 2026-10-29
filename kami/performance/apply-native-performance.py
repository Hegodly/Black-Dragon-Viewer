#!/usr/bin/env python3
"""Remove bundled GShade/ReShade integration and enable fully native KAMI controls.

Run AFTER kombatant.patch, apply-upscalers.py, and KAMI branding. Edits are
validated by explicit anchors: fail the build if upstream source diverges.
"""
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[2]

def file(path):
    return ROOT / path

def edit(path, before, after, count=1):
    p=file(path)
    text=p.read_text(encoding="utf-8-sig")
    actual=text.count(before)
    if actual != count:
        raise RuntimeError(f"{path}: expected {count} occurrences of {before[:75]!r}, found {actual}")
    p.write_text(text.replace(before, after, count), encoding="utf-8")
    print("Updated",path)

# Remove all in-process external shader add-on calls, not merely the UI toggle.
edit("indra/llwindow/llwindow.h",
     '    // Optional platform shader runtime. Other platforms retain native effects.\n'
     '    virtual bool configurePostProcessing(bool, const std::string&, bool, bool) { return false; }\n'
     '    virtual std::string getPostProcessingStatus() const { return "Shader runtime is available on Windows only."; }\n\n', '')
edit("indra/llwindow/llwindowwin32.h",
     '    bool configurePostProcessing(bool enabled, const std::string& preset, bool overlay, bool reload) override;\n'
     '    std::string getPostProcessingStatus() const override;\n', '')
edit("indra/llwindow/llwindowwin32.cpp",
     '#include "kombatantfxwin32.h"\n', '')
edit("indra/llwindow/llwindowwin32.cpp",
     '    KombatantFX::initialize();\n', '')
edit("indra/llwindow/llwindowwin32.cpp",
     '    KombatantFX::window = mWindowHandle;\n', '', count=2)
cpp=file("indra/llwindow/llwindowwin32.cpp")
text=cpp.read_text(encoding="utf-8")
a=text.index('bool LLWindowWin32::configurePostProcessing(')
b=text.index('void LLWindowWin32::swapBuffers()',a)
text=text[:a]+text[b:]
cpp.write_text(text,encoding="utf-8")
# Dedicated legacy runtime is not included in the compiled source.
file("indra/llwindow/kombatantfxwin32.h").unlink()

settings="indra/newview/app_settings/settings_blackdragon.xml"
edit(settings,
     '  <key>KombatantShaderFX</key>\n'
     '  <map><key>Comment</key><string>Enable bundled ReShade effects for this session.</string><key>Persist</key><integer>0</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>0</integer></map>\n', '')
keydefs = '''  <key>KAMIPerformanceMode</key>
  <map><key>Comment</key><string>Native reduced-cost 3D render mode. Toggle off to restore saved graphics.</string><key>Persist</key><integer>1</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>0</integer></map>
  <key>KAMIPerformanceBackup</key>
  <map><key>Comment</key><string>Serialized pre-performance settings, restored when leaving Performance Mode.</string><key>Persist</key><integer>1</integer><key>Type</key><string>String</string><key>Value</key><string></string></map>
'''
edit(settings, "<map>\n", "<map>\n"+keydefs,1)

# Keep native color grading/bloom/DOF. The old effects floater no longer
# references external FX runtime, preset files, or overlay.
fx=file("indra/newview/kombatanteffects.h")
fx.write_text('''// KAMI Native Graphics Lab; no GShade/ReShade runtime or preset loader.
#pragma once
#include "llfloater.h"
#include "llbutton.h"
#include "llviewercontrol.h"

class KombatantEffects final : public LLFloater
{
public:
    explicit KombatantEffects(const LLSD& key) : LLFloater(key) {}
    bool postBuild() override
    {
        getChild<LLButton>("restore_fx")->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            for (auto it = mSaved.beginMap(); it != mSaved.endMap(); ++it)
                if (auto control = gSavedSettings.getControl(it->first))
                    control->setValue(it->second);
        });
        return true;
    }
    void onOpen(const LLSD&) override
    {
        mSaved = LLSD::emptyMap();
        for (const char* name : {
            "KombatantNativeFX", "KombatantSaturation", "KombatantContrast",
            "KombatantVignette", "RenderCASSharpness", "RenderExposure",
            "RenderGlow", "RenderGlowStrength", "RenderDeferredSSAO",
            "RenderDepthOfField", "RenderVolumetricLighting",
            "RenderScreenSpaceReflections", "RenderShadowDetail"
        })
            if (auto control = gSavedSettings.getControl(name))
                mSaved[name] = control->getValue();
    }
private:
    LLSD mSaved;
};
''',encoding="utf-8")
xui=file("indra/newview/skins/default/xui/en/floater_kombatant_effects.xml")
s=xui.read_text(encoding="utf-8-sig")
assert s.count(' <text name="shader_heading"')==1
s=s[:s.index(' <text name="shader_heading"')] + ''' <text name="native_advanced" left="24" top="390" width="512" height="24" font="SansSerifLarge">Native advanced lighting</text>
 <check_box name="native_volumetric" label="Volumetric lighting" control_name="RenderVolumetricLighting" left="24" top="424" width="235" height="22" />
 <check_box name="native_ssr" label="Screen-space reflections" control_name="RenderScreenSpaceReflections" left="275" top="424" width="250" height="22" />
 <text name="native_note" left="24" top="456" width="512" height="70" wrap="true">These effects run inside the viewer's native renderer. No external ReShade or GShade runtime. Performance Mode is in Preferences / Display.</text>
</floater>
'''
s=s.replace('title="KOMBATANT / EFFECTS"','title="KAMI / NATIVE GRAPHICS"').replace('height="730"','height="550"')
xui.write_text(s,encoding="utf-8")
menu="indra/newview/skins/default/xui/en/menu_viewer.xml"
edit(menu,'label="Kombatant"','label="KAMI"')
edit(menu,'label="Effects / GShade presets"','label="Native Graphics Lab"')
edit(menu,'label="Kombatant Studio"','label="KAMI Photo Studio"')

# Add a prominently placed one-click native Performance Mode selector.
# Push the existing graphics controls down so they cannot overlap.
ui=file("indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml")
s=ui.read_text(encoding="utf-8-sig")
start=s.index('<layout_panel name="kombatant_display_quick"')
end=s.index('</layout_panel>',start)+len('</layout_panel>')
panel=s[start:end]
assert panel.count('top="8"')>=1, "Graphics panel anchor moved"
assert 'height="1620" auto_resize="false"' in panel
panel=panel.replace('height="1620" auto_resize="false"','height="1684" auto_resize="false"',1)
panel=re.sub(r'\btop="(\d+)"',lambda m:'top="'+str(int(m.group(1))+62 if int(m.group(1))>=38 else int(m.group(1)))+'"',panel)
panel=panel.replace('>Kombatant graphics</text>', '>KAMI graphics</text>',1)
header_end=panel.index('</text>',panel.index('name="kd_heading"'))+len('</text>')
panel=panel[:header_end]+'''
       <check_box name="kami_performance_toggle" label="Performance Mode (prioritize FPS)" control_name="KAMIPerformanceMode" left="12" top="38" width="465" height="22" tool_tip="Render the world at 75% scale, lower distant avatar/object detail and disable costly post-processing. Turn off to restore your prior settings." />
       <text name="kami_performance_note" left="12" top="66" width="465" height="22">One-click mode; turn off to restore previous graphics.</text>'''+panel[header_end:]
s=s[:start]+panel+s[end:]
# Enlarge scroll panel to match native controls.
assert 'height="1624"' in s
s=s.replace('height="1624"','height="1688"',1)
ui.write_text(s,encoding="utf-8")

# Integrate the persistent reversible mode as a native viewer control listener.
src=file("kami/performance/kami_performance_mode.h")
dst=file("indra/newview/kami_performance_mode.h")
shutil.copy2(src,dst)
viewer="indra/newview/llviewercontrol.cpp"
edit(viewer,
     '\nvoid settings_setup_listeners()\n{',
     '\n#include "kami_performance_mode.h"\n\nvoid settings_setup_listeners()\n{\n    KAMIPerformance::registerListener();',1)

# Make fail-fast builds: bundled plugin runtime, presets, and SDK are gone.
assert not (ROOT/"indra/llwindow/kombatantfxwin32.h").exists()
for path in ["indra/llwindow/llwindowwin32.cpp","indra/newview/kombatanteffects.h",
             "indra/newview/skins/default/xui/en/floater_kombatant_effects.xml"]:
    source=file(path).read_text(encoding="utf-8")
    assert "KombatantFX::" not in source and "configurePostProcessing" not in source and "shader_overlay" not in source, path
print("KAMI: native graphics only, reversible Performance Mode wired.")
