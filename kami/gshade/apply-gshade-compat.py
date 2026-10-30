#!/usr/bin/env python3
"""Wire personal GShade-compatible ReShade FX presets into KAMI Display.

Reuses the *existing*, optional and licensed ReShade runtime. This does not
embed, disguise or reimplement the proprietary GShade runtime or shader packs.
It does not stack OpenGL DLL hooks. Users must import their own compatible files.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def change(path,old,new):
    f=R/path
    s=f.read_text(encoding="utf-8-sig")
    assert s.count(old)==1,(path,old[:90],s.count(old))
    f.write_text(s.replace(old,new,1),encoding="utf-8")

header="indra/llwindow/kami_mxao_win32.h"
# The compiled runtime already has all ReShade effect processing, profile and
# overlay APIs. GShade-compatible presets are loaded using their ReShade.ini
# profile at process initialization, when the *user* installer created a marker.
change(header,
    'inline bool attempted=false;',
    'inline bool attempted=false;\ninline bool gshadeMode=false;\ninline bool lastGShadeState=false;')
change(header,
    '    if(!std::filesystem::is_regular_file(base/L"KAMI_MXAO_PRIVATE.enabled",ec))return;',
    '''    if(!std::filesystem::is_regular_file(base/L"KAMI_MXAO_PRIVATE.enabled",ec) &&
       !std::filesystem::is_regular_file(base/L"KAMI_GSHADE_USER.enabled",ec) &&
       !std::filesystem::is_regular_file(base/L"KAMI_EFFECTS_BUNDLED.enabled",ec)) {
        status="Import a GShade-compatible preset from Preferences > Display, then restart.";
        return;
    }''')
change(header,
    '    if(!enabled){runtime->set_effects_state(false);return status="MXAO off.";}',
    '''    // GShade mode controls the same ReShade runtime; do not let the
    // MXAO per-frame path repeatedly switch its effects off.
    if(gshadeMode)return status="GShade-compatible preset owns shader runtime.";
    if(!enabled){runtime->set_effects_state(false);return status="MXAO off.";}''')
p=R/header
s=p.read_text(encoding="utf-8")
anchor='\n}\n}\n'
assert s.endswith(anchor)
# set_effects_state only on transitions; effect reload occurs when the user
# changes profile via Home overlay or restarts with the imported files.
s=s[:-len(anchor)]+'''
}
inline std::string configureGShade(bool enabled,bool overlay) {
    gshadeMode=enabled;
    if(!runtime)return status;
    if(overlay)runtime->open_overlay(true,reshade::api::input_source::mouse);
    if(enabled!=lastGShadeState)
    {
        runtime->set_effects_state(enabled);
        lastGShadeState=enabled;
    }
    if(!enabled) return "GShade compatibility off.";
    return "Compatible preset enabled via ReShade FX. Home: check shader compile/depth; GShade-only extensions may fail.";
}
}
'''
p.write_text(s,encoding="utf-8")

change("indra/llwindow/llwindow.h",
    '    virtual std::string configurePrivateMXAO(bool, float, float, int, bool) { return "Private MXAO is Windows-only."; }',
    '''    virtual std::string configurePrivateMXAO(bool, float, float, int, bool) { return "Private MXAO is Windows-only."; }
    virtual std::string configureGShade(bool, bool) { return "GShade preset compatibility is Windows-only."; }''')
change("indra/llwindow/llwindowwin32.h",
    '    std::string configurePrivateMXAO(bool enabled, float amount, float radius, int quality, bool overlay) override;',
    '''    std::string configurePrivateMXAO(bool enabled, float amount, float radius, int quality, bool overlay) override;
    std::string configureGShade(bool enabled, bool overlay) override;''')
change("indra/llwindow/llwindowwin32.cpp",
    'void LLWindowWin32::swapBuffers()\n{',
    '''std::string LLWindowWin32::configureGShade(bool enabled, bool overlay)
{
    KAMIMXAO::window = mWindowHandle;
    return KAMIMXAO::configureGShade(enabled, overlay);
}

void LLWindowWin32::swapBuffers()
{''')
# Own graphics setting flags; all optional, off until user asks.
settings=R/"indra/newview/app_settings/settings_blackdragon.xml"
doc=ET.parse(settings)
root=doc.getroot().find("map")
assert root is not None
entries=[
    ("KAMIGShadeCompatibility","Boolean","integer","0",1,
     "Enable locally imported GShade-compatible ReShade FX preset. User must import own files."),
    ("KAMIGShadeOverlay","Boolean","integer","0",0,
     "One-shot opening of user shader diagnostics overlay."),
    ("KAMIGShadeStatus","String","string","Import your GShade-compatible preset to begin.",0,
     "Last actual runtime preset status, including blocked/missing runtime diagnostics."),
]
for name,ty,tag,value,persist,comment in entries:
    assert all(not(n.tag=="key" and n.text==name) for n in root)
    key=ET.Element("key");key.text=name
    node=ET.Element("map")
    for element,value2 in [
        ("key","Comment"),("string",comment),("key","Persist"),
        ("integer",str(persist)),("key","Type"),("string",ty),
        ("key","Value"),(tag,value),
    ]:
        it=ET.SubElement(node,element);it.text=value2
    root.insert(0,node);root.insert(0,key)
ET.indent(doc,space=" ")
doc.write(settings,encoding="utf-8",xml_declaration=True)

# Final swap/presentation loop: check once per render without allocating GPU
# resources. The shared runtime only changes enabled state on transitions.
display="indra/newview/llviewerdisplay.cpp"
old='''        const std::string mxao_status = gViewerWindow->getWindow()->configurePrivateMXAO(
            mxao && !blocked && !gSnapshot,
            llclamp(gSavedSettings.getF32("KAMIMXAOAmount"), 0.f, 2.f),
            llclamp(gSavedSettings.getF32("KAMIMXAORadius"), 0.5f, 10.f),
            llclamp(gSavedSettings.getS32("KAMIMXAOQuality"), 0, 4), overlay);'''
new='''        const bool gshadeRequested = gSavedSettings.getBOOL("KAMIGShadeCompatibility");
        const bool gshadeOverlay = gSavedSettings.getBOOL("KAMIGShadeOverlay");
        if (gshadeOverlay) gSavedSettings.setBOOL("KAMIGShadeOverlay", false);
        const bool gshadeBlocked = gSavedSettings.getBOOL("KAMIPerformanceMode") ||
            gSavedSettings.getBOOL("KAMIFrameGeneration") || gSnapshot;
        // GShade and MXAO share ONE optional ReShade runtime and cannot be
        // active as independent injectors/preset configurations.
        const std::string mxao_status = gViewerWindow->getWindow()->configurePrivateMXAO(
            mxao && !blocked && !gSnapshot && !gshadeRequested,
            llclamp(gSavedSettings.getF32("KAMIMXAOAmount"), 0.f, 2.f),
            llclamp(gSavedSettings.getF32("KAMIMXAORadius"), 0.5f, 10.f),
            llclamp(gSavedSettings.getS32("KAMIMXAOQuality"), 0, 4), overlay);
        const std::string gshade_status = gViewerWindow->getWindow()->configureGShade(
            gshadeRequested && !gshadeBlocked, gshadeOverlay);
        const std::string shown_gshade = (gshadeRequested && gshadeBlocked)
            ? "GShade-compatible FX paused by Performance Mode, optical flow or snapshot."
            : gshade_status;
        if (gSavedSettings.getString("KAMIGShadeStatus") != shown_gshade)
            gSavedSettings.setString("KAMIGShadeStatus", shown_gshade);'''
change(display,old,new)

# Display UI inside the existing scroll stack. Keep existing Fire Garden
# checkbox/slider/dropdown textures and 42 original graphics bindings.
ui=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(ui);root=tree.getroot()
panels=[n for n in root.iter() if n.tag=="layout_panel" and n.get("name")=="kombatant_display_quick"]
assert len(panels)==1
quick=panels[0]
oldheight=int(quick.get("height"))
assert oldheight>=4600,oldheight
quick.set("height",str(oldheight+289))
scroll_sizes=[n for n in root.iter() if n.get("height")==str(oldheight+4)]
assert len(scroll_sizes)>=1,("Expected parent scroll contents sizing",oldheight)
for node in scroll_sizes:node.set("height",str(oldheight+293))
def new(tag,name,top,**attrs):
    attributes={k:str(v) for k,v in attrs.items()}
    x=ET.Element(tag,name=name,left="12",top=str(top),**attributes)
    quick.append(x)
    return x
y=oldheight+6
new("text","kami_gshade_heading",y,width="465",height="24",font="SansSerifLarge").text="GShade Preset Compatibility"
new("check_box","kami_gshade_toggle",y+33,width="465",height="22",
    control_name="KAMIGShadeCompatibility",label="Enable built-in / compatible GShade FX",
    tool_tip="Use user-supplied ReShade FX-compatible .ini/.fx/.fxh and textures with the single optional KAMI runtime. Does not run proprietary GShade itself.")
new("check_box","kami_gshade_overlay",y+65,width="465",height="22",
    control_name="KAMIGShadeOverlay",label="Open shader diagnostics / effects overlay (Home)")
new("button","kami_gshade_import",y+98,width="465",height="28",
    label="Import GShade preset and shader folders...")
new("button","kami_fx_enable",y+137,width="465",height="28",label="Activate included KAMI FX pack (20 effects)")
new("text","kami_gshade_status",y+172,width="465",height="45",wrap="true").text="20 original effects included; optional one-time activation."
new("text","kami_gshade_help",y+219,width="465",height="55",wrap="true").text=(
    "20 original KAMI effects are included; activation is optional and requires restart. "
    "To use your personal GShade .ini and .fx/.fxh collection, click Import instead. "
    "GShade-exclusive effects may not compile; no duplicate OpenGL hooks.")
ET.indent(tree,space=" ")
tree.write(ui,encoding="utf-8",xml_declaration=True)

# Actual import button, Windows file chooser and script in portable root.
prefs=R/"indra/newview/llfloaterpreference.cpp"
s=prefs.read_text(encoding="utf-8-sig")
a='''bool LLFloaterPreference::postBuild()
{'''
assert s.count(a)==1
b=a+'''
#if LL_WINDOWS
    if (auto* import_button = findChild<LLButton>("kami_gshade_import"))
    {
        import_button->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            wchar_t executable[32768] = {};
            DWORD len = GetModuleFileNameW(nullptr, executable, 32768);
            if (!len || len >= 32768)
            {
                gSavedSettings.setString("KAMIGShadeStatus", "Could not locate KAMIViewer.exe.");
                return;
            }
            const auto base = std::filesystem::path(executable).parent_path();
            const auto installer = base / L"Import-GShade-Preset.ps1";
            if (!std::filesystem::is_regular_file(installer))
            {
                gSavedSettings.setString("KAMIGShadeStatus",
                    "Preset importer missing; extract the complete portable KAMI ZIP.");
                return;
            }
            const std::wstring quote(1, wchar_t(34));
            const std::wstring args = std::wstring(L"-NoProfile -STA -ExecutionPolicy Bypass -File ")
                + quote + installer.wstring() + quote + L" -ViewerDirectory " + quote + base.wstring() + quote;
            SHELLEXECUTEINFOW launch = {sizeof(launch)};
            launch.lpVerb = L"open";
            launch.lpFile = L"powershell.exe";
            launch.lpParameters = args.c_str();
            launch.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&launch))
                gSavedSettings.setString("KAMIGShadeStatus",
                    "Import wizard launched. Close KAMI, select preset/shaders, restart viewer.");
            else
                gSavedSettings.setString("KAMIGShadeStatus", "Could not launch preset importer.");
        });
    }
    if (auto* activate = findChild<LLButton>("kami_fx_enable"))
    {
        activate->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            wchar_t executable[32768] = {};
            DWORD len = GetModuleFileNameW(nullptr, executable, 32768);
            if (!len || len >= 32768) return;
            const auto base = std::filesystem::path(executable).parent_path();
            const auto installer = base / L"Enable-KAMI-FX.ps1";
            if (!std::filesystem::is_regular_file(installer))
            {
                gSavedSettings.setString("KAMIGShadeStatus",
                    "Bundled FX activation script missing; extract the full portable package.");
                return;
            }
            const std::wstring quote(1, wchar_t(34));
            const std::wstring args = std::wstring(L"-NoProfile -STA -ExecutionPolicy Bypass -File ")
                + quote + installer.wstring() + quote + L" -ViewerDirectory " + quote + base.wstring() + quote;
            SHELLEXECUTEINFOW launch = {sizeof(launch)};
            launch.lpVerb = L"open";
            launch.lpFile = L"powershell.exe";
            launch.lpParameters = args.c_str();
            launch.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&launch))
                gSavedSettings.setString("KAMIGShadeStatus",
                    "FX activator opened. Close viewer when prompted, then restart and enable FX.");
            else
                gSavedSettings.setString("KAMIGShadeStatus", "Failed to launch FX activator.");
        });
    }
#endif
'''
s=s.replace(a,b,1)
# Keep actual enable/compile feedback visible while Preferences is open.
txt='''    if (auto* text = findChild<LLTextBox>("kami_fg_status"))
        text->setText(LLStringExplicit(gSavedSettings.getString("KAMIFrameGenerationStatus")));'''
assert s.count(txt)==1
s=s.replace(txt,txt+'''
    if (auto* text = findChild<LLTextBox>("kami_gshade_status"))
        text->setText(LLStringExplicit(gSavedSettings.getString("KAMIGShadeStatus")));''',1)
prefs.write_text(s,encoding="utf-8")
ET.parse(settings);ET.parse(ui)
assert "KAMIMXAO::configureGShade" in (R/"indra/llwindow/llwindowwin32.cpp").read_text()
print("KAMI: Fire Garden GShade compatibility enabled with actual optional ReShade FX preset engine, no extra GL hooks or proprietary bundled shaders.")
