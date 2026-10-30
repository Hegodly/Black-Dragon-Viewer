#!/usr/bin/env python3
"""Wire KAMI Smart Performance Engine to the Windows viewer's real render loop.

Called after KAMI native graphics patch, before CMake configure. Fails on
changed anchors rather than silently adding decorative controls.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]

def edit(path, before, after, count=1):
    p=ROOT/path
    text=p.read_text(encoding="utf-8-sig")
    assert text.count(before)==count,(path,before[:80],text.count(before))
    p.write_text(text.replace(before,after,count),encoding="utf-8")

setting_path="indra/newview/app_settings/settings_blackdragon.xml"
entries = [
    ("KAMIAutoTune","Boolean","0","Automatically adapt existing native graphics settings toward target FPS."),
    ("KAMIOptimizeAvatars","Boolean","1","Reduce number of distant fully rendered avatars using impostors."),
    ("KAMIProtectMyAvatar","Boolean","1","Keep global avatar LOD untouched, preserving own-avatar quality."),
    ("KAMIThrottleDistantAvatars","Boolean","1","Prefer reduced updates through the existing distant-avatar impostor path."),
    ("KAMISmartTextures","Boolean","1","Use viewer auto VRAM budget and optional lower non-boosted texture size."),
    ("KAMIAdaptiveShadows","Boolean","1","Lower existing shadow detail and resolution where needed."),
    ("KAMIAdaptiveReflections","Boolean","1","Reduce reflection probe and screen-space reflection cost."),
    ("KAMIAdaptiveParticles","Boolean","1","Reduce max particle count at higher performance stages."),
    ("KAMIAdaptiveDrawDistance","Boolean","1","Reduce render distance and object level of detail."),
    ("KAMIAdaptiveResolution","Boolean","1","Reduce internal 3D world resolution at high performance stages."),
    ("KAMICrowdMode","Boolean","0","Force minimum adaptive optimization stage 3 in busy regions."),
    ("KAMIPotatoMode","Boolean","0","Force native stage 6 low-quality rendering for very weak PCs."),
    ("KAMIBackgroundFPS","S32","15","Frame cap while KAMI Viewer is not focused; 0 disables."),
    ("KAMISmartBackup","String","","Persistent XML snapshot of manual settings before auto optimization."),
]
def setting_xml(key, typ, value, comment):
    if typ=="Boolean": raw=f"<integer>{value}</integer>"
    elif typ=="S32": raw=f"<integer>{value}</integer>"
    else: raw=f"<string>{value}</string>"
    return (
        f"  <key>{key}</key>\n"
        f"  <map><key>Comment</key><string>{comment}</string>"
        f"<key>Persist</key><integer>{'0' if key=='KAMITargetFPS' else '1'}</integer>"
        f"<key>Type</key><string>{typ}</string><key>Value</key>{raw}</map>\n"
    )
extra="".join(setting_xml(*entry) for entry in entries)
root='  xsi:noNamespaceSchemaLocation="llsd.xsd">\n<map>\n'
edit(setting_path,root,root+extra)
# Copy the native controller into the compiled viewer source location.
shutil.copy2(ROOT/"kami/performance/kami_smart_engine.h",
             ROOT/"indra/newview/kami_smart_engine.h")
viewer="indra/newview/llappviewer.cpp"
edit(viewer,'#include "llviewercontrol.h"\n',
     '#include "llviewercontrol.h"\n#include "kami_smart_engine.h"\n')
# This location measures one actual rendered frame, before the existing FPS limiter.
edit(viewer,'                display();\n',
     '''                display();
                // Native KAMI adaptive quality; uses actual viewer frame cadence.
                KAMISmart::tick();
                // Prevent unnecessary background rendering on battery-powered PCs.
                KAMISmart::limitBackgroundFrames();
''')

panel="indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
p=ROOT/panel
s=p.read_text(encoding="utf-8-sig")
start=s.index('<layout_panel name="kombatant_display_quick"')
end=s.index('</layout_panel>',start)
area=s[start:end]
assert 'height="1760" auto_resize="false"' in area
area=area.replace('height="1760" auto_resize="false"',
                  'height="2380" auto_resize="false"',1)
extras='''       <text name="kami_smart_title" left="12" top="1705" width="465" height="24" font="SansSerifLarge">Smart Performance Engine (native)</text>
       <check_box name="kami_auto" label="Auto-adjust graphics toward target FPS" control_name="KAMIAutoTune" left="12" top="1735" width="465" height="22" />
       <combo_box name="kami_target" control_name="KAMITargetFPS" left="12" top="1765" width="465" height="24">
        <combo_box.item label="Target 30 FPS" value="30" />
        <combo_box.item label="Target 45 FPS" value="45" />
        <combo_box.item label="Target 60 FPS" value="60" />
        <combo_box.item label="Target 90 FPS" value="90" />
        <combo_box.item label="Target 100 FPS" value="100" />
        <combo_box.item label="Target 120 FPS" value="120" />
       </combo_box>
       <check_box name="kami_potato" label="Potato Mode — emergency low-end rendering" control_name="KAMIPotatoMode" left="12" top="1800" width="465" height="22" />
       <check_box name="kami_crowd" label="Crowd Mode — aggressive distant-scene budgets" control_name="KAMICrowdMode" left="12" top="1828" width="465" height="22" />
       <check_box name="kami_protect" label="Protect my avatar appearance" control_name="KAMIProtectMyAvatar" left="12" top="1865" width="465" height="22" />
       <check_box name="kami_avatars" label="Optimize distant avatars using impostors" control_name="KAMIOptimizeAvatars" left="12" top="1893" width="465" height="22" />
       <check_box name="kami_animations" label="Reduce distant avatar update cost (impostors)" control_name="KAMIThrottleDistantAvatars" left="12" top="1921" width="465" height="22" />
       <check_box name="kami_resolution" label="Adaptive 3D resolution (world only)" control_name="KAMIAdaptiveResolution" left="12" top="1949" width="465" height="22" />
       <check_box name="kami_distance" label="Adaptive draw distance and mesh LOD" control_name="KAMIAdaptiveDrawDistance" left="12" top="1977" width="465" height="22" />
       <check_box name="kami_shadows" label="Adaptive shadow budget" control_name="KAMIAdaptiveShadows" left="12" top="2005" width="465" height="22" />
       <check_box name="kami_reflections" label="Adaptive reflections and probes" control_name="KAMIAdaptiveReflections" left="12" top="2033" width="465" height="22" />
       <check_box name="kami_particles" label="Adaptive particle budget" control_name="KAMIAdaptiveParticles" left="12" top="2061" width="465" height="22" />
       <check_box name="kami_textures" label="Smart auto VRAM and texture budget" control_name="KAMISmartTextures" left="12" top="2089" width="465" height="22" />
       <text name="kami_background_label" left="12" top="2124" width="235" height="20">Background FPS cap</text>
       <combo_box name="kami_background" control_name="KAMIBackgroundFPS" left="240" top="2118" width="237" height="24">
        <combo_box.item label="Off" value="0" />
        <combo_box.item label="5 FPS" value="5" />
        <combo_box.item label="10 FPS" value="10" />
        <combo_box.item label="15 FPS" value="15" />
        <combo_box.item label="30 FPS" value="30" />
       </combo_box>
       <text name="kami_smart_help" left="12" top="2155" width="465" height="123" wrap="true">Target FPS is advisory, not a guarantee; the viewer may be CPU-, GPU-, or region-limited. Adaptive quality uses 3.5-second sampling and gradual recovery, with no quality changes while unfocused. Distant animation savings come from existing avatar impostors, not a separate animation renderer. Settings restore when all smart modes are disabled. Own-avatar protection avoids lowering global avatar LOD.</text>
'''
extras=__import__("re").sub(r'\btop="(\d+)"',lambda m: 'top="'+str(int(m.group(1))+75)+'"',extras)
area=area.replace('<combo_box.item label="90 FPS" value="90" />','<combo_box.item label="90 FPS" value="90" />\n        <combo_box.item label="100 FPS" value="100" />',1)
area+=extras
s=s[:start]+area+s[end:]
assert 'height="1764"' in s
s=s.replace('height="1764"','height="2384"',1)
p.write_text(s,encoding="utf-8")

# CI fails on invalid XML before downloading SDKs.
settings_tree=ET.parse(ROOT/setting_path)
keys=[el.text for el in settings_tree.getroot().find("map").findall("key")]
repeated=sorted(k for k in set(keys) if keys.count(k)>1)
print("Existing duplicate LLSD keys:",repeated)
assert all(keys.count(entry[0])==1 for entry in entries), "One of the new KAMI settings was duplicated"
assert keys.count("KAMITargetFPS")==1, "KAMI target setting duplicated"
ET.parse(ROOT/panel)
ET.parse(ROOT/"indra/newview/skins/default/xui/en/floater_kombatant_effects.xml")
print("KAMI adaptive controls, quality ladder and background FPS limiter installed.")
