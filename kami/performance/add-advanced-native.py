#!/usr/bin/env python3
"""Native KAMI lighting controls plus opt-in adaptive 3D resolution.

Requires the baseline patches and apply-native-performance.py first.
Only uses existing Black Dragon / LL GPU shaders. SSGI and skin SSS are not
claimed as implemented: both need dedicated renderer passes in a later patch.
"""
from pathlib import Path
import re

R = Path(__file__).resolve().parents[2]

def edit(path, old, new):
    p = R / path
    s = p.read_text(encoding="utf-8-sig")
    if s.count(old) != 1:
        raise RuntimeError(f"{path}: expected one exact anchor {old[:65]!r}, got {s.count(old)}")
    p.write_text(s.replace(old, new, 1), encoding="utf-8")
    print("Updated:", path)

# Persist the mode preferences. Automatic scaling applies only in Performance Mode.
settings = "indra/newview/app_settings/settings_blackdragon.xml"
defs = '''  <key>KAMIDynamicResolution</key>
  <map><key>Comment</key><string>Adapt 3D render scale while KAMI Performance Mode is active. Never scales UI or screenshots.</string><key>Persist</key><integer>1</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>1</integer></map>
  <key>KAMITargetFPS</key>
  <map><key>Comment</key><string>Desired rendered FPS for KAMI adaptive resolution: 30-120. This is not a frame limiter.</string><key>Persist</key><integer>1</integer><key>Type</key><string>U32</string><key>Value</key><integer>60</integer></map>
'''
edit(settings, '  <key>KAMIPerformanceMode</key>', defs + '  <key>KAMIPerformanceMode</key>')

# Make existing (already working) native renderer features accessible together.
xui = "indra/newview/skins/default/xui/en/floater_kombatant_effects.xml"
p = R / xui
s = p.read_text(encoding="utf-8-sig")
a = ' <text name="native_note" left="24" top="456" width="512" height="70" wrap="true">These effects run inside the viewer\\'s native renderer. No external ReShade or GShade runtime. Performance Mode is in Preferences / Display.</text>'
if s.count(a) != 1:
    raise RuntimeError("Native effects floater changed; no safe insertion point")
s = s.replace(a, ''' <text name="native_note" left="24" top="456" width="512" height="70" wrap="true">Native graphics, no external shader injector. Performance Mode is in Preferences / Display. Experimental SSGI and skin SSS are not yet implemented.</text>
 <text name="detail_heading" left="24" top="536" width="512" height="24" font="SansSerifLarge">Existing advanced renderer quality</text>
 <check_box name="kami_hbao" label="Higher-detail HBAO (ambient contact detail)" control_name="RenderDeferredHBAO" left="24" top="566" width="500" height="22" />
 <check_box name="kami_hq_dof" label="High-quality depth of field (slow)" control_name="RenderDepthOfFieldHighQuality" left="24" top="598" width="500" height="22" />
 <combo_box name="kami_probes" control_name="RenderReflectionProbeDetail" left="24" top="634" width="260" height="24">
  <combo_box.item label="Reflection probes: Static" value="0" />
  <combo_box.item label="Reflection probes: Dynamic" value="1" />
  <combo_box.item label="Reflection probes: Realtime (heavy)" value="2" />
 </combo_box>
 <combo_box name="kami_shadows" control_name="RenderShadowDetail" left="290" top="634" width="245" height="24">
  <combo_box.item label="Shadows: Off" value="0" />
  <combo_box.item label="Shadows: Low" value="1" />
  <combo_box.item label="Shadows: High" value="2" />
 </combo_box>
 <text name="kami_gfx_warning" left="24" top="668" width="512" height="46" wrap="true">HBAO requires ambient occlusion enabled. Realtime reflection probes and HQ DoF are expensive; disable them for gameplay FPS.</text>''', 1)
s = s.replace('height="550"','height="740"',1)
p.write_text(s, encoding="utf-8")

# Add actual adaptive scaling controls beside the Performance Mode switch.
ui = "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
p = R / ui
s = p.read_text(encoding="utf-8-sig")
a = s.index('<layout_panel name="kombatant_display_quick"')
b = s.index('</layout_panel>', a) + len('</layout_panel>')
panel = s[a:b]
if 'height="1684" auto_resize="false"' not in panel:
    raise RuntimeError("Performance Mode layout needs refresh")
