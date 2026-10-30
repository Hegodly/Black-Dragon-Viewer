#!/usr/bin/env python3
"""Add real, reversible KAMI smart avatar-physics scheduling and statistics.

All actual avatar physics remain in LLPhysicsMotion. Never touch server
movement/collision physics or transmit any changed body parameters. Native
breast/butt/belly LOD toggle retained. Optional client-only performance policy.
Run after graphics and Fire Garden UI patches, before final validators.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def edit(path,old,new):
 p=R/path
 s=p.read_text(encoding="utf-8-sig")
 assert s.count(old)==1,(path,"anchor",old[:110],s.count(old))
 p.write_text(s.replace(old,new,1),encoding="utf-8")
 return p

settings=R/"indra/newview/app_settings/settings_blackdragon.xml"
tree=ET.parse(settings)
root=tree.getroot().find("map")
assert root is not None
flags=[
 ("KAMIPhysicsSmart","Boolean","integer","0",1,"Opt-in adaptive client-only avatar physics scheduling; Off preserves original simulation."),
 ("KAMIPhysicsQuality","S32","integer","1",1,"Smart physics cadence: 0 FPS-first, 1 Balanced, 2 High, 3 Cinematic."),
 ("KAMIPhysicsProtectSelf","Boolean","integer","1",1,"Simulate own avatar every real rendered frame at full visual physics LOD."),
 ("KAMIPhysicsStabilizer","Boolean","integer","1",1,"Bound physics catch-up after stutters to avoid runaway spring-force steps."),
 ("KAMIPhysicsCullOffscreen","Boolean","integer","1",1,"Lower update frequency of invisible or off-screen remote avatars."),
 ("KAMIPhysicsMonitor","Boolean","integer","0",1,"Enable low-rate, in-world avatar-physics CPU and update monitoring."),
 ("KAMIPhysicsNearDistance","F32","real","20",1,"Meters around the camera for near-avatar physics quality."),
]
for name,typ,tag,value,persist,comment in flags:
 assert all(not(x.tag=="key" and x.text==name) for x in root)
 k=ET.Element("key");k.text=name
 m=ET.Element("map")
 for et,txt in [("key","Comment"),("string",comment),("key","Persist"),
                ("integer",str(persist)),("key","Type"),("string",typ),("key","Value"),(tag,value)]:
  n=ET.SubElement(m,et);n.text=txt
 root.insert(0,m);root.insert(0,k)
ET.indent(tree,space=" ")
tree.write(settings,encoding="utf-8",xml_declaration=True)

hdr="indra/newview/llphysicsmotion.h"
edit(hdr,
     '#include "llframetimer.h"',
     '#include "llframetimer.h"\n#include <string>')
edit(hdr,
     'class LLPhysicsMotion;\n',
     '''class LLPhysicsMotion;

// The Performance floater only asks for this snapshot once a second.
// Physics simulation does not write settings or allocate GPU resources.
namespace KAMIPhysicsStats { std::string summary(); }
''')
edit(hdr,
     '    LLCharacter*        mCharacter;',
     '''    LLCharacter*        mCharacter;
    F32 mLastKAMISimulationTime = 0.f; // Controller-local; never shared by avatars.''')

cpp="indra/newview/llphysicsmotion.cpp"
edit(cpp,
     '#include "llvoavatarself.h"',
     '''#include "llvoavatarself.h"
#include "llvoavatar.h"
#include "llagentcamera.h"
#include <chrono>
#include <set>
#include <sstream>
#include <iomanip>''')
# The original motion constructor left several spring-integrator state
# members uninitialized. Initialize them deterministically for all avatars;
# this avoids random spikes and costs nothing at runtime.
edit(cpp,
     '''                mVelocityJoint_local(0),
                mPositionLastUpdate_local(0)''',
     '''                mVelocityJoint_local(0),
                mAccelerationJoint_local(0),
                mVelocity_local(0),
                mPositionLastUpdate_local(0),
                mPosition_world(0.f,0.f,0.f)''')
edit(cpp,
     '#define MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION 0.f',
     '''#define MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION 0.f

// All counters are main-thread-only. No per-frame string creation, GPU query,
// telemetry, or background work. Monitor is OFF by default.
namespace KAMIPhysicsStats
{
    using Clock = std::chrono::steady_clock;
    struct Window
    {
        Clock::time_point since = Clock::now();
        std::set<const LLCharacter*> unique;
        U32 updates = 0, skipped = 0;
        U32 self_updates = 0, near_updates = 0;
        U32 far_updates = 0, offscreen_updates = 0;
        F64 cpu_ms = 0.;
    };
    static Window current;
    static std::string last = "Enable Physics monitoring in Preferences > Display.";

    void skip() { ++current.skipped; }
    void record(const LLCharacter* character, int tier, bool own, F64 elapsed_ms)
    {
        current.unique.insert(character);
        ++current.updates;
        current.cpu_ms += elapsed_ms;
        if (own) ++current.self_updates;
        else if (tier == 0) ++current.near_updates;
        else if (tier == 1) ++current.far_updates;
        else ++current.offscreen_updates;
    }
    std::string summary()
    {
        if (!gSavedSettings.getBOOL("KAMIPhysicsMonitor"))
        {
            current = Window{};
            return "Avatar physics monitoring OFF. Enable in Display > Smart Avatar Physics.";
        }
        const auto now = Clock::now();
        const F64 seconds = std::chrono::duration<F64>(now-current.since).count();
        if (seconds < 0.9 && last != "Enable Physics monitoring in Preferences > Display.")
            return last;
        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << "Avatar physics CPU: " << (seconds > 0 ? current.cpu_ms/seconds : 0.)
            << " ms/s | active avatars: " << current.unique.size()
            << "\\nUpdates/s: " << (seconds > 0 ? (U32)(current.updates/seconds) : 0)
            << " | skipped/s: " << (seconds > 0 ? (U32)(current.skipped/seconds) : 0)
            << " | self/near/far/hidden: "
            << current.self_updates << "/" << current.near_updates << "/"
            << current.far_updates << "/" << current.offscreen_updates;
        last = out.str();
        current = Window{};
        return last;
    }
}

namespace KAMIPhysicsPolicy
{
    // Quality controls only how frequently the *viewer* updates the client-side
    // parameter springs. Actual remote/server collisions are never modified.
    inline F32 updateRate(S32 quality, S32 tier)
    {
        static const F32 fps[4][3] = {
            {20.f,  8.f, 2.f},  // FPS first
            {30.f, 10.f, 3.f},  // Balanced
            {45.f, 20.f, 5.f},  // High
            {60.f, 30.f,10.f}   // Cinematic
        };
        return fps[llclamp(quality,0,3)][llclamp(tier,0,2)];
    }
}
''')
old='''    bool update_visuals = false;
    for (motion_vec_t::iterator iter = mMotions.begin();
            iter != mMotions.end();
            ++iter)
    {
            LLPhysicsMotion *motion = (*iter);
            update_visuals |= motion->onUpdate(time);
    }

    if (update_visuals)
            mCharacter->updateVisualParams();

    return true;'''
new='''    // Smart scheduling only when explicitly enabled. This is independent of
    // the user's original Enable Avatar Physics and its legacy quality combo.
    const bool smart = gSavedSettings.getBOOL("KAMIPhysicsSmart");
    const bool monitoring = gSavedSettings.getBOOL("KAMIPhysicsMonitor");
    // Keep the original OFF path lean: no per-avatar dynamic cast needed.
    LLVOAvatar* avatar = (smart || monitoring) ?
        dynamic_cast<LLVOAvatar*>(mCharacter) : nullptr;
    const bool own = avatar && (dynamic_cast<LLVOAvatarSelf*>(avatar) != nullptr);
    S32 tier = 0; // 0 near, 1 distant, 2 off-screen
    if (smart && avatar && !own)
    {
        const LLVector3 offset = avatar->getPositionAgent() -
            gAgentCamera.getCameraPositionAgent();
        const F32 near_distance = llclamp(
            gSavedSettings.getF32("KAMIPhysicsNearDistance"), 5.f, 64.f);
        tier = offset.magVecSquared() > near_distance*near_distance ? 1 : 0;
        if (gSavedSettings.getBOOL("KAMIPhysicsCullOffscreen") &&
            !avatar->isVisible())
            tier = 2;
    }
    if (smart && avatar && !(own && gSavedSettings.getBOOL("KAMIPhysicsProtectSelf")))
    {
        const F32 rate = KAMIPhysicsPolicy::updateRate(
            gSavedSettings.getS32("KAMIPhysicsQuality"),tier);
        if (mLastKAMISimulationTime > time)
            mLastKAMISimulationTime = 0.f; // avatar motion restarted
        if (mLastKAMISimulationTime > 0.f &&
            (time-mLastKAMISimulationTime) < 1.f / rate)
        {
            if (monitoring) KAMIPhysicsStats::skip();
            return true;
        }
    }
    // Clear throttle history immediately when Smart Mode is turned off.
    mLastKAMISimulationTime = smart ? time : 0.f;
    KAMIPhysicsStats::Clock::time_point start;
    if (monitoring) start = KAMIPhysicsStats::Clock::now();
    bool update_visuals = false;
    for (motion_vec_t::iterator iter = mMotions.begin();
            iter != mMotions.end();
            ++iter)
    {
            LLPhysicsMotion *motion = (*iter);
            update_visuals |= motion->onUpdate(time);
    }

    if (update_visuals)
            mCharacter->updateVisualParams();
    if (monitoring)
    {
        const F64 ms = std::chrono::duration<F64,std::milli>(
            KAMIPhysicsStats::Clock::now()-start).count();
        KAMIPhysicsStats::record(mCharacter,tier,own,ms);
    }

    return true;'''
edit(cpp,old,new)
edit(cpp,
     '''        const F32 time_delta = time - mLastTime;

    // If less than 1FPS, we don't want to be spending time updating physics at all.''',
     '''        // Bound catch-up after lag spikes or long periods of off-screen
        // throttling; protects springs from excessively large acceleration
        // and CPU-intensive substeps. Off preserves the original integrator.
        const bool stabilize = gSavedSettings.getBOOL("KAMIPhysicsSmart") &&
            gSavedSettings.getBOOL("KAMIPhysicsStabilizer");
        const F32 time_delta = stabilize ?
            llmin(time-mLastTime, 0.12f) : time-mLastTime;

    // If less than 1FPS, we don't want to be spending time updating physics at all.''')
edit(cpp,
     '''        const F32 lod_factor = LLVOAvatar::sPhysicsLODFactor;
        if (lod_factor == 0)
        {
                return true;
        }''',
     '''        // Self priority keeps local physics visually smooth even when the
        // global remote-avatar physics LOD is reduced by performance presets.
        F32 lod_factor = LLVOAvatar::sPhysicsLODFactor;
        if (gSavedSettings.getBOOL("KAMIPhysicsSmart") &&
            gSavedSettings.getBOOL("KAMIPhysicsProtectSelf") &&
            dynamic_cast<LLVOAvatarSelf*>(mCharacter) != nullptr)
            lod_factor = 1.f;
        if (lod_factor == 0)
        {
                mLastTime = time;
                return false;
        }''')
edit(cpp,
     '''        const F32 behavior_mass = getParamValue(MASS);''',
     '''        // Spring integrator divides by mass. Invalid/zero values could
        // produce NaN and violent motion. Stabilizer clamps only in smart mode.
        const F32 raw_mass = getParamValue(MASS);
        const F32 behavior_mass = stabilize ? llmax(raw_mass,0.001f) : raw_mass;''')
edit(cpp,
     '''    U32 steps = (U32)(time_delta / TIME_ITERATION_STEP_MAX) + 1;''',
     '''    U32 steps = (U32)(time_delta / TIME_ITERATION_STEP_MAX) + 1;
    if (stabilize) steps = llmin(steps, 4u); // bound CPU after 4K frame spikes''')

perf="indra/newview/llfloaterperformance.cpp"
edit(perf,
     '#include "llfloaterperformance.h"',
     '#include "llfloaterperformance.h"\n#include "llphysicsmotion.h"')
edit(perf,
     '''    mTextFPSLabel->setValue(fps_text);
}''',
     '''    mTextFPSLabel->setValue(fps_text);
    if (auto* physics = findChild<LLTextBox>("kami_physics_metrics"))
        physics->setText(LLStringExplicit(KAMIPhysicsStats::summary()));
}''')
pf=R/"indra/newview/skins/default/xui/en/floater_performance.xml"
tree=ET.parse(pf);root=tree.getroot()
main=[x for x in root.iter() if x.get("name")=="panel_performance_main"]
assert len(main)==1
p=main[0]
stats=ET.Element("panel",name="kami_physics_stats_panel",
    left="10",top="365",width="560",height="118",layout="topleft",
    background_visible="true",background_opaque="false",
    bg_alpha_color="0.04 0.08 0.13 0.90",border="true")
h=ET.SubElement(stats,"text",name="kami_physics_monitor_heading",left="12",top="9",
    width="535",height="22",font="SansSerifBoldLarge",text_color="0.55 0.77 0.95 1")
h.text="KAMI  |  SMART AVATAR PHYSICS"
m=ET.SubElement(stats,"text",name="kami_physics_metrics",left="12",top="37",
    width="535",height="65",wrap="true",font="SansSerif",text_color="0.87 0.93 0.98 1")
m.text="Avatar physics monitoring OFF. Enable in Display > Smart Avatar Physics."
p.append(stats)
root.set("title","KAMI | Performance & Avatar Physics")
ET.indent(tree,space=" ")
tree.write(pf,encoding="utf-8",xml_declaration=True)

ui=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(ui);root=tree.getroot()
ps=[x for x in root.iter() if x.tag=="layout_panel" and x.get("name")=="kombatant_display_quick"]
assert len(ps)==1
quick=ps[0]
oldheight=int(quick.get("height"));assert oldheight>=4500,oldheight
quick.set("height",str(oldheight+346))
# Other KAMI patches use exactly +4 for the parent vertical stack.
sized=[x for x in root.iter() if x.get("height")==str(oldheight+4)]
assert sized,("Expected vertical quick graphics scroll contents",oldheight)
for node in sized:node.set("height",str(oldheight+350))
def ctrl(tag,name,top,**attributes):
 attrs={k:str(v) for k,v in attributes.items()}
 x=ET.Element(tag,name=name,left="12",top=str(top),**attrs)
 quick.append(x);return x
y=oldheight+4
ctrl("text","kami_physics_heading",y,width="465",height="23",
    font="SansSerifLarge").text="Smart Avatar Physics — optional CPU optimizer"
ctrl("check_box","kami_physics_smart",y+29,width="465",height="23",
    control_name="KAMIPhysicsSmart",label="Adaptive avatar physics (reversible)",
    tool_tip="Off keeps the viewer's original per-frame avatar physics. On prioritizes your avatar, rate-limits distant avatars and reduces off-screen physics CPU work.")
quality=ctrl("combo_box","kami_physics_quality",y+58,width="465",height="24",
    control_name="KAMIPhysicsQuality",
    tool_tip="Controls client-side update frequencies of near/distant/off-screen physics. No GPU render resolution, frame limiter or network changes.")
for label,value in [("Low CPU — 20 / 8 / 2 Hz","0"),
                    ("Balanced — 30 / 10 / 3 Hz","1"),
                    ("High — 45 / 20 / 5 Hz","2"),
                    ("Cinematic — 60 / 30 / 10 Hz","3")]:
 ET.SubElement(quality,"combo_box.item",label=label,value=value)
ctrl("check_box","kami_physics_self",y+92,width="465",height="23",
    control_name="KAMIPhysicsProtectSelf",label="Self priority — full physics each rendered frame")
ctrl("check_box","kami_physics_stabilizer",y+120,width="465",height="23",
    control_name="KAMIPhysicsStabilizer",label="Physics stabilizer — reduce jitter after FPS drops")
ctrl("check_box","kami_physics_cull",y+148,width="465",height="23",
    control_name="KAMIPhysicsCullOffscreen",label="Off-screen avatar physics throttling")
near=ctrl("combo_box","kami_physics_distance",y+176,width="465",height="24",
    control_name="KAMIPhysicsNearDistance",
    tool_tip="Camera distance at which remote avatars use the near physics update rate.")
for label,value in [("Nearby radius: 10 m","10"),("Nearby radius: 20 m","20"),
                    ("Nearby radius: 32 m","32"),("Nearby radius: 48 m","48")]:
 ET.SubElement(near,"combo_box.item",label=label,value=value)
ctrl("check_box","kami_physics_monitor",y+208,width="465",height="23",
    control_name="KAMIPhysicsMonitor",label="Track physics CPU time / avatar updates (1-sec samples)")
ctrl("button","kami_physics_open_monitor",y+237,width="465",height="26",
    label="Open KAMI Performance / physics statistics")
ctrl("text","kami_physics_note",y+271,width="465",height="62",wrap="true").text=(
    "Keeps the original Enable Avatar Physics toggle and six-level physics LOD above. "
    "Smart Mode is opt-in, CPU-driven and does not affect server collisions or sim FPS. "
    "Real FPS savings depend on how many avatars are visible and your CPU bottleneck.")
ET.indent(tree,space=" ")
tree.write(ui,encoding="utf-8",xml_declaration=True)

prefs="indra/newview/llfloaterpreference.cpp"
edit(prefs,
     '''bool LLFloaterPreference::postBuild()
{''',
     '''bool LLFloaterPreference::postBuild()
{
    if (auto* show = findChild<LLButton>("kami_physics_open_monitor"))
        show->setClickedCallback([](LLUICtrl*, const LLSD&) {
            LLFloaterReg::showInstance("performance");
        });
''')

ET.parse(settings);ET.parse(pf);ET.parse(ui)
assert "KAMIPhysicsPolicy::updateRate" in (R/cpp).read_text()
print("KAMI: 6 native smart-avatar-physics features added, original physics controls retained, live CPU/update stats connected to Performance.")
