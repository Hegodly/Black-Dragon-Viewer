#!/usr/bin/env python3
"""Actual low-cost adaptive avatar body physics in KAMI.

The upstream Motion controller updates *all six* body springs per frame,
including distant avatars whose physics animation is visually insignificant.
This patch throttles only non-self avatars and keeps the physics spring
integrator's elapsed time intact on the next step. User's own avatar is
NEVER throttled; normal "AvatarPhysics" switch remains functional.
No scripted attachment/Havok/simulator physics changes.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
cpp=R/"indra/newview/llphysicsmotion.cpp"
s=cpp.read_text(encoding="utf-8-sig")
old='''    bool update_visuals = false;
    for (motion_vec_t::iterator iter = mMotions.begin();'''
new='''    // KAMI: Keep self-avatar physics fully responsive. Reduce the CPU cost
    // of the existing six spring simulations on visually small remote
    // avatars. Each spring continues with the actual elapsed dt when run,
    // and the upstream solver already substeps dt for stability.
    static LLCachedControl<bool> adaptive_physics(gSavedSettings,
                                                  "KAMIAvatarPhysicsAdaptive", false);
    // Smart mode owns update cadence when enabled; never stack both limiters.
    if (adaptive_physics && !gSavedSettings.getBOOL("KAMIPhysicsSmart") &&
        dynamic_cast<LLVOAvatarSelf*>(mCharacter) == nullptr)
    {
        // Compare squared pixel thresholds, avoiding per-avatar sqrtf work.
        const F32 area = llmax(0.f, mCharacter->getPixelArea());
        static LLCachedControl<F32> near_pixels(gSavedSettings,
                                                 "KAMIAvatarPhysicsNearPixels", 70.f);
        static LLCachedControl<F32> distant_pixels(gSavedSettings,
                                                    "KAMIAvatarPhysicsDistantPixels", 28.f);
        static LLCachedControl<S32> medium_rate(gSavedSettings,
                                                 "KAMIAvatarPhysicsMidHz", 18);
        static LLCachedControl<S32> far_rate(gSavedSettings,
                                              "KAMIAvatarPhysicsFarHz", 6);
        const F32 interval =
            area >= (F32)near_pixels * (F32)near_pixels ? 0.f :
            (area >= (F32)distant_pixels * (F32)distant_pixels ?
               1.f / llmax(1, (S32)medium_rate) :
               1.f / llmax(1, (S32)far_rate));
        if (interval > 0.f && mKAMILastPhysicsUpdate > 0.f &&
            time >= mKAMILastPhysicsUpdate &&
            (time - mKAMILastPhysicsUpdate) < interval)
        {
            return true;
        }
    }
    mKAMILastPhysicsUpdate = time;

    bool update_visuals = false;
    for (motion_vec_t::iterator iter = mMotions.begin();'''
assert s.count(old)==1,"original physics controller update anchor changed"
s=s.replace(old,new,1)
cpp.write_text(s,encoding="utf-8")

header=R/"indra/newview/llphysicsmotion.h"
s=header.read_text(encoding="utf-8-sig")
old='''    LLCharacter*        mCharacter;

    typedef std::vector<LLPhysicsMotion *> motion_vec_t;'''
new='''    LLCharacter*        mCharacter;
    // Only controls how often remote springs are evaluated. Self is never
    // throttled; the original six springs, blending and simulation remain.
    F32 mKAMILastPhysicsUpdate = 0.f;

    typedef std::vector<LLPhysicsMotion *> motion_vec_t;'''
assert s.count(old)==1,"original physics header anchor changed"
s=s.replace(old,new,1)
header.write_text(s,encoding="utf-8")

settings=R/"indra/newview/app_settings/settings_blackdragon.xml"
doc=ET.parse(settings);root=doc.getroot().find("map")
assert root is not None
specs=[
 ("KAMIAvatarPhysicsAdaptive","Boolean","integer","0","Optimize remote avatar body physics at lower rates as they become visually smaller; keep self full rate."),
 ("KAMIAvatarPhysicsNearPixels","F32","real","70","At this pixel-square-root area or greater remote avatars run full physics."),
 ("KAMIAvatarPhysicsDistantPixels","F32","real","28","Below this size remote avatars use the far physics update rate."),
 ("KAMIAvatarPhysicsMidHz","S32","integer","18","Remote avatar physics update rate for middle sized avatars, independent of FPS cap."),
 ("KAMIAvatarPhysicsFarHz","S32","integer","6","Remote avatar physics update rate for distant/small avatars, independent of FPS cap."),
]
for key,ty,tag,val,info in specs:
    assert all(el.text!=key for el in root.findall("key")),("duplicate physics setting",key)
    k=ET.Element("key");k.text=key
    v=ET.Element("map")
    for e,t in [("key","Comment"),("string",info),("key","Persist"),("integer","1"),
                ("key","Type"),("string",ty),("key","Value"),(tag,val)]:
        item=ET.SubElement(v,e);item.text=t
    root.insert(0,v);root.insert(0,k)
ET.indent(doc,space=" ")
doc.write(settings,encoding="utf-8",xml_declaration=True)

ui=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
doc=ET.parse(ui);root=doc.getroot()
# Keep the ORIGINAL avatar physics enable toggle and LOD dropdown unchanged.
matches=[node for node in root.iter() if node.get("name")=="AvatarPhysics"]
assert len(matches)==1
orig=matches[0]
assert orig.get("control_name")=="AvatarPhysics"
# Native panel is vertical auto layout. Insert next to the existing
# physics control instead of adding a separate lab/studio floater.
parent=None
for p in root.iter():
    if orig in list(p):parent=p;break
assert parent is not None
where=list(parent).index(orig)
checkbox=ET.Element("check_box",{
 "name":"kami_adaptive_avatar_physics",
 "control_name":"KAMIAvatarPhysicsAdaptive",
 "enabled_control":"AvatarPhysics",
 "label":"Optimize distant avatars (optional; own physics stays full speed)",
 "height":"17","layout":"topleft","left":"10","top_pad":"8","width":"450",
 "tool_tip":"Throttles calculations only on distant/small OTHER avatars. Your avatar remains full physics. Does not affect scripted mesh/hair physics or simulator dynamics. Optional; compare FPS and motion smoothness."
})
# We insert after the existing LOD combo to avoid disrupting its previous
# top-delta positioning. Its surrounding controls are siblings.
lod=[n for n in list(parent) if n.get("control_name")=="RenderAvatarPhysicsLODFactor" and n.tag=="combo_box"]
assert len(lod)==1
parent.insert(list(parent).index(lod[0])+1,checkbox)
note=ET.Element("text",{"name":"kami_avatar_physics_note","width":"445","height":"30",
                        "layout":"topleft","left":"10","top_pad":"3","wrap":"true"})
note.text=("Self stays full-rate. Nearby avatars stay smooth; distant avatar "
           "body physics updates at reduced rates to save CPU.")
parent.insert(list(parent).index(checkbox)+1,note)
ET.indent(doc,space=" ")
doc.write(ui,encoding="utf-8",xml_declaration=True)
ET.parse(settings);ET.parse(ui)

# Physics visual update fidelity must not be lowered by FPS or graphics
# presets. The native AvatarPhysics and RenderAvatarPhysicsLODFactor remain
# independently controllable by the resident. KAMI performance optimization
# applies to *explicitly opt-in* distant-avatar scheduling only.

assert 'KAMIAvatarPhysicsAdaptive' in cpp.read_text(encoding="utf-8")
print("Optional adaptive avatar physics installed; FPS presets do NOT override visible physics LOD. Own avatar retains original motion fidelity.")