panel = panel.replace('height="1684" auto_resize="false"', 'height="1760" auto_resize="false"', 1)
# Existing controls begin at 100, after the existing new performance header.
panel = re.sub(r'\btop="(\d+)"',
               lambda m: f'top="{int(m.group(1))+76}"' if int(m.group(1))>=100 else m.group(0),
               panel)
anchor = '<text name="kami_performance_note" left="12" top="66" width="465" height="22">One-click mode; turn off to restore previous graphics.</text>'
if panel.count(anchor)!=1:
    raise RuntimeError("Performance panel header not found")
panel = panel.replace(anchor,anchor+'''
       <check_box name="kami_adaptive_resolution" label="Auto-adjust world resolution for steadier FPS" control_name="KAMIDynamicResolution" left="12" top="97" width="465" height="22" />
       <text name="kami_target_label" left="12" top="130" width="180" height="20">Target frame rate</text>
       <combo_box name="kami_target_fps" control_name="KAMITargetFPS" left="212" top="126" width="265" height="24">
        <combo_box.item label="30 FPS" value="30" />
        <combo_box.item label="45 FPS" value="45" />
        <combo_box.item label="60 FPS" value="60" />
        <combo_box.item label="90 FPS" value="90" />
        <combo_box.item label="120 FPS" value="120" />
       </combo_box>
       <text name="kami_resolution_note" left="12" top="154" width="465" height="26" wrap="true">Adaptive scaling only runs with Performance Mode on.</text>
''',1)
s=s[:a]+panel+s[b:]
if 'height="1688"' not in s:
    raise RuntimeError("Scroll panel geometry changed")
s=s.replace('height="1688"','height="1764"',1)
p.write_text(s,encoding="utf-8")

# Moving-average frame pacing with 3s adjustment windows and a conservative
# hysteresis avoids rebuilding screen textures on each frame.
renderer="indra/newview/llviewerdisplay.cpp"
anchor='    LLPerfStats::RecordSceneTime T (LLPerfStats::StatType_t::RENDER_DISPLAY);'
runtime = '''    // KAMI native adaptive world resolution. No external shader runtime.
    // Only active in Performance Mode, and never while taking snapshots.
    // Frame pacing is sampled over a stable interval to avoid resize thrashing.
    if (!for_snapshot && !gCubeSnapshot && !gWindowResized &&
        gSavedSettings.getBOOL("KAMIPerformanceMode") &&
        gSavedSettings.getBOOL("KAMIDynamicResolution") &&
        gViewerWindow && gViewerWindow->getActive())
    {
        static F32 lastSampleTime = 0.f;
        static F64 sampleSeconds = 0.0;
        static U32 sampleFrames = 0;
        const F32 delta = gFrameIntervalSeconds.value();
        if (lastSampleTime == 0.f || gFrameTimeSeconds < lastSampleTime)
            lastSampleTime = gFrameTimeSeconds;
        if (delta > 0.003f && delta < 0.2f)
        {
            sampleSeconds += delta;
            ++sampleFrames;
        }
        if (gFrameTimeSeconds - lastSampleTime >= 3.0f)
        {
            if (sampleSeconds >= 1.0 && sampleFrames >= 30)
            {
                const F32 measuredFPS = static_cast<F32>(sampleFrames / sampleSeconds);
                const U32 target = llclamp(gSavedSettings.getU32("KAMITargetFPS"), 30U, 120U);
                const F32 previous = llclamp(gSavedSettings.getF32("RenderResolutionMultiplier"), 0.55f, 1.f);
                F32 next = previous;
                if (measuredFPS < target * 0.87f) next = llmax(0.55f, previous - 0.05f);
                else if (measuredFPS > target * 1.15f) next = llmin(1.f, previous + 0.05f);
                if (fabsf(next - previous) >= 0.025f)
                {
                    gSavedSettings.setF32("RenderResolutionMultiplier", next);
                    gPipeline.requestResizeScreenTexture();
                }
            }
            lastSampleTime = gFrameTimeSeconds;
            sampleSeconds = 0.0;
            sampleFrames = 0;
        }
    }
'''
edit(renderer,anchor,runtime+'\n'+anchor)

# Checks against accidentally shipping fake features or a new shader injector.
for name in ("KAMIDynamicResolution","KAMITargetFPS"):
    assert name in (R/settings).read_text(encoding="utf-8")
assert "gPipeline.requestResizeScreenTexture()" in (R/renderer).read_text(encoding="utf-8")
assert "RenderDeferredHBAO" in (R/xui).read_text(encoding="utf-8")
print("KAMI advanced native lighting and adaptive resolution wired.")
