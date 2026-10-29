#!/usr/bin/env python3
"""Apply KAMI Viewer visual branding after the base viewer patches, before CMake.

Preserves project source attribution and legacy internal symbol names. The approved
logo is stored at kami/branding/kami_logo.jpg. No runtime network access.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "kami/branding/kami_logo.jpg"
assert SOURCE.is_file(), f"Missing approved KAMI logo: {SOURCE}"

def edit(path, old, new):
    target = ROOT / path
    s = target.read_text(encoding="utf-8-sig")
    if old not in s:
        if new in s:
            print(f"Already branded: {path}")
            return
        raise RuntimeError(f"Cannot find branding insertion point in {path}: {old[:90]}")
    target.write_text(s.replace(old, new, 1), encoding="utf-8")
    print("Branded:", path)

# Viewer/channel/installer identities. Internal binary is intentionally unchanged
# until the installer manifest packages it as KAMIViewer.exe.
edit("indra/cmake/Variables.cmake", 'set(VIEWER_CHANNEL "Kombatant Viewer"', 'set(VIEWER_CHANNEL "KAMI Viewer"')
edit("indra/lib/python/indra/util/llmanifest.py", "CHANNEL_VENDOR_BASE = 'Kombatant'", "CHANNEL_VENDOR_BASE = 'KAMI'")
edit("kombatant/build-windows.sh", "-DVIEWER_CHANNEL:STRING='Kombatant Viewer'", "-DVIEWER_CHANNEL:STRING='KAMI Viewer'")
edit("indra/newview/viewer_manifest.py", 'return "BlackDragonViewer"', 'return "KAMIViewer"')
edit("indra/newview/res/viewerRes.rc", "Loading Black Dragon...", "Loading KAMI Viewer...")
edit("indra/newview/res/viewerRes.rc", 'VALUE "FileDescription", "Black Dragon"', 'VALUE "FileDescription", "KAMI Viewer"')
edit("indra/newview/res/viewerRes.rc", 'VALUE "InternalName", "Black Dragon"', 'VALUE "InternalName", "KAMI Viewer"')
edit("indra/newview/res/viewerRes.rc", 'VALUE "OriginalFilename", "BlackDragon.exe"', 'VALUE "OriginalFilename", "KAMIViewer.exe"')
edit("indra/newview/res/viewerRes.rc", 'VALUE "ProductName", "Second Life"', 'VALUE "ProductName", "KAMI Viewer"')
edit("indra/newview/skins/default/xui/en/floater_about.xml", 'title="About [APP_NAME]"', 'title="About KAMI Viewer"')

# Standalone source-based icons. CMake copies icons/<channel>/blackdragon.ico
# over the rc resource; populate ALL channels before CMake configuration.
src = Image.open(SOURCE).convert("RGB")
width, height = src.size
square = src.crop((round(width*.265), round(height*.16), round(width*.735), round(height*.63)))
# Build all ICO sizes even when the repository JPEG is space-optimized.
square = square.resize((512, 512), Image.Resampling.LANCZOS)
for variant in ("release", "beta", "project", "test"):
    directory = ROOT / f"indra/newview/icons/{variant}"
    directory.mkdir(parents=True, exist_ok=True)
    square.save(directory / "blackdragon.ico", format="ICO",
                sizes=[(16,16), (24,24), (32,32), (48,48), (64,64), (128,128), (256,256)])
    square.resize((256,256), Image.Resampling.LANCZOS).save(
        directory / "blackdragon_256.BMP", format="BMP")
    for size in (16,32,48,128,256,512):
        square.resize((size,size),Image.Resampling.LANCZOS).save(
            directory / f"blackdragon_{size}.png")
square.save(ROOT / "indra/newview/res/bd_icon.ico", format="ICO",
            sizes=[(16,16), (24,24), (32,32), (48,48), (64,64), (128,128), (256,256)])
square.resize((256,256), Image.Resampling.LANCZOS).save(
    ROOT / "indra/newview/res/bd_icon.BMP", format="BMP")
square.resize((256,256), Image.Resampling.LANCZOS).save(
    ROOT / "indra/newview/res-sdl/bd_icon.BMP", format="BMP")
for path in ("indra/newview/installers/windows/install_icon.ico",
             "indra/newview/installers/windows/uninstall_icon.ico"):
    square.save(ROOT / path, format="ICO", sizes=[(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)])

textures = ROOT / "indra/newview/skins/default/textures"
(src).save(textures / "windows/kami_logo.jpg", quality=92, optimize=True)
src.resize((640,640), Image.Resampling.LANCZOS).save(
    textures / "windows/startup_logo.png", optimize=True)
edit("indra/newview/skins/default/textures/textures.xml",
     '<texture name="startup_logo"  file_name="windows/startup_logo.png" preload="true" />',
     '<texture name="KAMI_Logo" file_name="windows/kami_logo.jpg" preload="true" />\n'
     '  <texture name="startup_logo"  file_name="windows/startup_logo.png" preload="true" />')

# Replace the old externally-hosted login video with local lightweight artwork.
# This removes an unnecessary remote video from startup.
panel = ROOT / "indra/newview/skins/default/xui/en/panel_login.xml"
s = panel.read_text(encoding="utf-8-sig")
start = s.find(" <web_browser\n")
if start == -1:
    raise RuntimeError("Original Black Dragon login video widget not found")
end = s.find(" />", start)
if end == -1:
    raise RuntimeError("Original login video widget not terminated")
end += len(" />")
s = s[:start] + (' <icon\n follows="top|left"\n height="440"\n width="440"\n'
                 ' left="278"\n top="24"\n name="kami_login_branding"\n'
                 ' image_name="KAMI_Logo"\n mouse_opaque="false" />') + s[end:]
panel.write_text(s, encoding="utf-8")

# Add branded image on About's Info tab without affecting attribution texts.
about = ROOT / "indra/newview/skins/default/xui/en/floater_about.xml"
s = about.read_text(encoding="utf-8-sig")
anchor = '  name="support_panel">\n   <text_editor'
if anchor not in s:
    raise RuntimeError("About Info panel anchor not found")
s = s.replace(anchor, '  name="support_panel">\n'
              '   <icon name="kami_about_branding" image_name="KAMI_Logo" '
              'left="191" top="0" width="68" height="68" '
              'follows="left|top" mouse_opaque="false" />\n   <text_editor', 1)
# Restrict spacing edits to the first Info text editor.
start = s.index('   <text_editor', s.index('name="support_panel"'))
end = s.index(' />', start)
piece = s[start:end].replace('height="357"', 'height="289"').replace('top="1"', 'top="69"')
s = s[:start] + piece + s[end:]
about.write_text(s, encoding="utf-8")
print("KAMI Viewer branding applied. Original open-source credits retained.")
