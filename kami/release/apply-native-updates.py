#!/usr/bin/env python3
"""Integrate safe, nonblocking, KAMI-only release discovery into the Windows login.

This is NOT an automatic updater. Only official GitHub release metadata is read
with TLS verification; no response is executed, installed, or trusted as a URL.
The KAMI release page is opened only when the resident chooses to visit it.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
XUI = VIEW / "skins/default/xui/en"

header = VIEW / "kami_update_checker.h"
assert not header.exists(), "KAMI release checker already integrated"
shutil.copyfile(ROOT / "kami/release/kami_update_checker.h", header)

login = VIEW / "llpanellogin.cpp"
data = login.read_text(encoding="utf-8-sig")

def once(old, new):
    global data
    count = data.count(old)
    if count != 1:
        raise RuntimeError(f"Login update anchor changed (found {count}): {old[:65]}")
    data = data.replace(old, new, 1)

once('#include "llpanellogin.h"\n',
     '#include "llpanellogin.h"\n#if LL_WINDOWS\n#include "kami_update_checker.h"\n#endif\n')
once('https://github.com/NiranV/Black-Dragon-Viewer/releases',
     'https://github.com/Hegodly/Black-Dragon-Viewer/releases')
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
                LLNotificationsUtil::add("UpdateAvailable", LLSD(), LLSD(),
                    callback_show_release_url);
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
ET.indent(tree, space=" ")
tree.write(login_ui, encoding="utf-8", xml_declaration=True)

notice = XUI / "notifications.xml"
tree = ET.parse(notice)
matches = [x for x in tree.getroot().iter("notification") if x.get("name") == "UpdateAvailable"]
assert len(matches) == 1
matches[0].text = (
    "A new KAMI Viewer release is available. Open the official KAMI releases "
    "page to review and download it? No update will be installed automatically.\n"
)
ET.indent(tree, space=" ")
tree.write(notice, encoding="utf-8", xml_declaration=True)
for path in (login_ui, notice):
    ET.parse(path)
print("PASS: KAMI-only asynchronous release discovery, opt-in official release link")
