#!/usr/bin/env python3
"""Integrate KAMI four signature features into the real Windows viewer.

Runs after hardware monitor, avatar physics, and Fire Garden skin patches.
Uses existing renderer and camera APIs, with no GL layer, hook or UI mockup.
"""
from pathlib import Path
import xml.etree.ElementTree as ET
import shutil

R=Path(__file__).resolve().parents[2]
skin=R/"indra/newview/skins/default"
src=R/"indra/newview"
shutil.copyfile(R/"kami/signature/kami_signature.h",src/"kami_signature.h")

settings=src/"app_settings/settings_blackdragon.xml"
doc=ET.parse(settings);root=doc.getroot().find("map")
assert root is not None
defs=[
 ("KAMIDoctorEnabled","Boolean","integer","0",1,"Enable periodic measured FPS diagnosis; never auto-modifies settings."),
 ("KAMIDoctorStatus","String","string","Off. Enable Doctor and open Hardware Monitor for trustworthy sensor measurements.",0,"KAMI Doctor diagnosis based on live FPS and available Windows performance metrics."),
 ("KAMIDoctorBackup","String","string","",1,"Single user-approved Render Doctor change, persisted for undo."),
 ("KAMIWorldSmart","Boolean","integer","0",1,"Reversible native world budget adaptation, disabled when Auto Tune or Performance owns rendering."),
 ("KAMIWorldBackup","String","string","",1,"Snapshot of manual world graphics before Smart World took ownership."),
 ("KAMIWorldStatus","String","string","Off. Original draw distance, impostor and light settings preserved.",0,"Smart World native stage, FPS and pause reasons."),
 ("KAMIProfileSelected","String","string","Exploration",1,"Chosen profile for manual load/save and region assignment."),
 ("KAMIProfileBank","String","string","",1,"User-customized snapshots of named KAMI profiles."),
 ("KAMIProfileManualBackup","String","string","",1,"Restore user's original graphics before switching profiles."),
 ("KAMIRegionProfileBank","String","string","",1,"Optional region UUID to graphic profile mapping, local only."),
 ("KAMIRegionProfilesEnabled","Boolean","integer","0",1,"Automatically apply previously remembered profiles when entering their region."),
 ("KAMIProfileStatus","String","string","Choose Club, Photography, Exploration or Low-end PC. Auto-region recall OFF.",0,"Current KAMI Intelligent Profile selection/action status."),
 ("KAMICameraStart","String","string","",1,"Recorded camera keyframe A, including global position, focus and region."),
 ("KAMICameraEnd","String","string","",1,"Recorded camera keyframe B, including global position, focus and region."),
 ("KAMICameraPathSeconds","F32","real","8",1,"Duration for smooth camera path from keyframe A to B."),
 ("KAMICameraTrackSelf","Boolean","integer","0",1,"During camera playback, aim at local avatar's position."),
 ("KAMICameraStatus","String","string","Record camera keyframes A and B to make a cinematic shot.",0,"Playback state of camera director."),
 ("KAMISensorTime","F32","real","0",0,"Age of last optional Hardware Monitor sample. No background sensor polling."),
 ("KAMISensorGPU","F32","real","-1",0,"Busiest PDH GPU engine percentage; may differ from rendering GPU."),
 ("KAMISensorCPU","F32","real","-1",0,"System CPU percentage reported by Windows."),
 ("KAMISensorVRAM","F32","real","-1",0,"Adapter zero VRAM allocated as percentage of adapter zero budget, not active GPU proof."),
]
for name,ty,tag,value,persist,comment in defs:
 assert not any(n.tag=="key" and n.text==name for n in root),name
 k=ET.Element("key");k.text=name
 entry=ET.Element("map")
 for typ,txt in [("key","Comment"),("string",comment),("key","Persist"),
                 ("integer",str(persist)),("key","Type"),("string",ty),
                 ("key","Value"),(tag,value)]:
  n=ET.SubElement(entry,typ);n.text=txt
 root.insert(0,entry);root.insert(0,k)
ET.indent(doc,space=" ");doc.write(settings,encoding="utf-8",xml_declaration=True)

