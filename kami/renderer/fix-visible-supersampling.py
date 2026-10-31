#!/usr/bin/env python3
"""Ensure world supersampling reaches the visible window, not just the FBO.

Run AFTER enable-true-1440p.py and add-gpu-detection.py.
Match actual main-world 3D viewport dimensions to main deferred FBO,
then restore window-space viewport for HUD, UI and presentation.
Add a box-area resolve in the final world color shader: 2x2, 3x3, 4x4
depending on actual supersampling ratio, not the dropdown selection.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
PIPE = R / "indra/newview/pipeline.cpp"
DISPLAY = R / "indra/newview/llviewerdisplay.cpp"
SHADER = R / "indra/newview/app_settings/shaders/class1/deferred/postDeferredNoDoFF.glsl"
PANEL = R / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"


def replace_one(path, before, after):
    s = path.read_text(encoding="utf-8-sig")
    count = s.count(before)
    if count != 1:
        raise RuntimeError(f"{path.relative_to(R)}: expected exactly one anchor, found {count}: {before[:100]!r}")
    path.write_text(s.replace(before, after, 1), encoding="utf-8")


shader = SHADER.read_text(encoding="utf-8-sig")
for token in ("uniform int kami_resolve_grid;",
              "uniform vec2 kami_present_resolution;",
              "kamiResolveSupersampledWorld(vary_fragcoord.xy)",
              "for (int x = 0; x < 4; ++x)"):
    if token not in shader:
        raise RuntimeError(f"Missing actual GLSL 1440p/4K/8K resolve: {token}")

# RenderTarget::bindTarget already sets glViewport to its real FBO dimensions.
# However, renderer shader uniforms look at gGLViewport, which stayed at window
# pixels. Set the logical viewport to the *real* deferred target while drawing
# the main 3D scene. Don't touch snapshot/probe paths.
replace_one(DISPLAY,
    '''        gPipeline.mRT->deferredScreen.bindTarget();
        if (gUseWireframe)''',
    '''        gPipeline.mRT->deferredScreen.bindTarget();
        // KAMI: the active FBO has a larger viewport than the desktop window
        // in QHD/4K/8K modes. Keep shader VIEWPORT uniforms in sync with it.
        if (!gCubeSnapshot && !gSnapshot)
        {
            gGLViewport[0] = 0;
            gGLViewport[1] = 0;
            gGLViewport[2] = gPipeline.mRT->deferredScreen.getWidth();
            gGLViewport[3] = gPipeline.mRT->deferredScreen.getHeight();
        }
        if (gUseWireframe)'''
)

# Restore logical screen coordinates *after* deferred lighting & SSR, before
# the scene monitor and user interface. Avoid changing the hardware FBO
# while it remains bound. OpenGL viewport is also restored for safety.
s = DISPLAY.read_text(encoding="utf-8")
marker = "gPipeline.copyScreenSpaceReflections(&gPipeline.mRT->screen, &gPipeline.mSceneMap);"
if s.count(marker) != 1:
    raise RuntimeError("SSR post-render anchor missing")
cursor = s.index(marker) + len(marker)
end = s.find("LLPipeline::sUnderWaterRender = false;", cursor)
if end == -1 or end - cursor > 400:
    raise RuntimeError("Unable to locate main-scene viewport restoration point")
restore = '''// Return all HUD, UI and monitor code to window-native pixel coordinates.
        if (!gCubeSnapshot && !gSnapshot)
        {
            const LLRect& kami_window = gViewerWindow->getWorldViewRectRaw();
            gGLViewport[0] = kami_window.mLeft;
            gGLViewport[1] = kami_window.mBottom;
            gGLViewport[2] = kami_window.getWidth();
            gGLViewport[3] = kami_window.getHeight();
            glViewport(gGLViewport[0], gGLViewport[1],
                       gGLViewport[2], gGLViewport[3]);
        }

        '''
s = s[:end] + restore + s[end:]
DISPLAY.write_text(s, encoding="utf-8")

# The normal last pass uses bilinear filtering, meaning a downsampled pixel
# only sees a tiny part of a huge 4K/8K source. Supersample resolve covers the
# full output pixel footprint instead, and requires no extra full-size FBO.
replace_one(PIPE,
    '''    gDeferredPostNoDoFNoiseProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, (GLfloat)sourceBuffer->getWidth(), (GLfloat)sourceBuffer->getHeight());''',
    '''    gDeferredPostNoDoFNoiseProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, (GLfloat)sourceBuffer->getWidth(), (GLfloat)sourceBuffer->getHeight());

    // Resolve *actual* world pixels to the viewer's physical output size.
    // 1440p on a 1440p display stays one-to-one; 4K/8K on that same display
    // now integrate the extra source pixels, improving small detail/edges.
    // No tone/color or quality presets are silently changed.
    static LLStaticHashedString kami_present_res("kami_present_resolution");
    static LLStaticHashedString kami_grid("kami_resolve_grid");
    const F32 kami_out_width = (F32)llmax(1, gViewerWindow->getWorldViewRectRaw().getWidth());
    const F32 kami_out_height = (F32)llmax(1, gViewerWindow->getWorldViewRectRaw().getHeight());
    const F32 kami_ratio = llmin(
        (F32)sourceBuffer->getWidth() / kami_out_width,
        (F32)sourceBuffer->getHeight() / kami_out_height);
    const S32 kami_samples_per_axis = kami_ratio < 1.05f ? 1 :
        (kami_ratio >= 3.75f ? 4 : (kami_ratio >= 1.75f ? 3 : 2));
    gDeferredPostNoDoFNoiseProgram.uniform2f(kami_present_res,
        kami_out_width, kami_out_height);
    gDeferredPostNoDoFNoiseProgram.uniform1i(kami_grid, kami_samples_per_axis);'''
)

# Make the status truthful about both real GPU pixels and how those pixels
# are presented, so we can spot a mode that merely adds invisible work.
s = DISPLAY.read_text(encoding="utf-8")
anchor = '''            const std::string gpu_selection =
                gSavedSettings.getString("KAMIHardwareDetectionStatus");'''
if s.count(anchor) != 1:
    raise RuntimeError("Existing GPU readout anchor missing")
extra = '''            // Source buffer size alone does not prove a higher-quality image.
            // Show when KAMI is actually resolving supersampled world pixels
            // into the final physical window. These are not FPS estimates.
            const U32 kami_output_w = llmax(1, gViewerWindow->getWorldViewRectRaw().getWidth());
            const U32 kami_output_h = llmax(1, gViewerWindow->getWorldViewRectRaw().getHeight());
            const F32 kami_source_ratio = llmin(
                (F32)gpu_width / (F32)kami_output_w,
                (F32)gpu_height / (F32)kami_output_h);
            if (kami_source_ratio >= 1.05f)
            {
                const S32 grid = kami_source_ratio >= 3.75f ? 4 :
                    (kami_source_ratio >= 1.75f ? 3 : 2);
                resolution_status += llformat(" Output %u x %u, %dx%d SSAA resolve.",
                    kami_output_w, kami_output_h, grid, grid);
            }
            else
                resolution_status += " One-to-one window output.";
'''
s=s.replace(anchor,extra+anchor,1)
DISPLAY.write_text(s,encoding="utf-8")

tree=ET.parse(PANEL)
elements=[e for e in tree.getroot().iter() if e.get("name")=="kd_resolution"]
if len(elements)!=1 or elements[0].get("control_name")!="KombatantRenderWidth":
    raise RuntimeError("KAMI resolution dropdown does not control world buffer")
choices={c.get("value"):c for c in elements[0] if c.tag=="combo_box.item"}
if set(choices) != {"0","2560","3840","7680"}:
    raise RuntimeError(f"Unexpected resolutions {choices}")
choices["2560"].set("label","Native 1440p — 2560 x 1440 at 16:9 (recommended RTX 5070)")
choices["3840"].set("label","4K — true source pixels + 3x3 supersample resolve")
choices["7680"].set("label","8K — true source pixels + 4x4 photo resolve")
elements[0].set("tool_tip",
    "GPU detection starts RTX 5070 at real 1440p while preserving saved choices. "
    "4K and 8K render more true scene pixels, then use pixel-footprint supersample "
    "filtering so fine edges/glints reach the window image. "
    "Higher pixel counts cost FPS and VRAM; actual visible improvements depend on "
    "window resolution and scene detail. The HUD stays window native.")
notes=[e for e in tree.getroot().iter() if e.get("name")=="kd_resolution_note"]
if len(notes)!=1:
    raise RuntimeError("Missing active framebuffer diagnostics")
notes[0].set("tool_tip",
    "Shows real allocated scene pixels, final window output and active SSAA grid. "
    "At 1440p desktop, native 1440p is 1:1, 4K adds SSAA 3x3 and 8K adds 4x4. "
    "8K is photo-only and may exceed your graphics memory.")
ET.indent(tree,space=" ")
tree.write(PANEL,encoding="utf-8",xml_declaration=True)
print("KAMI world-vs-window viewport sync and 2x2/3x3/4x4 supersampling resolved.")
