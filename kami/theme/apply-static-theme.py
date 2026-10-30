#!/usr/bin/env python3
"""Install local static fire-garden artwork and the Beyond Reality palette."""
from pathlib import Path
import re
import shutil
import xml.etree.ElementTree as ET
ROOT = Path(__file__).resolve().parents[2]
textures = ROOT / 'indra/newview/skins/default/textures'
assets = ROOT / 'kami/theme/assets'
for source, destination in [('fire_garden.jpg', 'kami_fire_garden.jpg'), ('kami_beyond_reality.jpg', 'kami_logo.jpg')]:
    shutil.copyfile(assets / source, textures / 'windows' / destination)
# Keep all executable icons and the startup logo consistent with the approved emblem.
from PIL import Image
logo = Image.open(assets / 'kami_beyond_reality.jpg').convert('RGB')
logo.resize((640,640), Image.Resampling.LANCZOS).save(textures / 'windows/startup_logo.png')
p = textures / 'textures.xml'
s = p.read_text(encoding='utf-8-sig')
anchor = '<texture name="KAMI_Logo"'
assert anchor in s, 'Apply base branding first'
s = s.replace(anchor, '<texture name="KAMI_FireGarden" file_name="windows/kami_fire_garden.jpg" preload="false" />\n  '+anchor, 1)
p.write_text(s, encoding='utf-8')
p = ROOT / 'indra/newview/skins/default/xui/en/panel_login.xml'
s = p.read_text(encoding='utf-8-sig')
anchor = ' <icon\n follows="top|left"\n height="440"'
assert anchor in s, 'Branded login anchor changed'
s = s.replace(anchor, ' <icon name="kami_fire_garden" image_name="KAMI_FireGarden" follows="all" left="0" top="0" width="996" height="600" mouse_opaque="false" />\n'+anchor, 1)
s = s.replace('color="0.15 0.05 0 1"', 'color="0.18 0.48 0.72 0.6"')
p.write_text(s, encoding='utf-8')
p = ROOT / 'indra/newview/skins/default/colors.xml'
s = p.read_text(encoding='utf-8-sig')
colors = {
 'FloaterDefaultBackgroundColor':'0.025 0.035 0.05 0.82',
 'FloaterFocusBackgroundColor':'0.035 0.05 0.07 0.86',
 'FloaterFocusBorderColor':'0.38 0.70 0.94 0.65',
 'FloaterUnfocusBorderColor':'0.40 0.48 0.56 0.30',
 'EmphasisColor':'0.24 0.60 0.92 0.35',
 'EmphasisColor_13':'0.24 0.60 0.92 0.13',
 'EmphasisColor_35':'0.24 0.60 0.92 0.35',
}
for name,value in colors.items():
 pattern = r'(<color\s+name="'+name+r'"\s+)(?:value|reference)="[^"]*"'
 s,count = re.subn(pattern, lambda m:m[1]+'value="'+value+'"',s)
 assert count == 1, name
p.write_text(s, encoding='utf-8')
for path in [textures/'textures.xml',p,ROOT/'indra/newview/skins/default/xui/en/panel_login.xml']:
 ET.parse(path)
print('Static fire garden, Beyond Reality emblem, translucent obsidian panels and blue accents installed. No video or animation.')