# Hook the one existing native display main loop. No new rendering passes.
display=src/"llviewerdisplay.cpp"
s=display.read_text(encoding="utf-8-sig")
a='#include "llviewerdisplay.h"'
assert s.count(a)==1
s=s.replace(a,a+'\n#include "kami_signature.h"',1)
a='    gSnapshot = for_snapshot;'
assert s.count(a)==1
s=s.replace(a,a+'''
    // No extra GPU pass. Optional analytics/world/region/camera algorithms.
    // Never advance keyframe playback while taking a still snapshot.
    if (!gSnapshot) KAMISignature::tick();
''',1)
display.write_text(s,encoding="utf-8")

# Export optional real hardware monitor data. Doctor never invents GPU load
# while monitoring is closed; its sensor timestamp expires in a few seconds.
hw=src/"kami_hardware_monitor.h"
s=hw.read_text(encoding="utf-8-sig")
assert '#include <string>' in s
s=s.replace('#include <string>','#include <string>\n#include <chrono>',1)
a='''    void sample()
    {
        const F64 fps ='''
b='''    void sample()
    {
        // Hardware samples are captured only while this floater is visible.
        const F64 now = std::chrono::duration<F64>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        gSavedSettings.setF32("KAMISensorTime",(F32)now);
        gSavedSettings.setF32("KAMISensorCPU",-1.f);
        gSavedSettings.setF32("KAMISensorGPU",-1.f);
        gSavedSettings.setF32("KAMISensorVRAM",-1.f);
        const F64 fps ='''
assert s.count(a)==1;s=s.replace(a,b)
a='''            if(mPrevKernel && total>0 && total>=idleElapsed)
                label("kami_hw_cpu", llformat("CPU (system): %.1f%%",
                      100.0*(1.0-static_cast<double>(idleElapsed)/static_cast<double>(total))));
            else label("kami_hw_cpu","CPU (system): collecting...");'''
b='''            if(mPrevKernel && total>0 && total>=idleElapsed)
            {
                const F32 measured = (F32)(100.0*
                    (1.0-static_cast<double>(idleElapsed)/static_cast<double>(total)));
                gSavedSettings.setF32("KAMISensorCPU",llclamp(measured,0.f,100.f));
                label("kami_hw_cpu", llformat("CPU (system): %.1f%%", measured));
            }
            else label("kami_hw_cpu","CPU (system): collecting...");'''
assert s.count(a)==1;s=s.replace(a,b)
a='''                    if(highest>=0)
                        label("kami_hw_gpu",llformat("GPU busiest engine: %.1f%%",llclamp(highest,0.0,100.0)));
                    else label("kami_hw_gpu","GPU busiest engine: N/A");'''
b='''                    if(highest>=0)
                    {
                        gSavedSettings.setF32("KAMISensorGPU",(F32)llclamp(highest,0.0,100.0));
                        label("kami_hw_gpu",llformat("GPU busiest engine: %.1f%%",llclamp(highest,0.0,100.0)));
                    }
                    else label("kami_hw_gpu","GPU busiest engine: N/A");'''
assert s.count(a)==1;s=s.replace(a,b)
a='''            if(SUCCEEDED(vramAdapter->QueryVideoMemoryInfo(0,
                         DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info)))
                label("kami_hw_vram",llformat("VRAM (adapter 0): %.1f / %.1f GB (allocation/budget)",
                      static_cast<double>(info.CurrentUsage)/1073741824.0,
                      static_cast<double>(info.Budget)/1073741824.0));
            else label("kami_hw_vram","VRAM (adapter 0): N/A");'''
b='''            if(SUCCEEDED(vramAdapter->QueryVideoMemoryInfo(0,
                         DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info)))
            {
                if (info.Budget>0)
                    gSavedSettings.setF32("KAMISensorVRAM",
                        (F32)(100.0*static_cast<double>(info.CurrentUsage)/
                              static_cast<double>(info.Budget)));
                label("kami_hw_vram",llformat("VRAM (adapter 0): %.1f / %.1f GB (allocation/budget)",
                      static_cast<double>(info.CurrentUsage)/1073741824.0,
                      static_cast<double>(info.Budget)/1073741824.0));
            }
            else label("kami_hw_vram","VRAM (adapter 0): N/A");'''
assert s.count(a)==1;s=s.replace(a,b)
hw.write_text(s,encoding="utf-8")

