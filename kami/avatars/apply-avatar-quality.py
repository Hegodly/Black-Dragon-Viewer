#!/usr/bin/env python3
"""Add opt-in live Avatar Quality + FPS LOD policies without new render passes.

High-detail self avatar, distance-weighted remote avatar LOD. No texture
downloads, impostor snapshots, GL buffer resets or work on every toggle.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
view=R/"indra/newview"
cpp=view/"llvoavatar.cpp"
s=cpp.read_text(encoding="utf-8-sig")
anchor='''        // now select meshes to render based on adjusted pixel area
        LLViewerJoint* root = dynamic_cast<LLViewerJoint*>(mRoot);'''
assert s.count(anchor)==1, "Avatar native joint LOD anchor changed"
replacement='''        // KAMI Avatar Quality + FPS: use the existing avatar joint LOD
        // selection, not a second GPU render pass or texture reload.
        // The cached control reacts while the viewer is rendering; OFF is
        // mathematically identical to the upstream LOD behavior.
        static LLCachedControl<bool> kami_avatar_quality_fps(
            gSavedSettings, "KAMIAvatarQualityFPS", false);
        if (kami_avatar_quality_fps)
        {
            if (isSelf())
            {
                // Keep the player's native head/body joints detailed, even
                // when a crowded world uses distant-avatar optimizations.
                // This does not upscale creator-authored texture assets.
                mAdjustedPixelArea = llmax(mAdjustedPixelArea, 262144.f);
            }
            else if (!mIsDummy && mDrawable.notNull())
            {
                // Reduce geometry only on distant OTHER avatars. The gradual
                // factor avoids an abrupt detail step when crossing a radius.
                const F32 distance = llmax(0.f, mDrawable->mDistanceWRTCamera);
                const F32 factor = llclamp(
                    1.f - llmax(0.f, distance - 18.f)*0.009f,
                    0.65f, 1.f);
                mAdjustedPixelArea *= factor;
            }
        }

        // now select meshes to render based on adjusted pixel area
        LLViewerJoint* root = dynamic_cast<LLViewerJoint*>(mRoot);'''
s=s.replace(anchor,replacement,1)
assert s.count("KAMIAvatarQualityFPS")==1
cpp.write_text(s,encoding="utf-8")

# Use the native avatar impostor budget for an additional crowd FPS saving.
# Lower limits chosen elsewhere (Potato/Performance) are never increased.
budget_anchor='''\t// the sLimitNonImpostors flag depends on whether or not sMaxNonImpostors is set to the no-limit value (0)
\tsLimitNonImpostors = (0 != sMaxNonImpostors);'''
budget_new='''    // KAMI Avatar Quality + FPS: retain the existing native impostor
    // engine; only cap the count of detailed avatars in crowds.
    if (gSavedSettings.getBOOL("KAMIAvatarQualityFPS") &&
        (sMaxNonImpostors == 0 || sMaxNonImpostors > 8))
    {
        sMaxNonImpostors = 8;
    }
\t// the sLimitNonImpostors flag depends on whether or not sMaxNonImpostors is set to the no-limit value (0)
\tsLimitNonImpostors = (0 != sMaxNonImpostors);'''
avatar_code=cpp.read_text(encoding="utf-8")
assert avatar_code.count(budget_anchor)==1, "Native impostor budget anchor changed"
avatar_code=avatar_code.replace(budget_anchor,budget_new,1)
cpp.write_text(avatar_code,encoding="utf-8")

# Refresh the budget when the toggle changes; never reupload textures.
viewer=view/"llviewercontrol.cpp"
control=viewer.read_text(encoding="utf-8-sig")
loc='''static bool handleAvatarPhysicsLODChanged(const LLSD& newvalue)'''
assert control.count(loc)==1
control=control.replace(loc,'''static bool handleKAMIAvatarQualityChanged(const LLSD&)
{
    // The existing viewer updates the impostor count immediately.
    // Per-avatar LOD reads the same toggle during normal live rendering.
    LLVOAvatar::updateImpostorRendering(
        gSavedSettings.getU32("RenderAvatarMaxNonImpostors"));
    return true;
}

'''+loc,1)
hook='''    setting_setup_signal_listener(gSavedSettings, "RenderAvatarLODFactor", handleAvatarLODChanged);'''
assert control.count(hook)==1
control=control.replace(hook,hook+'''
    setting_setup_signal_listener(gSavedSettings, "KAMIAvatarQualityFPS", handleKAMIAvatarQualityChanged);''',1)
viewer.write_text(control,encoding="utf-8")

# Persist only the opt-in choice; do not change avatar appearance upload,
# textures, simulator physics or user-selected world framebuffer resolution.
settings=view/"app_settings/settings_blackdragon.xml"
tree=ET.parse(settings)
mapnode=tree.getroot().find("map")
assert mapnode is not None
assert not any(n.tag=="key" and n.text=="KAMIAvatarQualityFPS" for n in mapnode)
key=ET.Element("key");key.text="KAMIAvatarQualityFPS"
entry=ET.Element("map")
for tag,value in (
    ("key","Comment"),
    ("string","Opt-in live avatar quality/FPS: protect self joint LOD; reduce distant other-avatar joint LOD. Never changes world resolution, textures, frame cap or simulator state."),
    ("key","Persist"),("integer","1"),
    ("key","Type"),("string","Boolean"),
    ("key","Value"),("integer","0"),
):
    el=ET.SubElement(entry,tag);el.text=value
mapnode.insert(0,entry);mapnode.insert(0,key)
ET.indent(tree,space=" ")
tree.write(settings,encoding="utf-8",xml_declaration=True)

ui=view/"skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(ui);root=tree.getroot()
panels=[n for n in root.iter("layout_panel")
        if n.get("name")=="kombatant_display_quick"]
assert len(panels)==1
quick=panels[0]
assert any(n.get("name")=="kami_recovery_status" for n in quick), (
    "Avatar Quality must be installed after Session Recovery, with scroll bounds updated")
oldheight=int(quick.get("height"))
delta=119
quick.set("height",str(oldheight+delta))
newwidgets=(
    ("text","kami_avatar_quality_heading",{"top":oldheight+8,"height":24,"font":"SansSerifLarge",
        "text":"KAMI AVATAR QUALITY  |  Live detail + FPS"}),
    ("check_box","kami_avatar_quality_toggle",{"top":oldheight+42,"height":23,
        "label":"Optimize avatar quality + FPS (live)",
        "control_name":"KAMIAvatarQualityFPS",
        "tool_tip":"Keep my avatar's native joint mesh detailed. Gradually reduce distant OTHER avatars' joint LOD and cap full-rendered avatars to 8 (or your lower limit). Updates while in-world; no relog needed. Existing mesh attachment/texture limits still apply."}),
    ("text","kami_avatar_quality_note",{"top":oldheight+73,"height":41,"wrap":"true",
        "text":"Self avatar stays detailed; distant avatars are lighter. No new GPU pass, asset re-download, frame cap or render-scale change. Turn off to restore native avatar LOD."}),
)
for tag,name,attributes in newwidgets:
    attrs={"name":name,"left":"12","width":"465"}
    for k,v in attributes.items():
        if k!="text": attrs[k]=str(v)
    n=ET.SubElement(quick,tag,attrs)
    if "text" in attributes:n.text=attributes["text"]

# Scroll content must include both the full quick section and the remaining
# native Black Dragon Basic/Advanced/Post sections below.
scroll=[n for n in root.iter("panel") if n.get("name")=="gfx_scroll_panel"]
stack=[n for n in root.iter("layout_stack") if n.get("name")=="gfx_stack"]
assert len(scroll)==len(stack)==1
scroll[0].set("height",str(int(quick.get("height"))+66))
children=[n for n in stack[0] if n.tag=="layout_panel"]
assert quick in children
stack_min=sum(int(n.get("height","0")) for n in children)+96
stack[0].set("height",str(max(int(stack[0].get("height","0")),stack_min)))
assert max(int(n.get("top"))+int(n.get("height"))
           for n in quick if n.get("top") and n.get("height") and
           n.get("top").lstrip("-").isdigit())<=int(quick.get("height"))
ET.indent(tree,space=" ")
tree.write(ui,encoding="utf-8",xml_declaration=True)
ET.parse(ui);ET.parse(settings)
print("KAMI live Avatar Quality + FPS installed: opt-in self detail and distant other-avatar LOD, reversible without render-scale changes.")
