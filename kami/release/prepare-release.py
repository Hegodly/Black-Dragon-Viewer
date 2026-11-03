#!/usr/bin/env python3
"""Remove prototype execution paths and finish native release settings layout.

Run after the historical integration scripts and before native compilation.
Optional, functioning features remain optional. No GPU work is added here.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / 'indra/newview'
XUI = VIEW / 'skins/default/xui/en'

def edit(relative, transform):
    p = VIEW / relative
    before = p.read_text(encoding='utf-8-sig')
    after = transform(before)
    p.write_text(after, encoding='utf-8')

def between(s, first, last, replacement=''):
    a = s.index(first)
    b = s.index(last, a)
    return s[:a] + replacement + s[b:]

def remove_dx12(s):
    # These explicit feature guards contain no nested preprocessor branches.
    return re.sub(r'^#ifdef KAMI_HAS_NATIVE_DX12\n.*?^#endif\n', '', s,
                  flags=re.M | re.S)

for relative in ('llappviewerwin32.cpp', 'llviewerdisplay.cpp'):
    edit(relative, remove_dx12)
edit('CMakeLists.txt', lambda s: between(s,
    '# KAMI DX12: compile the native renderer',
    'target_link_libraries(${VIEWER_BINARY_NAME}\n'))

def pipeline(s):
    s = s.replace('#include "kombatantrtx.h"\n', '')
    s = s.replace('#include "kombatantrtxstate.h"\n', '')
    s = between(s, '    // DLSS takes precedence',
                '    const U32 target = gSavedSettings.getU32("KombatantRenderWidth");')
    s = s.replace(' || KombatantRTXState::wantsUpscaler()', '')
    s = s.replace('    KombatantRTX::release();\n', '')
    s = s.replace('    sourceBuffer=KombatantRTX::render(sourceBuffer,&mRT->deferredScreen,&mVelocityMap);\n', '')
    s = s.replace('    ++KombatantRTXState::frameIndex;\n', '')
    s = s.replace('!KombatantRTX::hadDLSS && ', '')
    s = between(s, '            static LLStaticHashedString kami_bounce_uniform',
                '            soften_shader.uniform1f(kami_bounce_uniform, bounce);').replace(
                '            soften_shader.uniform1f(kami_bounce_uniform, bounce);\n', '')
    s = between(s, '            // One scalar uniform on the existing deferred lighting pass;',
                '            static LLCachedControl<F32> ssao_scale')
    return s
edit('pipeline.cpp', pipeline)

def camera(s):
    s = s.replace('#include "kombatantrtxstate.h"\n', '')
    s = between(s, '        const bool dlss=KombatantRTXState::wantsUpscaler();',
                '        proj_mat[2][0] += jx;',
                '        float jx = jitters[idx][0] * 2.0f / llmax(1, width);\n'
                '        float jy = jitters[idx][1] * 2.0f / llmax(1, height);\n')
    return s
edit('llviewercamera.cpp', camera)

def display(s):
    s = s.replace('#include "kombatantrtxstate.h"\n', '')
    s = s.replace(' || KombatantRTXState::wantsUpscaler()', '')
    s = s.replace('#if LL_WINDOWS\n#include "kami_framegen.h"\n#endif\n', '')
    s = between(s, '#if LL_WINDOWS\n        gGL.flush();',
                '        gViewerWindow->getWindow()->swapBuffers();')
    # Preserve the independently throttled real framebuffer readout.
    s = between(s, '        // Update text only on status changes;',
                '        // A label for the *allocated* world target, not a guessed mode.')
    return s
edit('llviewerdisplay.cpp', display)
edit('llviewerwindow.cpp', lambda s: s.replace(
    '#if LL_WINDOWS\n#include "kami_framegen.h"\n#endif\n', '').replace(
    '#if LL_WINDOWS\n        KAMIFrameGen::generator().release();\n#endif', ''))
edit('llfloaterpreference.cpp', lambda s: re.sub(
    r'^    if\s*\(auto\*?\s*(?:text)?\s*=\s*findChild<LLTextBox>\("(?:kami_fg_status|kd_rtx_status)"\)\)\n[^\n]*\n',
    '', s, flags=re.M))
edit('llviewercontrol.cpp', lambda s: '\n'.join(
    line for line in s.split('\n') if not ('setting_setup_signal_listener' in line and
    any(k in line for k in ('KombatantUpscaler', 'KombatantUpscaleQuality', 'KombatantRayAO', 'KombatantDLSS')))))
edit('llviewercontrol.cpp', lambda s: s.replace('void settings_setup_listeners()\n{',
    '''void settings_setup_listeners()
{
    // Migrate retired choices before graphics listeners are installed. Saved
    // settings from test builds must not reactivate hidden, broken effects.
    if (gSavedSettings.getU32("RenderFSAAType") == 1)
        gSavedSettings.setU32("RenderFSAAType", 2);
    for (const char* key : {"RenderGlow", "RenderDepthOfField", "RenderMotionBlur"})
        gSavedSettings.setBOOL(key, false);
''', 1))
edit('app_settings/shaders/class3/deferred/softenLightF.glsl', lambda s:
    between(s, '// Optional four-tap near-field', 'void main()\n{').replace(
    '        irradiance += kamiNearFieldBounce(tc, pos.xyz, gb.normal);\n', ''))
edit('app_settings/shaders/class3/deferred/softenLightF.glsl', lambda s:
    between(s, '        // Existing glTF ORM and normal maps remain source-of-truth.',
            '        float metallic = orm.b;').replace(
    '// KAMI: optional PBR-only material response. No new textures/shader pass.\nuniform float kami_cinematic_material;\n', ''))
edit('kami_quality_presets.h', lambda s: '\n'.join(
    line for line in s.split('\n') if 'set("KAMINearFieldGI"' not in line))
for header in ('kami_smart_engine.h', 'kami_performance_mode.h'):
    edit(header, lambda s: re.sub(
        r'^.*(?:set\("Kombatant(?:UpscalerProvider|RayAO)"|getBOOL\("KAMIAdaptiveResolution"\)).*\n',
        '', s, flags=re.M).replace('"KombatantUpscalerProvider", ', '').replace(
        '"KombatantRayAO", ', '').replace(', "KombatantRayAO"', '').replace(
        ', "KombatantNativeFX"', ''))

retired_prefixes = ('KombatantUpscal', 'KombatantDLSS', 'KombatantRayAO',
                    'KombatantRTX', 'KombatantAutoPerformanceGuard',
                    'KAMIAlive', 'KAMIFrameGeneration', 'KamiDX12',
                    'KAMINearFieldGI', 'KAMICinematicMaterials', 'KAMIDynamicResolution', 'KAMIAdaptiveResolution')
def retired(value):
    return bool(value and value.startswith(retired_prefixes))

for relative in ('app_settings/settings.xml', 'app_settings/settings_blackdragon.xml'):
    p = VIEW / relative
    t = ET.parse(p)
    m = t.getroot().find('map')
    entries = list(m)
    for i in range(0, len(entries), 2):
        if retired(entries[i].text):
            m.remove(entries[i]); m.remove(entries[i+1])
    ET.indent(t, space=' '); t.write(p, encoding='utf-8', xml_declaration=True)

for p in XUI.glob('*.xml'):
    t = ET.parse(p); changed = False
    for parent in list(t.getroot().iter()):
        for node in list(parent):
            name = node.get('name', '')
            references = [node.get('control_name'), node.get('control')]
            # Inspect this widget's own callbacks, not entire descendant menus:
            # a retired menu item must never remove its World/Me ancestors.
            references += [n.get('control') or n.get('parameter') for n in node]
            if (any(retired(v) for v in references) or
                name.startswith(('kami_alive_', 'kami_fg_', 'kd_rtx_', 'kd_ray_', 'kd_upscal')) or
                name in ('kd_guard', 'kami_adaptive_resolution', 'kami_resolution', 'kami_mw_gi', 'kami_mw_shader_cache',
                         'kami_audio_quality_label', 'kami_audio_quality')):
                parent.remove(node); changed = True
    if changed:
        ET.indent(t, space=' '); t.write(p, encoding='utf-8', xml_declaration=True)

for name in ('kombatantrtx.h', 'kombatantrtxstate.h', 'kami_framegen.h',
             'kami_dx12_live_preview.cpp', 'kami_dx12_live_preview.h',
             'kami_dx12_scene_bridge.cpp', 'kami_dx12_scene_bridge.h'):
    (VIEW / name).unlink(missing_ok=True)
for name in ('floater_kombatant_effects.xml', 'floater_kombatant_studio.xml'):
    (XUI / name).unlink(missing_ok=True)

# Replace the enormous always-open quick panel with native collapsible groups.
# Names/bindings stay intact so existing C++ lookups and callbacks still work.
p = XUI / 'panel_preferences_graphics1.xml'
t = ET.parse(p); root = t.getroot()
for parent in root.iter():
    for node in list(parent):
        # Explicitly unavailable widgets are removed; dynamic capability gates
        # on working settings remain in place.
        if node.get('enabled') == 'false' and node.tag in ('check_box','slider','combo_box','button'):
            parent.remove(node)
        elif node.tag == 'combo_box' and node.get('control_name') == 'RenderFSAAType':
            for item in list(node):
                if item.tag.endswith('.item') and item.get('value') == '1': node.remove(item)
        if node.get('name') == 'kami_cine_features':
            node.text = 'Local cinematic lighting and avatar detail controls.'
        if node.get('name') == 'kd_fx_note':
            node.text = 'Screen-space reflections use visible scene data. Detailed lighting controls are available below.'
        if node.get('name') == 'kd_quality_preset':
            node.set('tool_tip','Changes scene detail, avatars, lights, shadows, reflections and antialiasing. Preserves your frame cap and selected render resolution.')
stack = next(n for n in root.iter('layout_stack') if n.get('name') == 'gfx_stack')
quick = next(n for n in stack if n.get('name') == 'kombatant_display_quick')
intro = next(n for n in stack if n.get('name') == 'kami_graphics_intro_card')
intro.set('height', '64')
for n in list(intro):
    if n.get('name') == 'kami_graphics_frame_note': intro.remove(n)
intro.find("text[@name='kami_graphics_subtitle']").text = 'DISPLAY / QUALITY / PERFORMANCE'
starts = {
    'kami_smart_title': 'Smart performance',
    'kami_bench_header': 'Benchmark',
    'kami_budget_heading': 'Lighting and shadows',
    'kami_mw_heading': 'Advanced graphics',
    'kami_physics_heading': 'Avatar physics',
    'kami_doctor_head': 'Render Doctor',
    'kami_world_head': 'Smart World',
    'kami_camera_head': 'Camera Director',
    'kami_profile_head': 'Graphics profiles',
    'kami_atmo_title': 'Atmosphere',
    'kami_recovery_title': 'Session recovery',
    'kami_avatar_quality_heading': 'Avatar quality and cinematic options',
}
groups = [('Display and quality', [])]
for node in list(quick):
    if node.get('name') in starts: groups.append((starts[node.get('name')], []))
    groups[-1][1].append(node)
    quick.remove(node)

settings_path = VIEW / 'app_settings/settings_blackdragon.xml'
st = ET.parse(settings_path); settings = st.getroot().find('map')
insert = list(stack).index(quick)
stack.remove(quick)
for i, (caption, nodes) in enumerate(groups):
    control = f'KAMIReleaseSection{i}'
    ET.SubElement(settings, 'key').text = control
    m = ET.SubElement(settings, 'map')
    for key, tag, value in [('Comment','string',f'Expand {caption} in Graphics.'),
                            ('Persist','integer','1'),('Type','string','Boolean'),
                            ('Value','integer','1' if i == 0 else '0')]:
        ET.SubElement(m,'key').text=key; ET.SubElement(m,tag).text=value
    content = ET.Element('layout_panel', name='kombatant_display_quick' if i == 0 else f'kami_release_group_{i}',
        layout='topleft', width='495', height='1', auto_resize='false',
        visibility_control=control, follows='top|left|right')
    top = min(int(n.get('top','0')) for n in nodes)
    for node in nodes:
        node.set('top', str(int(node.get('top','0')) - top + 8))
        if node.get('name') in starts:
            node.text = caption
            node.set('font','SansSerifBold')
        content.append(node)
    content.set('height', str(max(int(n.get('top')) + int(n.get('height','0')) for n in nodes)+8))
    header = ET.Element('layout_panel', name=f'kami_release_header_{i}', width='495',
        height='32', layout='topleft', auto_resize='false', follows='top|left|right')
    b = ET.SubElement(header,'button', name=f'kami_release_expand_{i}', label=caption,
        control_name=control, is_toggle='true', left='8', top='2', width='469', height='28',
        layout='topleft', follows='top|left|right', halign='left',
        image_overlay='Accordion_ArrowClosed_Off', image_overlay_selected='Accordion_ArrowOpened_Off',
        image_overlay_alignment='right')
    ET.SubElement(b,'button.commit_callback',function='Pref.Tab', parameter=content.get('name'))
    stack.insert(insert,header); stack.insert(insert+1,content); insert += 2
stack.set('width','495')
for panel in stack: panel.set('width','495')
# Keep artwork at login; an opaque reading surface prevents scenery behind text.
prefs_path = XUI / 'floater_preferences.xml'
prefs = ET.parse(prefs_path)
for node in prefs.getroot().iter():
    if node.get('name') == 'vicon': node.set('visible','false')
prefs.write(prefs_path, encoding='utf-8', xml_declaration=True)
ET.indent(t,space=' '); t.write(p,encoding='utf-8',xml_declaration=True)
ET.indent(st,space=' '); st.write(settings_path,encoding='utf-8',xml_declaration=True)
# Final extent includes every group; the native toggle handler sizes visible
# sections again after opening/collapsing and on preferences refresh.
total = sum(int(n.get('height','0')) for n in stack) + 4 * (len(stack)-1)
stack.set('height',str(total))
scroll = next(n for n in root.iter('panel') if n.get('name') == 'gfx_scroll_panel')
scroll.set('height',str(total))
ET.indent(t,space=' '); t.write(p,encoding='utf-8',xml_declaration=True)
print(f'PASS release cleanup: native renderer, no prototype hooks; {len(groups)} collapsible settings groups')