# Add real Fire Garden themed controls inside native Display scroll. Existing
# User-tested native graphics, physics and hardware menus remain.
p=skin/"xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(p);main=tree.getroot()
panel=[n for n in main.iter() if n.tag=="layout_panel" and n.get("name")=="kombatant_display_quick"]
assert len(panel)==1
panel=panel[0]
height=int(panel.get("height"));assert height>=4200
panel.set("height",str(height+870))
parent=[n for n in main.iter() if n.get("height")==str(height+4)]
assert parent,("graphics quick scroll parent not found",height)
for n in parent:n.set("height",str(height+874))
def widget(tag,name,top,**attributes):
 el=ET.SubElement(panel,tag,{**{"name":name,"left":"12","top":str(top)},**{k:str(v) for k,v in attributes.items()}})
 return el
y=height+6
widget("text","kami_doctor_head",y,width=465,height=23,font="SansSerifLarge").text="KAMI RENDER DOCTOR  |  Measured diagnostics"
widget("check_box","kami_doctor_enable",y+29,width=465,height=22,
 control_name="KAMIDoctorEnabled",label="Diagnose real FPS every 2 seconds (no automatic changes)")
widget("text","kami_doctor_status",y+57,width=465,height=57,wrap="true").text="Off. Open Hardware Monitor for measured CPU/GPU information."
widget("button","kami_doctor_hardware",y+120,width=465,height=25,label="Open Windows CPU / GPU / VRAM monitor")
widget("button","kami_doctor_fix",y+153,width=223,height=27,label="Apply suggested single fix")
widget("button","kami_doctor_undo",y+186,width=223,height=27,label="Undo Doctor fix")
widget("text","kami_world_head",y+226,width=465,height=23,font="SansSerifLarge").text="KAMI SMART WORLD  |  Scene-aware optimization"
widget("check_box","kami_world_enabled",y+254,width=465,height=22,
 control_name="KAMIWorldSmart",label="Adaptive native avatar impostors / lighting budgets",
 tool_tip="Uses the viewer's existing distance/screen-size impostor pipeline; intelligently reduces work at low FPS. Preserves full render resolution and restores prior settings.")
widget("text","kami_world_status",y+282,width=465,height=47,wrap="true").text="Off. Draw distance and world detail unchanged."
widget("text","kami_camera_head",y+337,width=465,height=23,font="SansSerifLarge").text="KAMI CAMERA DIRECTOR  |  Real cinematic paths"
widget("button","kami_cam_capture_a",y+366,width=220,height=26,label="Capture camera A")
widget("button","kami_cam_capture_b",y+395,width=220,height=26,label="Capture camera B")
widget("button","kami_cam_play",y+426,width=220,height=26,label="Play smooth A → B")
widget("button","kami_cam_stop",y+456,width=220,height=26,label="Stop & restore camera")
widget("combo_box","kami_cam_duration",y+487,width=465,height=24,
 control_name="KAMICameraPathSeconds",
 tool_tip="Duration of the camera move from A to B. Uses native camera global positions, not prerecorded video.")
for label,value in [("2-second camera path","2"),("5-second path","5"),
                    ("8-second path","8"),("12-second path","12"),("20-second path","20")]:
 ET.SubElement(panel[-1],"combo_box.item",{"label":label,"value":value})
widget("check_box","kami_cam_track",y+517,width=465,height=23,
 control_name="KAMICameraTrackSelf",label="Follow and aim at my avatar during playback")
widget("text","kami_cam_status",y+542,width=465,height=49,wrap="true").text="Capture A and B, then Play. Follow/aim tracking uses smoothed avatar movement; unavailable in mouselook."
widget("text","kami_profile_head",y+595,width=465,height=24,font="SansSerifLarge").text="KAMI INTELLIGENT PROFILES  |  Region memory"
choices=widget("combo_box","kami_profile_choice",y+626,width=465,height=24,
 control_name="KAMIProfileSelected")
for name in ("Club","Photography","Exploration","Low-end PC"):
 ET.SubElement(choices,"combo_box.item",{"label":name,"value":name})
widget("button","kami_profile_load",y+658,width=220,height=26,label="Load selected profile")
widget("button","kami_profile_save",y+688,width=220,height=26,label="Save current graphics")
widget("button","kami_profile_restore",y+717,width=220,height=26,label="Restore manual graphics")
widget("button","kami_profile_remember",y+747,width=220,height=26,label="Remember for this region")
widget("check_box","kami_profile_auto",y+781,width=465,height=22,
 control_name="KAMIRegionProfilesEnabled",label="Automatically recall profiles when changing regions")
