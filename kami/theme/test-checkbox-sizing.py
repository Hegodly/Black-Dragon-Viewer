#!/usr/bin/env python3
"""Exercise actual theme functions: fixed glyph size, preserved label hit area."""
import ast
from pathlib import Path
import tempfile
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw
SOURCE=Path(__file__).with_name('apply-fire-garden-ui.py')
module=ast.parse(SOURCE.read_text())
functions=[n for n in module.body if isinstance(n,ast.FunctionDef) and n.name in ('checkbox','fix_checkbox_scaling')]
assert len(functions)==2
with tempfile.TemporaryDirectory() as d:
    root=Path(d);(root/'widgets').mkdir()
    path=root/'widgets/check_box.xml'
    path.write_text('<check_box><check_box.label_text left="34"/><check_box.check_button width="18" height="18" scale_image="true"/></check_box>')
    context={'ET':ET,'Image':Image,'ImageDraw':ImageDraw,'WIN':root,'blue':(77,187,224),'silver':(167,176,183)}
    exec(compile(ast.Module(body=functions,type_ignores=[]),str(SOURCE),'exec'),context)
    context['checkbox']('off.png',False);context['checkbox']('on.png',True)
    for name in ('off.png','on.png'):
        with Image.open(root/name) as image:assert image.size==(18,18)
    context['fix_checkbox_scaling'](root)
    tree=ET.parse(path);node=tree.getroot().find('check_box.check_button')
    assert node.get('scale_image')=='false'
    assert node.get('width')==node.get('height')=='18'
    assert tree.getroot().find('check_box.label_text').get('left')=='34'
    print('PASS 18px checkbox assets, unscaled rendering, original label placement and hit-area geometry')
