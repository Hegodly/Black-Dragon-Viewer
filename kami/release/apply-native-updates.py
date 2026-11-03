#!/usr/bin/env python3
"""Integrate safe, nonblocking, KAMI-only release discovery into the Windows login.

This integrates an opt-in restart installer for a full SHA256-verified release
ZIP. The installer waits for viewer exit and retains the prior build to restore
on failure. Neither update checks nor downloads block the UI thread.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"

header = VIEW / "kami_update_checker.h"
installer_header = VIEW / "kami_update_install.h"
installer_script = VIEW / "app_settings/kami-updater.ps1"
assert not header.exists() and not installer_header.exists() and not installer_script.exists(), "KAMI update system already integrated"
shutil.copyfile(ROOT / "kami/release/kami_update_checker.h", header)
shutil.copyfile(ROOT / "kami/release/kami_update_install.h", installer_header)
shutil.copyfile(ROOT / "kami/release/KAMIUpdater.ps1", installer_script)
test_script = VIEW / "app_settings/kami-test-updater.ps1"
assert not test_script.exists(), "Test updater already integrated"
shutil.copyfile(ROOT / "kami/release/KAMI-Test-Update.ps1", test_script)

login = VIEW / "llpanellogin.cpp"
data = login.read_text(encoding="utf-8-sig")

def once(old, new):
    global data
    count = data.count(old)
    if count != 1:
        raise RuntimeError(f"Login update anchor changed (found {count}): {old[:65]}")
    data = data.replace(old, new, 1)

once('#include "llpanellogin.h"\n',
     '#include "llpanellogin.h"\n#if LL_WINDOWS\n#include "kami_update_checker.h"\n#include "kami_update_install.h"\n#endif\n')
once('https://github.com/NiranV/Black-Dragon-Viewer/releases',
     'https://github.com/Hegodly/Black-Dragon-Viewer/releases')
once('    return false;\n}\n\nLLPointer<LLCredential> load_user_credentials', '''    return false;
}

#if LL_WINDOWS
bool callback_install_kami_update(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
        return false;
    const auto tag = kami::updates::selectedVersion();
    if (!kami::updates::launchInstall(tag))
    {
        LLNotificationsUtil::add("KAMIUpdateLauncherError");
        return false;
    }
    LLAppViewer::instance()->requestQuit();
    return false;
}

bool callback_test_kami_update(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
        return false;
    if (!kami::updates::launchTestInstall())
    {
        LLNotificationsUtil::add("KAMIUpdateLauncherError");
        return false;
    }
    LLAppViewer::instance()->requestQuit();
    return false;
}
#endif

LLPointer<LLCredential> load_user_credentials''')

begin = data.index('    //BD - Fetch latest version.')
end = data.index('    LLButton* new_version_text = getChild<LLButton>("new_version");', begin)
assert begin > 0 and end > begin
old = data[begin:end]
assert 'gDragonLibrary.fetchRemoteVersion()' in old
assert 'LLNotificationsUtil::add("UpdateAvailable"' in old
data = data[:begin] + '''    // KAMI release metadata is checked off the UI thread.
    // No downloads, installers or executable files are started here.
    std::string new_version = LLVersionInfo::instance().getShortVersion();
    std::string update_str = "Checking KAMI releases...";
#if LL_WINDOWS
    kami::updates::begin(LLVersionInfo::instance().getShortVersion());
#endif

''' + data[end:]

once('#if LL_WINDOWS\n    LLCheckBoxCtrl* soundtrack = getChild<LLCheckBoxCtrl>("kami_login_music_enabled");', '''#if LL_WINDOWS
    getChild<LLButton>("kami_test_update")->setClickedCallback(
        [](LLUICtrl*, const LLSD&) {
            LLNotificationsUtil::add("KAMITestUpdateConfirm", LLSD(), LLSD(),
                callback_test_kami_update);
        });
    LLCheckBoxCtrl* soundtrack = getChild<LLCheckBoxCtrl>("kami_login_music_enabled");''')

once('void LLPanelLogin::draw()\n{\n#if LL_WINDOWS\n',
'''void LLPanelLogin::draw()
{
#if LL_WINDOWS
    kami::updates::Result release;
    if (kami::updates::takeReady(release))
    {
        if (auto* status = findChild<LLButton>("new_version"))
        {
            status->setLabelArg("[CHANNEL]", release.version.empty() ? "KAMI" : release.version);
            switch (release.status)
            {
            case kami::updates::Status::Available:
                status->setLabelArg("[UPDATE_STRING]", "NEW KAMI RELEASE AVAILABLE");
                kami::updates::selectedVersion() = release.version;
                if (release.installable)
                {
                    LLNotificationsUtil::add("UpdateAvailable", LLSD(), LLSD(),
                        callback_install_kami_update);
                }
                else
                {
                    LLNotificationsUtil::add("KAMIUpdateManual", LLSD(), LLSD(),
                        callback_show_release_url);
                }
                break;
            case kami::updates::Status::Current:
                status->setLabelArg("[UPDATE_STRING]", "UP TO DATE");
                break;
            case kami::updates::Status::NoPublishedRelease:
                status->setLabelArg("[UPDATE_STRING]", "NO PUBLIC RELEASE YET");
                break;
            default:
                status->setLabelArg("[UPDATE_STRING]", "CHECK UNAVAILABLE (OFFLINE?)");
                break;
            }
        }
    }
''')
login.write_text(data, encoding="utf-8")

login_ui = XUI / "panel_login.xml"
tree = ET.parse(login_ui)
root = tree.getroot()
matches = [x for x in root.iter("button") if x.get("name") == "new_version"]
assert len(matches) == 1
button = matches[0]
button.set("label", "KAMI RELEASES: [CHANNEL]  |  [UPDATE_STRING]")
actions = [x for x in button if x.tag == "button.commit_callback"]
assert len(actions) == 1 and actions[0].get("function") == "PromptShowURL"
actions[0].set("parameter",
    "WebLaunchExternalTarget,https://github.com/Hegodly/Black-Dragon-Viewer/releases")
assert not any(n.get("name") == "kami_test_update" for n in root.iter())
ET.SubElement(root, "button", {
    "name": "kami_test_update",
    "label": "TEST UPDATE (LOCAL)",
    "tool_tip": "Verify the updater and restart KAMI without changing executable code",
    "layout": "topleft", "follows": "right|top",
    "left": "808", "top": "30", "width": "170", "height": "26",
    "font": "SansSerifSmall",
})
ET.indent(tree, space=" ")
tree.write(login_ui, encoding="utf-8", xml_declaration=True)

notice = XUI / "notifications.xml"
tree = ET.parse(notice)
matches = [x for x in tree.getroot().iter("notification") if x.get("name") == "UpdateAvailable"]
assert len(matches) == 1
matches[0].text = (
    "A new KAMI Viewer release is available. Install and restart now? "
    "KAMI will verify the full update package, close and restart, with a "
    "backup of the previous build available for recovery.\n"
)
# Preserve the native confirmation layout; it now starts a local installer.
templ = matches[0]
buttons = templ.find("usetemplate")
assert buttons is not None
buttons.set("yestext", "INSTALL & RESTART")
buttons.set("notext", "LATER")
manual = ET.SubElement(tree.getroot(), "notification", {
    "name": "KAMIUpdateManual", "icon": "notify.tga", "type": "alertmodal"
})
manual.text = "A newer KAMI release is available, but it has no verified automatic installation package. Open the official KAMI releases page?\\n"
ET.SubElement(manual, "tag").text = "confirm"
ET.SubElement(manual, "usetemplate", {
    "name": "okcancelbuttons", "yestext": "VIEW RELEASE", "notext": "LATER"
})
testConfirm = ET.SubElement(tree.getroot(), "notification", {
    "name": "KAMITestUpdateConfirm", "icon": "notify.tga", "type": "alertmodal"
})
testConfirm.text = ("Run the local KAMI test update? It will verify a bundled test ZIP, "
    "close the viewer, install a harmless marker file, and restart KAMI. "
    "It needs free disk space for a backup. No new program code is downloaded.\\n")
ET.SubElement(testConfirm, "tag").text = "confirm"
ET.SubElement(testConfirm, "usetemplate", {
    "name": "okcancelbuttons", "yestext": "TEST & RESTART", "notext": "CANCEL"
})
fail = ET.SubElement(tree.getroot(), "notification", {
    "name": "KAMIUpdateLauncherError", "icon": "alertmodal.tga", "type": "alertmodal"
})
fail.text = "Unable to start the bundled KAMI updater. Your current installation was not changed.\\n"
ET.SubElement(fail, "tag").text = "confirm"
ET.SubElement(fail, "usetemplate", {"name": "okbutton", "yestext": "OK"})
ET.indent(tree, space=" ")
tree.write(notice, encoding="utf-8", xml_declaration=True)
for path in (login_ui, notice):
    ET.parse(path)
print("PASS: KAMI-only release discovery plus user-approved verified install & restart")