widget("text","kami_profile_status",y+812,width=465,height=47,wrap="true").text="No profile applied; normal graphics remain unchanged."
ET.indent(tree,space=" ");tree.write(p,encoding="utf-8",xml_declaration=True)

# Functional button handlers, no new floaters or overlapping settings. Existing
# KAMI hardware floater already registered; use it for actual telemetry.
pref=src/"llfloaterpreference.cpp";s=pref.read_text(encoding="utf-8-sig")
a='#include "llfloaterpreference.h"'
assert s.count(a)==1
s=s.replace(a,a+'\n#include "kami_signature.h"',1)
a='bool LLFloaterPreference::postBuild()\n{'
assert s.count(a)==1
callbacks=r'''bool LLFloaterPreference::postBuild()
{
    // KAMI signature controls reuse the same Fire Garden skinned Display panel.
    auto bindKAMI=[this](const char* name, auto operation) {
        if (auto* b=findChild<LLButton>(name))
            b->setClickedCallback([operation](LLUICtrl*,const LLSD&) { operation(); });
    };
    bindKAMI("kami_doctor_hardware",[] {
        LLFloaterReg::showInstance("kami_hardware_monitor");
    });
    bindKAMI("kami_doctor_fix",[] { KAMISignature::Doctor::apply(); });
    bindKAMI("kami_doctor_undo",[] { KAMISignature::Doctor::undo(); });
    bindKAMI("kami_cam_capture_a",[] {
        KAMISignature::Camera::capture("KAMICameraStart");
    });
    bindKAMI("kami_cam_capture_b",[] {
        KAMISignature::Camera::capture("KAMICameraEnd");
    });
    bindKAMI("kami_cam_play",[] { KAMISignature::Camera::play(); });
    bindKAMI("kami_cam_stop",[] { KAMISignature::Camera::stop(); });
    bindKAMI("kami_profile_load",[] {
        KAMISignature::Profiles::applyProfile(
            gSavedSettings.getString("KAMIProfileSelected"));
    });
    bindKAMI("kami_profile_save",[] {
        KAMISignature::Profiles::saveProfile(
            gSavedSettings.getString("KAMIProfileSelected"));
    });
    bindKAMI("kami_profile_restore",[] {
        KAMISignature::Profiles::restoreManual();
    });
    bindKAMI("kami_profile_remember",[] {
        KAMISignature::Profiles::rememberRegion(
            gSavedSettings.getString("KAMIProfileSelected"));
    });
'''
s=s.replace(a,callbacks,1)
a='void LLFloaterPreference::draw()\n{'
assert s.count(a)==1
displaystatus=r'''void LLFloaterPreference::draw()
{
    // Only update when a status actually changes; does not sample the GPU.
    static F64 lastKAMIStatusUI=0.;
    const F64 nowKAMI=KAMISignature::seconds();
    if(nowKAMI-lastKAMIStatusUI>=1.)
    {
        lastKAMIStatusUI=nowKAMI;
        auto kamiText=[this](const char* node,const char* setting) {
            if (auto* t=findChild<LLTextBox>(node))
                t->setText(LLStringExplicit(gSavedSettings.getString(setting)));
        };
        kamiText("kami_doctor_status","KAMIDoctorStatus");
        kamiText("kami_world_status","KAMIWorldStatus");
        kamiText("kami_cam_status","KAMICameraStatus");
        kamiText("kami_profile_status","KAMIProfileStatus");
    }
'''
s=s.replace(a,displaystatus,1)
pref.write_text(s,encoding="utf-8")

manifest=skin/"KAMI-SIGNATURE-FEATURES.txt"
manifest.write_text(
 "KAMI | Beyond Reality | signature features\n"
 "Render Doctor: measured FPS + optional CPU/GPU/VRAM monitor, single-fix undo.\n"
 "Smart World: scene-aware native impostors and reversible stage budgets.\n"
 "Camera Director: keyframe A/B, per-display-frame path and smooth avatar tracking.\n"
 "Intelligent Profiles: Club, Photography, Exploration, Low-end PC; save/restore, region recall.\n"
 "Controls inside Fire Garden Display; no Material Inspector or new GL hooks.\n",
 encoding="utf-8")
ET.parse(settings);ET.parse(p)
print("KAMI signature features: four actual native engines wired to Fire Garden Display; manual 4K/FPS caps preserved.")
