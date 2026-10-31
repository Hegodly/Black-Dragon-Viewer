#!/usr/bin/env python3
"""KAMI: fix avatar physics visibility diagnostics after all physics integrations.

Do not fake physics by moving unrigged attachments. Native body physics
requires a Physics wearable and visual parameters on the avatar. Fitted mesh
rigged to BELLY / BUTT / LEFT_PEC / RIGHT_PEC collision bones can follow the
parameters; unrigged attachments do not inherit those deformations.

The current monitor counts simulated avatars as "active" even when no physics
layer or body movement is present. Correct that in the actual viewer and
leave manual physics/LOD controls reversible.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
CPP=R/"indra/newview/llphysicsmotion.cpp"
UI=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"

def replace(path,old,new):
    s=path.read_text(encoding="utf-8-sig")
    n=s.count(old)
    if n!=1:raise RuntimeError(f"{path.name}: expected one anchor, got {n}: {old[:96]!r}")
    path.write_text(s.replace(old,new,1),encoding="utf-8")

# The original "Protect My Avatar" control only protected self when
# Smart Physics was ON, while graphics quality presets could lower self
# spring visual update thresholds. Respect the explicit master OFF/LOD zero,
# but ensure "protect self" does what the UI promises at nonzero LOD.
replace(CPP,
'''if (gSavedSettings.getBOOL("KAMIPhysicsSmart") &&
            gSavedSettings.getBOOL("KAMIPhysicsProtectSelf") &&
            dynamic_cast<LLVOAvatarSelf*>(mCharacter) != nullptr)
            lod_factor = 1.f;''',
'''if (lod_factor > 0.f &&
            gSavedSettings.getBOOL("KAMIPhysicsProtectSelf") &&
            dynamic_cast<LLVOAvatarSelf*>(mCharacter) != nullptr)
            lod_factor = 1.f;''')

# Display the real local wearable state from the existing native outfit
# system, not guessed from the presence of animation code or object render.
replace(CPP, '#include "llagentcamera.h"',
        '#include "llagentcamera.h"\n#include "llagentwearables.h"')

# Protected getParamValue(MAX_EFFECT) is the actual spring effect intensity.
# This is only queried when user explicitly enables the physics monitor.
replace(CPP,
'''        LLPointer<LLJointState> getJointState()
        {
                return mJointState;
        }
protected:''',
'''        LLPointer<LLJointState> getJointState()
        {
                return mJointState;
        }

        bool hasConfiguredEffect()
        {
            return mParamDriver != nullptr &&
                   getParamValue(MAX_EFFECT) > 0.0001f;
        }
protected:''')

replace(CPP,
'''        U32 updates = 0, skipped = 0;
        U32 self_updates = 0, near_updates = 0;''',
'''        U32 updates = 0, skipped = 0;
        U32 self_effect_channels = 0, self_visual_updates = 0;
        U32 self_updates = 0, near_updates = 0;''')
replace(CPP,
'''    void skip() { ++current.skipped; }
    void record(''',
'''    void skip() { ++current.skipped; }
    void recordSelfEffects(U32 configured_channels, bool visual_changed)
    {
        current.self_effect_channels = configured_channels;
        if (visual_changed)
            ++current.self_visual_updates;
    }
    void record(''')

replace(CPP,
'''    std::string summary()
    {
        if (!gSavedSettings.getBOOL("KAMIPhysicsMonitor"))
        {
            current = Window{};
            return "Avatar physics monitoring OFF. Enable in Display > Smart Avatar Physics.";
        }
        const auto now = Clock::now();''',
'''    std::string summary()
    {
        // Read from the *actual* viewer settings and worn physics layer.
        // Do not claim that sim ticks imply an active/worn physics asset.
        const bool viewer_on = gSavedSettings.getBOOL("AvatarPhysics");
        const F32 physics_lod =
            gSavedSettings.getF32("RenderAvatarPhysicsLODFactor");
        const U32 worn_layers =
            gAgentWearables.getWearableCount(LLWearableType::WT_PHYSICS);
        std::string status;
        if (!viewer_on)
            status = "Physics OFF in viewer settings";
        else if (physics_lod <= 0.0001f)
            status = "Physics quality 0 (no visible physics)";
        else if (!worn_layers)
            status = "NO PHYSICS LAYER WORN (Appearance > New Physics)";
        else
            status = "Physics layer worn; fitted mesh collision-bone rigging required";

        if (!gSavedSettings.getBOOL("KAMIPhysicsMonitor"))
        {
            current = Window{};
            return status + ". Turn on physics monitoring for motion diagnostics.";
        }
        const auto now = Clock::now();''')

replace(CPP,
'''        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << "Avatar physics CPU: "''',
'''        std::ostringstream out;
        out << status << "\\n";
        out << std::fixed << std::setprecision(2)
            << "Avatar physics CPU: "''')

replace(CPP,
'''            << current.far_updates << "/" << current.offscreen_updates;
        last = out.str();''',
'''            << current.far_updates << "/" << current.offscreen_updates
            << "\\nSelf spring channels: " << current.self_effect_channels
            << " | visual param updates: " << current.self_visual_updates;
        if (viewer_on && physics_lod > 0.0001f && worn_layers &&
            current.self_updates > 0 && current.self_effect_channels == 0)
            out << " | No nonzero MaxEffect";
        else if (viewer_on && physics_lod > 0.0001f && worn_layers &&
                 current.self_updates > 0 && current.self_visual_updates == 0)
            out << " | Move avatar / inspect physics wearable";
        last = out.str();''')

replace(CPP,
'''    if (update_visuals)
            mCharacter->updateVisualParams();
    if (monitoring)
    {''',
'''    if (update_visuals)
            mCharacter->updateVisualParams();
    if (monitoring && own)
    {
        U32 configured_effects = 0;
        for (LLPhysicsMotion* motion : mMotions)
            if (motion && motion->hasConfiguredEffect())
                ++configured_effects;
        KAMIPhysicsStats::recordSelfEffects(configured_effects, update_visuals);
    }
    if (monitoring)
    {''')

# Make the core enable checkbox actionable and distinguish physics monitoring
# from true garment physics. Existing control names/callbacks remain untouched.
tree=ET.parse(UI)
root=tree.getroot()
native=[e for e in root.iter("check_box") if e.get("control_name")=="AvatarPhysics"]
if len(native)!=1: raise RuntimeError("Missing actual AvatarPhysics switch")
native[0].set("label","Enable avatar body physics movement")
native[0].set("tool_tip",
    "Viewer display of Second Life breast/belly/butt physics. You must WEAR a "
    "Physics clothing layer for your own physics to move. For fitted mesh "
    "bodies the weighted collision bones must support the effect. "
    "This does not enable scripted mesh or simulator object physics.")

monitor=[e for e in root.iter("text") if e.get("name")=="kami_physics_note"]
if len(monitor)!=1: raise RuntimeError("Missing KAMI physics help text")
monitor[0].text=(
    "Physics visibility requires a worn Physics clothing layer and compatible "
    "fitted-mesh rigging. The Performance window reports worn-layer state, "
    "effect channels and REAL visual-parameter changes. Smart Physics only "
    "optimizes the animation; it does not create missing physics.")
monitor[0].set("height","82")
link=[e for e in root.iter("button") if e.get("name")=="kami_physics_open_monitor"]
if len(link)!=1: raise RuntimeError("Missing native performance/physics shortcut")
link[0].set("label","Open KAMI Physics Status / Diagnostics")
ET.indent(tree,space=" ")
tree.write(UI,encoding="utf-8",xml_declaration=True)
ET.parse(UI)

# Expand the existing real Performance stats text in its existing panel.
performance=R/"indra/newview/skins/default/xui/en/floater_performance.xml"
tree=ET.parse(performance)
root=tree.getroot()
boxes=[e for e in root.iter("text") if e.get("name")=="kami_physics_metrics"]
if len(boxes)!=1:raise RuntimeError("Performance physics stats not installed")
boxes[0].set("height","101")
boxes[0].set("width","540")
boxes[0].text="Physics status checks the actual wearable and simulated visual changes."
panel=[e for e in root.iter("panel") if e.get("name")=="kami_physics_stats_panel"]
if len(panel)!=1:raise RuntimeError("KAMI Performance physics box missing")
panel[0].set("height","158")
ET.indent(tree,space=" ")
tree.write(performance,encoding="utf-8",xml_declaration=True)
ET.parse(performance)

print("PASS: Native KAMI physics detection reports wearable/LOD/master switch, "
      "effective spring channels and actual visual updates; no fake motion.")
