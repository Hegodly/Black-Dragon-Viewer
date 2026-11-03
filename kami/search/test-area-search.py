#!/usr/bin/env python3
"""Exercise the real installer in an isolated source fixture or check CI output."""
from pathlib import Path
import argparse, shutil, subprocess, sys, tempfile
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
def verify(root):
    view=root/'indra/newview';xui=view/'skins/default/xui/en'
    reg=(view/'llviewerfloaterreg.cpp').read_text(encoding='utf-8-sig')
    select=(view/'llselectmgr.cpp').read_text(encoding='utf-8-sig')
    assert reg.count('LLFloaterReg::add("kami_area_search",')==1
    assert select.count('KAMIAreaSearch::receive(msg);')==1
    ui=ET.parse(xui/'floater_kami_area_search.xml').getroot()
    assert {e.get('name') for e in ui.iter('button')}=={'refresh','stop','filter','focus','beacon'}
    assert {e.get('name') for e in ui.iter('line_editor')}=={'name_filter','description_filter','owner_filter'}
    menu=ET.parse(xui/'menu_viewer.xml').getroot()
    me=next(e for e in menu if e.get('name')=='Me')
    assert any(e.get('name')=='MarketplaceListings' for e in me.iter())
    world=next(e for e in me if e.get('name')=='World')
    items=[e for e in world if e.get('name')=='Kami.AreaSearch'];assert len(items)==1
    assert items[0].find('menu_item_call.on_click').get('parameter')=='kami_area_search'
    print('PASS Area Search native registration, reply hook, UI controls and startup-safe menu')
if '--integrated' in sys.argv:verify(ROOT)
else:
    with tempfile.TemporaryDirectory() as d:
        root=Path(d)
        for name in ['kami/search/apply-area-search.py','kami/search/kami_area_search.h',
                     'indra/newview/llviewerfloaterreg.cpp','indra/newview/llselectmgr.cpp',
                     'indra/newview/skins/default/xui/en/menu_viewer.xml']:
            target=root/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,target)
        subprocess.run([sys.executable,str(root/'kami/search/apply-area-search.py')],check=True)
        verify(root)
