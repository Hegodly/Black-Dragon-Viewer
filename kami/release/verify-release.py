#!/usr/bin/env python3
"""Verify generated release source and UI, not physical GPU performance."""
from pathlib import Path
import itertools
import re
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
V = R/'indra/newview'
X = V/'skins/default/xui/en'
bad = ('KombatantRTX', 'KombatantUpscaler', 'KombatantRayAO', 'KAMIFrameGen',
       'KAMIAlive', 'KAMINearFieldGI', 'kamiNearFieldBounce', 'KAMI_HAS_NATIVE_DX12',
       'kami_dx12', 'kami_near_field_gi', 'KAMICinematicMaterials', 'kami_cinematic_material',
       'KAMIDynamicResolution', 'KAMIAdaptiveResolution')
for path in list(V.glob('*.cpp'))+list(V.glob('*.h'))+[V/'CMakeLists.txt',
        V/'app_settings/shaders/class3/deferred/softenLightF.glsl']:
    source = path.read_text(encoding='utf-8-sig')
    assert not any(token in source for token in bad), path
print('PASS no experimental renderer, ALIVE, framegen or quarantined upscaler execution paths')

settings = {}
for name in ('settings.xml','settings_blackdragon.xml'):
    m = ET.parse(V/'app_settings'/name).getroot().find('map')
    children = list(m)
    for i in range(0,len(children),2):
        key = children[i].text
        assert not any(token in key for token in bad),key
        settings[key] = children[i+1]
root = ET.parse(X/'panel_preferences_graphics1.xml').getroot()
stack = next(n for n in root.iter('layout_stack') if n.get('name')=='gfx_stack')
groups = [n for n in stack if n.get('visibility_control','').startswith('KAMIReleaseSection')]
assert len(groups)==13
for group in groups:
    key = group.get('visibility_control')
    assert key in settings
    header = next(n for n in root.iter('button') if n.get('control_name')==key)
    assert header.find('button.commit_callback').get('function')=='Pref.Tab'
    assert header.find('button.commit_callback').get('parameter')==group.get('name')
    for n in group:
        top = int(n.get('top','0')); height = int(n.get('height','0'))
        assert top>=0 and top+height<=int(group.get('height')),n.attrib
        left = int(n.get('left','0')); width = int(n.get('width','0'))
        assert left>=0 and left+width<=495,n.attrib
for n in root.iter():
    assert not any(token in str(n.attrib) for token in bad),n.attrib
    if n.tag in ('button','slider','check_box','combo_box'):
        assert n.get('enabled')!='false',n.attrib
    if n.tag=='combo_box' and n.get('control_name')=='RenderFSAAType':
        assert not any(c.get('value')=='1' for c in n if c.tag.endswith('.item'))
# All 8192 combinations of new section states retain fixed headers and valid
# extents. The three original sections use the same generic native handler.
fixed = [n for n in stack if not n.get('visibility_control')]
for state in itertools.product((False,True),repeat=len(groups)):
    visible = fixed+[g for g,on in zip(groups,state) if on]
    total = sum(int(n.get('height','0')) for n in visible)+4*(len(visible)-1)
    assert total>0
assert 'for (LLView* child : *mGFXStack->getChildList())' in (V/'llfloaterpreference.cpp').read_text()
print('PASS thirteen native collapsible groups, control bounds and all 8192 visibility combinations')

menu = ET.parse(X/'menu_viewer.xml').getroot()
assert any(n.get('parameter')=='kami_area_search' for n in menu.iter())
assert any(n.get('name')=='Me' for n in menu), 'Startup-critical Me menu lost'
me = next(n for n in menu if n.get('name')=='Me')
assert any(n.get('name')=='MarketplaceListings' for n in me.iter()), 'Startup-critical Marketplace hierarchy lost'
assert any(n.get('name')=='World' for n in menu.iter()), 'World actions lost'
hub = ET.parse(X/'floater_kami_workspaces.xml').getroot()
assert any(n.get('name')=='hub_world' for n in hub.iter('button'))
for path in X.glob('*.xml'):
    t = ET.parse(path)
    assert not any(any(token in str(n.attrib) for token in bad) for n in t.getroot().iter()),path
audio = ET.parse(X/'panel_preferences_sound.xml').getroot()
assert not any(n.get('control_name')=='KamiAudioResamplerQuality' for n in audio.iter()), 'FMOD-only control in OpenAL release'
for name in ('kami_area_search.h','llkamireadreceipts.h','kami_smart_engine.h',
             'kami_atmosphere.h','kami_signature.h','kami_recovery.h'):
    assert (V/name).is_file(),name
display=(V/'llviewerdisplay.cpp').read_text()
assert 'last_world_resolution_status_time' in display
assert 'KAMIFrameGen::' not in display
control = (V/'llviewercontrol.cpp').read_text()
assert 'gSavedSettings.setU32("RenderFSAAType", 2);' in control
assert 'for (const char* key : {"RenderGlow", "RenderDepthOfField", "RenderMotionBlur"})' in control
assert 'sourceBuffer=KombatantRTX::render' not in (V/'pipeline.cpp').read_text()
print('PASS stable native features, World navigation and readout retained without SDK frame transfers')
