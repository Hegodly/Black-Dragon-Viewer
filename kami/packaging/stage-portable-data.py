#!/usr/bin/env python3
"""Stage runtime data omitted by the unpackaged CMake dependency-copy target."""
from pathlib import Path
import argparse
import hashlib
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
# The approved theme must ship in-world as well as on the login screen.
fire_ui=[
 'skins/default/xui/en/floater_preferences.xml',
 'skins/default/KAMI-BASE-GRAPHICS-KIT.txt',
 'skins/default/KAMI-SIGNATURE-FEATURES.txt',
 'skins/default/colors.xml',
 'skins/default/textures/windows/kami_fg_window.png',
 'skins/default/textures/windows/kami_fg_panel.png',
 'skins/default/textures/windows/kami_fg_button.png',
 'skins/default/textures/windows/kami_fg_preferences.png',
]
for name in fire_ui:
 assert (out/name).is_file(), f'Missing approved in-world Fire Garden skin asset: {name}'
# Bundle locally authored shader effects, licensed presets and the user's
# optional import/activation tools, without shipping private MXAO source.
built_in_fx=[
 'KAMI-FX/Shaders/KAMI-FX.fx',
 'KAMI-FX/FireGarden.ini',
 'KAMI-FX/Cinematic.ini',
 'KAMI-FX/Clean4K.ini',
 'KAMI-FX/FX-LICENSE.txt',
 'KAMI-GSHADE-FX-README.md',
 'Import-GShade-Preset.ps1',
 'Enable-KAMI-FX.ps1',
 'Install-Private-MXAO.ps1',
 'KAMIMXAO64.dll',
]
for name in built_in_fx:
 assert (out/name).is_file(), f'Missing KAMI built-in FX or MXAO personal-install component: {name}'
assert not list(out.rglob('MartysMods_MXAO.fx')), 'Copyright-restricted MXAO source cannot be publicly redistributed'
assert not list(out.rglob('KAMI_MXAO_PRIVATE.enabled')) and not list(out.rglob('KAMI_EFFECTS_BUNDLED.enabled')), 'Optional FX must remain disabled on first launch'
pref=ET.parse(out/'skins/default/xui/en/floater_preferences.xml').getroot()
assert any(x.get('name')=='vicon' and x.get('image_name')=='KAMI_FireGardenPrefs' for x in pref.iter()), 'Preferences still use Black Dragon wallpaper'
for obsolete in ('floater_kombatant_effects.xml','floater_kombatant_studio.xml'):
 assert not (out/'skins/default/xui/en'/obsolete).exists(), f'Deleted Lab/Studio floater was packaged: {obsolete}'

for name in ('app_settings/settings.xml','app_settings/settings_blackdragon.xml','skins/default/xui/en/panel_login.xml'):ET.parse(out/name)
# Fail the release before packaging if a login asset silently regresses to the
# original Black Dragon HTML/video layout or has a missing image texture.
login=ET.parse(out/'skins/default/xui/en/panel_login.xml').getroot()
textures=ET.parse(out/'skins/default/textures/textures.xml').getroot()
named={node.get('name'):node for node in login.iter() if node.get('name')}
wallpaper=named.get('kami_fire_garden')
emblem=named.get('kami_header_emblem')
assert wallpaper is not None and wallpaper.get('image_name')=='KAMI_FireGarden', 'KAMI wallpaper not selected in login'
assert emblem is not None and emblem.get('image_name')=='KAMI_Logo', 'KAMI login emblem missing'
assert named.get('kami_login_title') is not None and 'K A M I' in (named['kami_login_title'].text or ''), 'KAMI title missing'
assert named.get('kami_login_subtitle') is not None and 'B E Y O N D' in (named['kami_login_subtitle'].text or ''), 'KAMI tagline missing'
assert not any(node.tag=='web_browser' for node in login.iter()), 'Legacy remote video/browser still present'
assert not any(node.get('name') in ('login_stack','login_bg') for node in login.iter()), 'Legacy Black Dragon login layer still present'
registry={node.get('name'):node.get('file_name') for node in textures.findall('texture')}
assert registry.get('KAMI_FireGarden') == 'windows/kami_fire_garden.jpg'
assert registry.get('KAMI_Logo') == 'windows/kami_logo.jpg'
wall=out/'skins/default/textures/windows/kami_fire_garden.jpg'
assert wall.read_bytes()==(R/'kami/theme/assets/fire_garden.jpg').read_bytes(), 'Packaged fire-garden artwork differs from approved source'
assert (out/'skins/default/textures/windows/kami_logo.jpg').stat().st_size>4096, 'KAMI emblem file missing or truncated'
assert (out/'skins/default/textures/windows/startup_logo.png').stat().st_size>4096, 'KAMI startup logo missing'
(out/'KAMI-THEME-MANIFEST.txt').write_text(
    'KAMI Beyond Reality - static Fire Garden login theme\\n'
    + 'panel_login.xml SHA256: '+hashlib.sha256((out/'skins/default/xui/en/panel_login.xml').read_bytes()).hexdigest()+'\\n'
    + 'kami_fire_garden.jpg SHA256: '+hashlib.sha256(wall.read_bytes()).hexdigest()+'\\n'
    + 'Status: branded login, no Black Dragon video/changelog overlay\\n',
    encoding='utf-8')
print('Approved KAMI Beyond Reality login artwork and layout verified in portable release.')
print(f'Runtime assets staged and validated: {out}')

# Repair overlays can be extracted directly over an existing portable folder.
(out/'Start-KAMI.cmd').write_text('@echo off\r\ncd /d "%~dp0"\r\nif not exist "KAMIViewer.exe" (echo Extract this repair into the folder containing KAMIViewer.exe. & pause & exit /b 1)\r\nif exist "blackdragon-bin.exe" del "blackdragon-bin.exe"\r\nstart "" "KAMIViewer.exe"\r\n',encoding='ascii')
