#!/usr/bin/env python3
"""Stage runtime data omitted by the unpackaged CMake dependency-copy target."""
from pathlib import Path
import argparse
import shutil
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
a=argparse.ArgumentParser();a.add_argument('--output',default='build-vc170-64/newview/Release');a.add_argument('--fonts');args=a.parse_args()
out=R/args.output;out.mkdir(parents=True,exist_ok=True)
source=R/'indra/newview'
for name in ('app_settings','skins','character'):
 shutil.copytree(source/name,out/name,dirs_exist_ok=True)
fonts=Path(args.fonts) if args.fonts else source/'fonts'
assert fonts.is_dir() and list(fonts.rglob('*.ttf')),f'Missing runtime fonts: {fonts}'
shutil.copytree(fonts,out/'fonts',dirs_exist_ok=True)
for name in ('featuretable.txt','cube.dae'):
 shutil.copyfile(source/name,out/name)
shutil.copyfile(source/'licenses-win32.txt',out/'licenses.txt')
# Keep source attribution available in the portable copy.
shutil.copyfile(R/'doc/contributions.txt',out/'app_settings/contributors-source.txt')
required=['app_settings/settings.xml','app_settings/settings_blackdragon.xml','app_settings/shaders/class3/deferred/softenLightF.glsl','character/avatar_skeleton.xml','skins/default/xui/en/panel_login.xml','skins/default/textures/windows/kami_fire_garden.jpg','skins/default/textures/windows/kami_logo.jpg','featuretable.txt','cube.dae']
for name in required:assert (out/name).is_file(),f'Incomplete portable data: {name}'
for name in ('app_settings/settings.xml','app_settings/settings_blackdragon.xml','skins/default/xui/en/panel_login.xml'):ET.parse(out/name)
print(f'Runtime assets staged and validated: {out}')

# Repair overlays can be extracted directly over an existing portable folder.
(out/'Start-KAMI.cmd').write_text('@echo off\r\ncd /d "%~dp0"\r\nif not exist "KAMIViewer.exe" (echo Extract this repair into the folder containing KAMIViewer.exe. & pause & exit /b 1)\r\nif exist "blackdragon-bin.exe" del "blackdragon-bin.exe"\r\nstart "" "KAMIViewer.exe"\r\n',encoding='ascii')
