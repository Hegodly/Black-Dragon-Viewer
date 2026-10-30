#!/usr/bin/env python3
"""Optional personal MXAO runtime adapter. Contains no proprietary shader code."""
from pathlib import Path
import shutil
import xml.etree.ElementTree as E
R=Path(__file__).resolve().parents[2]
def edit(p,a,b):
 f=R/p;s=f.read_text(encoding='utf-8-sig')
 if s.count(a)!=1:raise RuntimeError(f'{p}: anchor count {s.count(a)}: {a[:70]}')
 f.write_text(s.replace(a,b,1),encoding='utf-8')
shutil.copyfile(R/'kami/mxao/kami_mxao_win32.h',R/'indra/llwindow/kami_mxao_win32.h')
edit('indra/llwindow/llwindow.h','\tvirtual void show() = 0;', '''    virtual std::string configurePrivateMXAO(bool, float, float, int, bool) { return "Private MXAO is Windows-only."; }
\tvirtual void show() = 0;''')
edit('indra/llwindow/llwindowwin32.h','    void swapBuffers() override;', '''    std::string configurePrivateMXAO(bool enabled, float amount, float radius, int quality, bool overlay) override;
    void swapBuffers() override;''')
p='indra/llwindow/llwindowwin32.cpp'
edit(p,'#include "llwindowwin32.h"','#include "llwindowwin32.h"\n#include "kami_mxao_win32.h"')
edit(p,'    LoadLibrary(L"opengl32.dll");','    LoadLibrary(L"opengl32.dll");\n    KAMIMXAO::initialize();')
edit(p,'    if (!switchContext(mFullscreen, windowSize, enable_vsync, &windowPos))','    KAMIMXAO::window = mWindowHandle;\n    if (!switchContext(mFullscreen, windowSize, enable_vsync, &windowPos))')
edit(p,'void LLWindowWin32::swapBuffers()\n{','''std::string LLWindowWin32::configurePrivateMXAO(bool enabled, float amount, float radius, int quality, bool overlay)
{
    KAMIMXAO::window = mWindowHandle;
    return KAMIMXAO::configure(enabled, amount, radius, quality, overlay);
}

void LLWindowWin32::swapBuffers()
{
    KAMIMXAO::window = mWindowHandle;''')
settings='indra/newview/app_settings/settings_blackdragon.xml'
controls=[('KAMIMXAO','Boolean','integer','0',0),('KAMIMXAOAmount','F32','real','1',1),('KAMIMXAORadius','F32','real','2.5',1),('KAMIMXAOQuality','S32','integer','1',1),('KAMIMXAOOverlay','Boolean','integer','0',0),('KAMIMXAOStatus','String','string','Private MXAO not installed.',0)]
xml=''
for n,t,tag,v,persist in controls:
 xml+=f'<key>{n}</key><map><key>Comment</key><string>Optional personal MXAO runtime control; no shader code is bundled.</string><key>Persist</key><integer>{persist}</integer><key>Type</key><string>{t}</string><key>Value</key><{tag}>{v}</{tag}></map>\n'
edit(settings,'<map>\n  <key>KAMILiveGraphicsPreview</key>','<map>\n'+xml+'  <key>KAMILiveGraphicsPreview</key>')
p='indra/newview/llviewerdisplay.cpp'
edit(p,'    if (gDisplaySwapBuffers)\n    {','''    if (gDisplaySwapBuffers)
    {
#if LL_WINDOWS
        const bool mxao = gSavedSettings.getBOOL("KAMIMXAO");
        const bool blocked = gSavedSettings.getBOOL("RenderDeferredSSAO") ||
            gSavedSettings.getBOOL("KAMIFrameGeneration") || gSavedSettings.getBOOL("KAMIPerformanceMode");
        const bool overlay = gSavedSettings.getBOOL("KAMIMXAOOverlay");
        if (overlay) gSavedSettings.setBOOL("KAMIMXAOOverlay", false);
        const std::string mxao_status = gViewerWindow->getWindow()->configurePrivateMXAO(
            mxao && !blocked && !gSnapshot,
            llclamp(gSavedSettings.getF32("KAMIMXAOAmount"), 0.f, 2.f),
            llclamp(gSavedSettings.getF32("KAMIMXAORadius"), 0.5f, 10.f),
            llclamp(gSavedSettings.getS32("KAMIMXAOQuality"), 0, 4), overlay);
        const bool missing = mxao_status.find("not installed") != std::string::npos ||
            mxao_status.find("failed") != std::string::npos ||
            mxao_status.find("skipped") != std::string::npos;
        const std::string next_status = (mxao && blocked && !missing)
            ? "MXAO paused: disable native AO, frame generation and Performance Mode first."
            : mxao_status;
        if (gSavedSettings.getString("KAMIMXAOStatus") != next_status)
            gSavedSettings.setString("KAMIMXAOStatus", next_status);
#endif''')
p='indra/newview/llfloaterpreference.cpp'
edit(p,'void LLFloaterPreference::draw()\n{','''void LLFloaterPreference::draw()
{
    if (auto* text = findChild<LLTextBox>("kami_mxao_status"))
        text->setText(LLStringExplicit(gSavedSettings.getString("KAMIMXAOStatus")));''')
