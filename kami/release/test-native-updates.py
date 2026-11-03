#!/usr/bin/env python3
"""Source and build-integration regression gates for safe KAMI release discovery."""
from pathlib import Path
import argparse
import ast
import xml.etree.ElementTree as ET
r = Path(__file__).resolve().parents[2]
header = (r/"kami/release/kami_update_checker.h").read_text()
script = (r/"kami/release/apply-native-updates.py").read_text()
ast.parse(script)
assert "std::async(std::launch::async" in header
assert "CURLOPT_SSL_VERIFYPEER, 1L" in header
assert "CURLOPT_SSL_VERIFYHOST, 2L" in header
assert "CURLOPT_FOLLOWLOCATION, 0L" in header
assert "CURLOPT_TIMEOUT, 8L" in header
assert "CURLOPT_CONNECTTIMEOUT, 4L" in header
assert "https://api.github.com/repos/Hegodly/Black-Dragon-Viewer/releases/latest" in header
assert "Status::Available" in header and "parseSemver" in header
assert "curl_easy_perform" in header and "131072" in header
for forbidden in ("ShellExecute", "CreateProcess", "system(", "WinExec(", "popen(", "CURLOPT_SSL_VERIFYPEER, 0"):
    assert forbidden not in header, forbidden
assert "gDragonLibrary.fetchRemoteVersion()" in script
assert "callback_show_release_url" in script
assert "Hegodly/Black-Dragon-Viewer/releases" in script
assert "INSTALL & RESTART" in script
assert "callback_install_kami_update" in script
assert "callback_test_kami_update" in script
assert "launchTestInstall" in script
assert "KAMI-Test-Update.ps1" in script
assert "kami-test-updater.ps1" in script
p=argparse.ArgumentParser()
p.add_argument("--integrated",action="store_true")
a=p.parse_args()
if a.integrated:
    view=r/"indra/newview"
    src=(view/"llpanellogin.cpp").read_text(encoding="utf-8-sig")
    assert "gDragonLibrary.fetchRemoteVersion()" not in src
    assert '#include "kami_update_checker.h"' in src
    assert "kami::updates::begin(" in src
    assert "kami::updates::takeReady(" in src
    assert "UpdateAvailable" in src
    login=ET.parse(view/"skins/default/xui/en/panel_login.xml").getroot()
    button=next(n for n in login.iter("button") if n.get("name")=="new_version")
    action=button.find("button.commit_callback")
    assert "Hegodly/Black-Dragon-Viewer/releases" in action.get("parameter")
    assert "NiranV" not in action.get("parameter")
    notice=ET.parse(view/"skins/default/xui/en/notifications.xml").getroot()
    template=next(n for n in notice.iter("notification") if n.get("name")=="UpdateAvailable")
    assert "new KAMI Viewer release" in (template.text or "")
    assert template.find("usetemplate").get("yestext") == "INSTALL & RESTART"
    assert "KAMI-Test-Update.zip" in (r/"kami/packaging/stage-portable-data.py").read_text()
    assert (view/"app_settings/kami-test-updater.ps1").is_file()
    assert (view/"app_settings/kami-updater.ps1").is_file()
    assert any(n.get("name") == "kami_test_update" for n in login.iter("button"))
    assert any(n.get("name") == "KAMITestUpdateConfirm" for n in notice.iter("notification"))
print("PASS: KAMI release discovery, explicit Install & Restart, offline test update safeguards")
