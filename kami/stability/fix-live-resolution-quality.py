#!/usr/bin/env python3
"""Stabilize KAMI live world render resolution without camera cropping.

Keeps native 4K as an explicit manual choice; warns against 8K for gameplay.
Does NOT enable the unstable internal-resolution downscaler: that path is
disabled until the full-viewport presentation is independently verified.
Adds a low-overhead actual-GPU-buffer size readout to the Display controls.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
src=R/"indra/newview/pipeline.cpp"
s=src.read_text(encoding="utf-8-sig")
old='''        GLint texture_limit = 0, renderbuffer_limit = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_limit);
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbuffer_limit);
        GLint viewport_limit[2] = {0, 0};
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport_limit);'''
new='''        // Driver capability queries can synchronize the GPU. The native
        // window-buffer size path is polled on resize; cache these
        // context-independent hardware limits instead of re-querying GL.
        static GLint texture_limit = 0, renderbuffer_limit = 0;
        static GLint viewport_limit[2] = {0, 0};
        if (texture_limit == 0 || renderbuffer_limit == 0)
        {
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_limit);
            glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbuffer_limit);
            glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport_limit);
        }'''
assert s.count(old)==1,"Live resolution GPU query code changed"
s=s.replace(old,new,1)
src.write_text(s,encoding="utf-8")

# The older Smart Auto Tune ladder still tries to lower the internal render
# resolution at stages 5 and 6. That causes cropped/shaking output on some
# GPUs even after Performance Mode itself was forced to native. Replace these
# unsafe optimizations with budget-only changes; keep explicit live 4K intact.
smart=R/"indra/newview/kami_smart_engine.h"
s=smart.read_text(encoding="utf-8-sig")
old='''            if (resolution && !gSavedSettings.getBOOL("KAMIPotatoMode"))
            {
                set("KombatantRenderWidth", 0);
                set("RenderResolutionDivisor", 1);
                atMostF32("RenderResolutionMultiplier", 0.75f);
            }'''
new='''            // Do not silently erase a manually chosen 4K/8K buffer or
            // downscale a live scene: unresolved presentation cropping.
            // Instead use fewer light/particle/avatar/shadow samples.
            if (resolution) atMostU32("RenderLocalLightCount", 32);'''
if s.count(old)==1:
    s=s.replace(old,new,1)
else:
    assert "Automatic world pixel scaling is quarantined" in s, (
        "Smart stage 5 resolution code changed unexpectedly")
    assert 'set("KombatantRenderWidth", 0);' not in s, (
        "Smart must preserve manually selected world resolution")
old='''            // Preserve real selected render pixels, including explicit 3840-wide 4K.
            // Avoid downscaling to 60% when Potato Mode is on.
            if (gSavedSettings.getBOOL("KAMIPotatoMode"))
            {
                set("RenderResolutionDivisor", 1);
                set("RenderResolutionMultiplier", 1.f);
                // 100 FPS is an advisory performance goal only.
                // Never modify the actual frame limiter here.
                set("KAMITargetFPS", 100);
            }
            else if (resolution) atMostF32("RenderResolutionMultiplier", 0.6f);'''
new='''            // Respect the exact current resolution (native/4K/8K) and
            // viewport. Never synthesize a new unvalidated world scale.
            if (gSavedSettings.getBOOL("KAMIPotatoMode"))
            {
                // 100 FPS is advisory only, never a frame limiter.
                set("KAMITargetFPS", 100);
            }'''
if s.count(old)==1:
    s=s.replace(old,new,1)
else:
    assert "No hidden resolution" in s or "No hidden resolution changes" in s or (
        "No hidden resolution" in s), "Smart stage 6 resolution code changed unexpectedly"
    assert 'else if (resolution) atMostF32("RenderResolutionMultiplier", 0.6f);' not in s
smart.write_text(s,encoding="utf-8")
# The Smart header has already been copied into compiled viewer by earlier
# integration steps. Refresh the final header after changing its logic.
(R/"indra/newview/kami_smart_engine.h").write_text(s,encoding="utf-8")

settings=R/"indra/newview/app_settings/settings_blackdragon.xml"
tree=ET.parse(settings)
root=tree.getroot().find("map")
assert root is not None
assert all(n.text!="KAMILiveResolutionStatus" for n in root.findall("key"))
key=ET.Element("key");key.text="KAMILiveResolutionStatus"
value=ET.Element("map")
for tag,txt in (
  ("key","Comment"),("string","Actual 3D GPU world-buffer resolution readout. Not a synthetic FPS claim."),
  ("key","Persist"),("integer","0"),("key","Type"),("string","String"),
  ("key","Value"),("string","Live world render buffer: initializing."),
):
 el=ET.SubElement(value,tag);el.text=txt
root.insert(0,value);root.insert(0,key)
ET.indent(tree,space=" ")
tree.write(settings,encoding="utf-8",xml_declaration=True)

# Runtime actual-vs-requested readout updates no more than once per 1.5 sec.
display=R/"indra/newview/llviewerdisplay.cpp"
s=display.read_text(encoding="utf-8-sig")
anchor='''        if (gSavedSettings.getString("KAMIFrameGenerationStatus") != actual_fg_status)
            gSavedSettings.setString("KAMIFrameGenerationStatus", actual_fg_status);'''
insert=anchor+'''
        // A label for the *allocated* world target, not a guessed mode.
        // Throttle settings writes to avoid UI signals on every rendered frame.
        static F32 last_world_resolution_status_time = 0.f;
        if (gFrameTimeSeconds < last_world_resolution_status_time ||
            gFrameTimeSeconds - last_world_resolution_status_time >= 1.5f)
        {
            last_world_resolution_status_time = gFrameTimeSeconds;
            const U32 gpu_width = gPipeline.mMainRT.screen.getWidth();
            const U32 gpu_height = gPipeline.mMainRT.screen.getHeight();
            const U32 requested = gSavedSettings.getU32("KombatantRenderWidth");
            std::string resolution_status = llformat(
                "Actual world buffer: %u x %u. ", gpu_width, gpu_height);
            if (requested == 3840 && gpu_width != 3840)
                resolution_status += "4K requested, but the live GPU target differs.";
            else if (requested == 7680 && gpu_width != 7680)
                resolution_status += "8K requested, but the live GPU target differs.";
            else if (requested == 7680)
                resolution_status += "8K costs 4x as many pixels as 4K; photo use only.";
            else if (requested == 3840)
                resolution_status += "4K native pixels; quality presets control GPU work.";
            else
                resolution_status += "Window-native world rendering.";
            if (gSavedSettings.getString("KAMILiveResolutionStatus") != resolution_status)
                gSavedSettings.setString("KAMILiveResolutionStatus", resolution_status);
        }'''
assert s.count(anchor)==1,"Optical-flow final-frame status anchor changed"
s=s.replace(anchor,insert,1)
display.write_text(s,encoding="utf-8")

ui=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(ui)
root=tree.getroot()
def one(name):
 nodes=[n for n in root.iter() if n.get("name")==name]
 assert len(nodes)==1,(name,len(nodes))
 return nodes[0]
# Reuse an existing note to avoid introducing overlapping controls.
note=one("kd_resolution_note")
note.set("control_name","KAMILiveResolutionStatus")
note.set("wrap","true")
note.set("tool_tip","Shows the real allocated world render target, so you can tell whether live native 4K or 8K is actually being used. The monitor's window size may be different. Internal downscaling is disabled pending cropping fix.")
note.text="Actual world buffer: initializing."
resolution=one("kd_resolution")
assert resolution.get("control_name")=="KombatantRenderWidth"
items=[node for node in resolution if node.tag=="combo_box.item"]
values={node.get("value") for node in items}
assert {"0","3840","7680"}.issubset(values),values
for item in items:
 if item.get("value")=="0":item.set("label","Window native — best FPS / normal pixels")
 elif item.get("value")=="3840":item.set("label","4K live — 3840 wide (quality + GPU cost)")
 elif item.get("value")=="7680":item.set("label","8K live — photo use (4x 4K pixels)")
resolution.set("tool_tip","Select the native world framebuffer width, independent of the interface. 4K is 3840 pixels wide; 8K requires very high VRAM and is not an FPS feature. No 12K live. Rendering above window size reduces FPS; use Low/Medium/Performance quality presets at 4K for lower GPU costs.")
for name in ("kami_adaptive_resolution","kami_resolution"):
 try: widget=one(name)
 except AssertionError: continue
 widget.set("enabled","false")
 widget.set("tool_tip","Temporarily disabled to prevent cropped/shaking camera output when reduced world framebuffer dimensions do not match the viewport. Quality presets safely optimize geometry, avatars and lighting at native or explicit 4K resolution.")
 widget.set("label","Adaptive world pixel scaling (disabled until safe)")
ET.indent(tree,space=" ")
tree.write(ui,encoding="utf-8",xml_declaration=True)
ET.parse(ui);ET.parse(settings)
print("Live native/4K/8K controls audited; actual GPU buffer readout and static driver limits integrated; unsafe Auto Tune downscaling removed.")
