#!/usr/bin/env python3
"""Preflight: core KAMI features must be compiled and shipped, no user scripts.

This does NOT claim the independent GShade/ReShade runtime or personal MP3s
are included. Those remain explicitly optional third-party/user media.
"""
from pathlib import Path
import argparse
import ast
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
VIEW=ROOT/"indra/newview"
THEME=ROOT/"kami/theme"
PACK=ROOT/"kami/packaging"
FLOW=ROOT/".github/workflows/kombatant-windows.yaml"

def verify_source():
    native=(VIEW/"llpanellogin.cpp").read_text(encoding="utf-8-sig")
    header=(VIEW/"llpanellogin.h").read_text(encoding="utf-8-sig")
    theme=(THEME/"fix-login-theme.py").read_text(encoding="utf-8-sig")
    stage=(PACK/"stage-portable-data.py").read_text(encoding="utf-8-sig")
    final=(PACK/"finalize-kami-standalone.py").read_text(encoding="utf-8-sig")
    workflow=FLOW.read_text(encoding="utf-8-sig")
    for script in (THEME/"fix-login-theme.py",PACK/"stage-portable-data.py",
                   PACK/"finalize-kami-standalone.py"):
        ast.parse(script.read_text(encoding="utf-8-sig"),filename=str(script))
    for token in ('onKamiLoginMusicBrowse()', 'getOpenFile(LLFilePicker::FFLOAD_ALL)',
                  'std::filesystem::copy_file(source, destination',
                  'LOCALAPPDATA', 'KAMI-login.mp3'):
        assert token in native, ("Native KAMI login music source missing",token)
    assert "void onKamiLoginMusicBrowse();" in header
    assert "Install-KAMI-Login-Music" not in native
    assert 'name": "kami_login_music_browse"' in theme
    assert "Install-KAMI-Login-Music.cmd" not in theme
    for token in ("kami/theme/apply-nextgen-workspaces.py",
                  "kami/renderer/enable-true-1440p.py",
                  "kami/renderer/fix-visible-supersampling.py",
                  "kami/packaging/stage-portable-data.py",
                  "kami/packaging/finalize-kami-standalone.py"):
        assert token in workflow, ("Missing feature integration at build time",token)
    assert "stage-external-assets.ps1" not in workflow
    assert "Install-GShade-Assets.ps1" not in workflow
    assert "shutil.copyfile(R/'kami/theme'/installer" not in stage
    assert "Start-KAMI.cmd').write_text" not in stage
    assert "No CMD, BAT or PowerShell feature installers are required" in final
    assert "User-facing installer/launcher scripts" in final
    print("PASS zero-install source: all native KAMI features compiled and packaged; personal audio picked in-viewer")

def verify_integrated():
    p=VIEW/"skins/default/xui/en/panel_login.xml"
    root=ET.parse(p).getroot()
    n={e.get("name"):e for e in root.iter() if e.get("name")}
    for name in ("kami_fire_garden","kami_login_title","kami_login_subtitle",
                 "kami_login_music_controls","kami_login_music_enabled",
                 "kami_login_music_volume","kami_login_music_browse"):
        assert name in n, ("Missing native KAMI widget",name)
    assert n["kami_login_music_browse"].tag=="button"
    assert "Install" not in (n["kami_login_music_status"].text or "")
    print("PASS integrated KAMI Fire Garden login & music chooser live in XUI; no external feature setup")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--integrated",action="store_true")
    opts=p.parse_args()
    verify_source()
    if opts.integrated:verify_integrated()

if __name__=="__main__":
    main()
