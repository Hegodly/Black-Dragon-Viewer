#!/usr/bin/env python3
"""Install the native KAMI Area Search floater and properties reply hook."""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
VIEW=ROOT/'indra/newview'
XUI=VIEW/'skins/default/xui/en'
def replace(path,old,new):
    src=path.read_text(encoding='utf-8-sig')
    assert src.count(old)==1,(path,old)
    path.write_text(src.replace(old,new,1),encoding='utf-8')
shutil.copyfile(ROOT/'kami/search/kami_area_search.h',VIEW/'kami_area_search.h')
replace(VIEW/'llviewerfloaterreg.cpp','#include "llfloaterperformance.h"',
        '#include "llfloaterperformance.h"\n#include "kami_area_search.h"')
anchor='    LLFloaterReg::add("preferences", "floater_preferences.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterPreference>);'
replace(VIEW/'llviewerfloaterreg.cpp',anchor,anchor+'\n    LLFloaterReg::add("kami_area_search", "floater_kami_area_search.xml", (LLFloaterBuildFunc)&LLFloaterReg::build<KAMIAreaSearch>);')
replace(VIEW/'llselectmgr.cpp','#include "llselectmgr.h"',
        '#include "llselectmgr.h"\n#include "kami_area_search.h"')
anchor='void LLSelectMgr::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)\n{'
replace(VIEW/'llselectmgr.cpp',anchor,anchor+'\n    KAMIAreaSearch::receive(msg);')
f=ET.Element('floater',name='kami_area_search',title='KAMI | AREA SEARCH',layout='topleft',width='820',height='460',can_resize='false',can_close='true',save_rect='true')
ET.SubElement(f,'text',name='help',left='16',top='40',width='788',height='32',wrap='true').text='Search objects known to your viewer in the current region. Turn or move to discover more, then Refresh. Maximum 4,096 root objects; attachments excluded.'
for name,label,left,width in [('name','Object name',16,245),('description','Description',270,260),('owner','Owner name or UUID',540,264)]:
    ET.SubElement(f,'line_editor',name=name+'_filter',label=label,left=str(left),top='82',width=str(width),height='24',max_length_chars='256')
results=ET.SubElement(f,'scroll_list',name='results',left='16',top='118',width='788',height='250',draw_heading='true',multi_select='false')
for name,label,width in [('distance','Meters',70),('name','Name',210),('description','Description',270),('owner','Owner',215)]:
    ET.SubElement(results,'column',name=name,label=label,width=str(width))
ET.SubElement(f,'text',name='status',left='16',top='378',width='788',height='24').text='Ready'
for i,(name,label) in enumerate([('refresh','REFRESH'),('stop','PAUSE'),('filter','SEARCH'),('focus','FOCUS CAMERA'),('beacon','SHOW BEACON')]):
    ET.SubElement(f,'button',name=name,label=label,left=str(16+i*158),top='414',width='150',height='28')
ET.indent(f);ET.ElementTree(f).write(XUI/'floater_kami_area_search.xml',encoding='utf-8',xml_declaration=True)
p=XUI/'menu_viewer.xml';t=ET.parse(p);menus=[x for x in t.getroot().iter('menu') if x.get('name')=='World'];assert len(menus)==1
assert not any(x.get('name')=='Kami.AreaSearch' for x in t.getroot().iter())
e=ET.SubElement(menus[0],'menu_item_call',name='Kami.AreaSearch',label='KAMI Area Search')
ET.SubElement(e,'menu_item_call.on_click',function='Floater.Show',parameter='kami_area_search')
ET.indent(t);t.write(p,encoding='utf-8',xml_declaration=True)
print('PASS native Area Search registry, properties hook, floater and World menu installed')
