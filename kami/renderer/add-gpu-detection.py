#!/usr/bin/env python3
"""Select a conservative first-run KAMI world resolution using the active GL GPU.

This patch runs AFTER enable-true-1440p.py. GPU identification is local and
one-time at OpenGL buffer initialization, before the first main FBO allocation.
Existing saved render-width choices and later manual overrides win.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "indra/newview/app_settings/settings_blackdragon.xml"
PIPELINE = ROOT / "indra/newview/pipeline.cpp"
DISPLAY = ROOT / "indra/newview/llviewerdisplay.cpp"
PANEL = ROOT / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"


def replace_once(path, needle, replacement):
    source = path.read_text(encoding="utf-8-sig")
    if source.count(needle) != 1:
        raise RuntimeError(f"{path.relative_to(ROOT)}: expected 1 anchor, found {source.count(needle)}: {needle[:65]!r}")
    path.write_text(source.replace(needle, replacement, 1), encoding="utf-8")


tree = ET.parse(SETTINGS)
settings = tree.getroot().find("map")
if settings is None:
    raise RuntimeError("Black Dragon settings LLSD map missing")
entries = list(settings)
existing = {entries[i].text for i in range(0, len(entries), 2) if entries[i].tag == "key"}
to_add = [
    ("KAMIHardwareResolutionInitialized", "Boolean", "0", "First-run GPU resolution selection already considered."),
    ("KAMIHardwareLastRenderer", "String", "", "Last active OpenGL GPU renderer, stored locally to re-evaluate on GPU changes."),
    ("KAMIHardwareAutoLastWidth", "U32", "2560", "Most recent auto-selected world-buffer width; manual changes will be preserved."),
    ("KAMIHardwareDetectionStatus", "String", "GPU detection not run yet.", "Local, transient detected GPU/resolution recommendation. Not telemetry."),
]
for name, typ, value, description in to_add:
    if name in existing:
        raise RuntimeError(f"Setting already exists: {name}")
    key = ET.SubElement(settings, "key")
    key.text = name
    entry = ET.SubElement(settings, "map")
    for tag, raw in (
        ("key", "Comment"), ("string", description),
        ("key", "Persist"), ("integer", "0" if name == "KAMIHardwareDetectionStatus" else "1"),
        ("key", "Type"), ("string", typ),
        ("key", "Value"), ("string" if typ == "String" else "integer", value),
    ):
        child = ET.SubElement(entry, tag)
        child.text = raw
ET.indent(tree, space=" ")
tree.write(SETTINGS, encoding="utf-8", xml_declaration=True)

# This is *not* a claim of benchmarking every GPU.
# Unknown/ambiguous models and all unlisted GPUs stay at window-native.
# The explicit whitelist can grow as the actual viewer is tested.
helper = r'''// KAMI active-renderer GPU classification: runs only during GL buffer startup.
// No Windows WMI queries, device enumeration, benchmarking or external telemetry.
static std::string kamiLowercaseGPUString(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

static U32 kamiRecommendedWorldWidth(const std::string& renderer)
{
    // Conservative verified-modern discrete GPU candidates. The RTX 5070 is
    // intentionally included. This is a *starting quality* recommendation,
    // not a promise of measured FPS or VRAM capacity.
    static const char* kQHDGPUIdentifiers[] = {
        "rtx 5070", "rtx 5080", "rtx 5090",
        "rtx 4070", "rtx 4080", "rtx 4090",
        "rtx 3070", "rtx 3080", "rtx 3090",
        "rx 6800", "rx 6900", "rx 6950",
        "rx 7700", "rx 7800", "rx 7900", "rx 9070",
    };
    const std::string gpu = kamiLowercaseGPUString(renderer);
    for (const char* model : kQHDGPUIdentifiers)
    {
        if (gpu.find(model) != std::string::npos)
            return 2560U; // True 1440p world buffer; not upscaling.
    }
    return 0U; // Intel UHD/Iris, Radeon integrated, entry GPUs and unknown GPUs.
}

static void kamiDetectWorldRendererOnce()
{
    // Called before main world FBO allocation. Never query the GPU per frame.
    static bool processed = false;
    if (processed)
        return;

    const GLubyte* raw_gpu = glGetString(GL_RENDERER);
    if (!raw_gpu || !raw_gpu[0])
        return; // GL context unavailable: retry on next createGLBuffers().

    processed = true;
    const std::string renderer(reinterpret_cast<const char*>(raw_gpu));
    const std::string previous = gSavedSettings.getString("KAMIHardwareLastRenderer");
    const bool checked = gSavedSettings.getBOOL("KAMIHardwareResolutionInitialized");
    const U32 previous_auto = gSavedSettings.getU32("KAMIHardwareAutoLastWidth");
    const U32 selected = gSavedSettings.getU32("KombatantRenderWidth");
    LLControlVariable* render_control = gSavedSettings.getControl("KombatantRenderWidth");
    const bool fresh_default = render_control && render_control->isDefault();

    // First run: touch ONLY a previously untouched default. Later startup:
    // re-evaluate on a different GPU only if the previously auto-selected
    // value is still selected. Do not erase explicit 0/2560/4K/8K choices.
    const bool first_auto = !checked && fresh_default;
    const bool gpu_changed_auto = checked && previous != renderer && selected == previous_auto;
    const bool choose_automatically = first_auto || gpu_changed_auto;
    if (choose_automatically)
    {
        const U32 recommendation = kamiRecommendedWorldWidth(renderer);
        if (selected != recommendation)
            gSavedSettings.setU32("KombatantRenderWidth", recommendation);
        gSavedSettings.setU32("KAMIHardwareAutoLastWidth", recommendation);
        gSavedSettings.setString("KAMI1440FallbackReason", "");
        gSavedSettings.setString("KAMIHardwareDetectionStatus",
            renderer + (recommendation == 2560U ?
                ": true native 1440p selected automatically." :
                ": window-native resolution chosen for FPS safety."));
    }
    else
    {
        gSavedSettings.setString("KAMIHardwareDetectionStatus",
            renderer + ": saved or manually selected world resolution preserved.");
    }
    gSavedSettings.setString("KAMIHardwareLastRenderer", renderer);
    gSavedSettings.setBOOL("KAMIHardwareResolutionInitialized", true);
    LL_INFOS("KAMI") << gSavedSettings.getString("KAMIHardwareDetectionStatus") << LL_ENDL;
}

'''
replace_once(
    PIPELINE,
    "void LLPipeline::createGLBuffers()\n{",
    helper + "void LLPipeline::createGLBuffers()\n{"
)
replace_once(
    PIPELINE,
    '''    assertInitialized();

    stop_glerror();

    GLuint resX = gViewerWindow->getWorldViewWidthRaw();''',
    '''    assertInitialized();

    // Detect the ACTIVE OpenGL renderer before allocating the first world FBO.
    // This is a one-time startup recommendation and preserves manual choices.
    kamiDetectWorldRendererOnce();

    stop_glerror();

    GLuint resX = gViewerWindow->getWorldViewWidthRaw();'''
)

# C++ compatibility, independent of transitive STL includes.
replace_once(
    PIPELINE, '#include "gltfscenemanager.h"\n',
    '#include "gltfscenemanager.h"\n#include <algorithm>\n#include <cctype>\n'
)

# Existing actual-world-pixel readout shows the GPU decision in Display.
replace_once(
    DISPLAY,
    '''            if (gSavedSettings.getString("KAMILiveResolutionStatus") != resolution_status)
                gSavedSettings.setString("KAMILiveResolutionStatus", resolution_status);''',
    '''            const std::string gpu_selection =
                gSavedSettings.getString("KAMIHardwareDetectionStatus");
            if (!gpu_selection.empty())
                resolution_status += " GPU: " + gpu_selection;
            if (gSavedSettings.getString("KAMILiveResolutionStatus") != resolution_status)
                gSavedSettings.setString("KAMILiveResolutionStatus", resolution_status);'''
)

# The explicit combo selection remains the manual override. Avoid introducing
# a deceptive auto-adjust toggle (real-time dynamic scaling is quarantined).
ui = ET.parse(PANEL)
control = [n for n in ui.getroot().iter() if n.get("name") == "kd_resolution"]
if len(control) != 1 or control[0].get("control_name") != "KombatantRenderWidth":
    raise RuntimeError("QHD manual resolution selector unavailable")
options = {e.get("value") for e in control[0] if e.tag == "combo_box.item"}
if options != {"0", "2560", "3840", "7680"}:
    raise RuntimeError(f"Unexpected resolution choices: {options}")
control[0].set("tool_tip",
    "At first launch, KAMI checks the active OpenGL GPU and automatically "
    "selects true 1440p for confirmed modern discrete models (including RTX 5070). "
    "Unlisted, integrated and unknown GPUs use window-native pixels for FPS safety. "
    "Your manual selection is saved and takes priority. When the active GPU changes, "
    "only an unchanged auto-selected value is recalculated. "
    "True 1440p draws 1440 actual GPU pixel rows; no hidden upscaling.")
status = [n for n in ui.getroot().iter() if n.get("name") == "kd_resolution_note"]
if len(status) != 1:
    raise RuntimeError("World buffer status unavailable")
status[0].set("tool_tip",
    "Shows actual world framebuffer pixels and the detected GPU. GPU detection "
    "runs locally at graphics startup, does not send telemetry, and does not "
    "guarantee FPS. Manual resolution selection always wins.")
ET.indent(ui, space=" ")
ui.write(PANEL, encoding="utf-8", xml_declaration=True)

assert "kamiDetectWorldRendererOnce();" in PIPELINE.read_text(encoding="utf-8")
assert "const bool first_auto = !checked && fresh_default;" in PIPELINE.read_text(encoding="utf-8")
assert "const bool gpu_changed_auto = checked && previous != renderer && selected == previous_auto;" in PIPELINE.read_text(encoding="utf-8")
print("KAMI local GPU detection integrated before main FBO allocation with manual-preference safety.")