p='indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml'
f=R/p;s=f.read_text(encoding='utf-8-sig');a=s.index('<layout_panel name="kombatant_display_quick"');b=s.index('</layout_panel>',a)
area=s[a:b];assert 'height="4250"' in area
area=area.replace('height="4250"','height="4650"',1)+''' <text name="kami_mxao_heading" left="12" top="4260" width="465" height="24" font="SansSerifLarge">Private MXAO (optional runtime)</text>
 <check_box name="kami_mxao_enable" control_name="KAMIMXAO" label="Enable actual Marty’s Mods MXAO" left="12" top="4292" width="465" height="22" />
 <slider name="kami_mxao_amount" control_name="KAMIMXAOAmount" label="MXAO strength" left="12" top="4324" width="465" height="20" label_width="210" min_val="0" max_val="2" increment="0.05" decimal_digits="2" />
 <slider name="kami_mxao_radius" control_name="KAMIMXAORadius" label="MXAO radius" left="12" top="4356" width="465" height="20" label_width="210" min_val="0.5" max_val="10" increment="0.1" decimal_digits="1" />
 <combo_box name="kami_mxao_quality" control_name="KAMIMXAOQuality" left="12" top="4388" width="465" height="24">
 <combo_box.item label="MXAO quality: Low" value="0" /><combo_box.item label="MXAO quality: Medium" value="1" /><combo_box.item label="MXAO quality: High" value="2" /><combo_box.item label="MXAO quality: Very high" value="3" /><combo_box.item label="MXAO quality: Ultra" value="4" />
 </combo_box>
 <check_box name="kami_mxao_overlay" control_name="KAMIMXAOOverlay" label="Open MXAO / depth diagnostics (Home)" left="12" top="4420" width="465" height="22" />
 <text name="kami_mxao_status" left="12" top="4452" width="465" height="42" wrap="true">MXAO not installed. Click Install MXAO.</text>
 <button name="kami_mxao_install" label="Install MXAO from author (optional)" left="12" top="4500" width="465" height="28" />
 <text name="kami_mxao_help" left="12" top="4538" width="465" height="92" wrap="true">Optional personal install downloads the author’s shader directly, without Git. Restart KAMI after installation. Native AO, frame generation and Performance Mode pause MXAO. Depth selection must be verified.</text>
'''
s=s[:a]+area+s[b:];assert s.count('height="4254"')==1
s=s.replace('height="4254"','height="4654"',1);f.write_text(s,encoding='utf-8')

# An explicit installation button; never run downloads while drawing frames.
p='indra/newview/llfloaterpreference.cpp'
edit(p,'#include "llfloaterpreference.h"', '#include "llfloaterpreference.h"\n#include <filesystem>')
edit(p,'bool LLFloaterPreference::postBuild()\n{', '''bool LLFloaterPreference::postBuild()
{
#if LL_WINDOWS
    if (auto* install = findChild<LLButton>("kami_mxao_install"))
    {
        install->setClickedCallback([this](LLUICtrl*, const LLSD&) {
            wchar_t executable[32768] = {};
            DWORD len = GetModuleFileNameW(nullptr, executable, 32768);
            if (!len || len >= 32768)
            {
                gSavedSettings.setString("KAMIMXAOStatus", "Cannot locate KAMIViewer.exe.");
                return;
            }
            const auto base = std::filesystem::path(executable).parent_path();
            const auto script = base / L"Install-Private-MXAO.ps1";
            if (!std::filesystem::is_regular_file(script))
            {
                gSavedSettings.setString("KAMIMXAOStatus", "MXAO installer missing. Re-extract complete KAMI ZIP.");
                return;
            }
            const std::wstring quote(1, wchar_t(34));
            const std::wstring args = std::wstring(L"-NoProfile -ExecutionPolicy Bypass -File ")
                + quote + script.wstring() + quote + L" -ViewerDirectory " + quote + base.wstring() + quote;
            SHELLEXECUTEINFOW launch = { sizeof(launch) };
            launch.lpVerb = L"open";
            launch.lpFile = L"powershell.exe";
            launch.lpParameters = args.c_str();
            launch.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&launch))
                gSavedSettings.setString("KAMIMXAOStatus",
                    "Installer launched. Finish download, close viewer, then restart KAMI.");
            else
                gSavedSettings.setString("KAMIMXAOStatus", "Cannot open PowerShell installer.");
        });
    }
#endif
''')

E.parse(f);E.parse(R/settings)
print('Optional private MXAO runtime adapter applied; shader is separately installed by its user.')
