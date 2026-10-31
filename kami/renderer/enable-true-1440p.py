#!/usr/bin/env python3
"""KAMI true-1440p world-rendering integration.

Run AFTER the existing final live-resolution stabilization step and UI/theme
integration. Preserve the native OpenGL rendering pipeline, separate 3D world
buffer from the UI viewport, and do not introduce temporal upscaling.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "indra/newview/app_settings/settings_blackdragon.xml"
PIPELINE = ROOT / "indra/newview/pipeline.cpp"
DISPLAY = ROOT / "indra/newview/llviewerdisplay.cpp"
PANEL = ROOT / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"


def replace_one(path, before, after):
    value = path.read_text(encoding="utf-8-sig")
    count = value.count(before)
    if count != 1:
        raise RuntimeError(f"{path.relative_to(ROOT)}: expected one anchor, got {count}: {before[:85]!r}")
    path.write_text(value.replace(before, after, 1), encoding="utf-8")


def get_pair(parent, name):
    entries = list(parent)
    matches = [entries[i + 1] for i, e in enumerate(entries[:-1])
               if e.tag == "key" and e.text == name]
    if len(matches) != 1 or matches[0].tag != "map":
        raise RuntimeError(f"Expected exactly one settings key {name}")
    return matches[0]


def set_field(record, name, value):
    fields = list(record)
    found = [fields[i + 1] for i, e in enumerate(fields[:-1])
             if e.tag == "key" and e.text == name]
    if len(found) != 1:
        raise RuntimeError(f"Setting field {name} not unique")
    found[0].text = value


# The original Kombatant setting gets created by kombatant.patch at build time.
# The build pipeline intentionally must not edit kombatant.patch itself.
tree = ET.parse(SETTINGS)
settings = tree.getroot().find("map")
if settings is None:
    raise RuntimeError("Settings LLSD map not found")
render_width = get_pair(settings, "KombatantRenderWidth")
set_field(render_width, "Comment",
          "Requested main-world framebuffer. 2560 means true 1440 pixels high "
          "with viewport aspect preserved (2560x1440 at 16:9). "
          "0 uses window-native; 3840/7680 are optional higher widths. "
          "Not an upscaling setting. The choice is persisted; unsupported "
          "render targets fall back to window-native.")
set_field(render_width, "Persist", "1")
set_field(render_width, "Value", "2560")

# Visible reason when hardware cannot support the minimum real 1440p buffer.
try:
    get_pair(settings, "KAMI1440FallbackReason")
except RuntimeError:
    entry = ET.Element("key")
    entry.text = "KAMI1440FallbackReason"
    record = ET.Element("map")
    for tag, value in (
        ("key", "Comment"), ("string", "Transient explanation when a true 1440p framebuffer cannot be allocated."),
        ("key", "Persist"), ("integer", "0"),
        ("key", "Type"), ("string", "String"),
        ("key", "Value"), ("string", ""),
    ):
        item = ET.SubElement(record, tag)
        item.text = value
    settings.extend([entry, record])
else:
    raise RuntimeError("KAMI1440FallbackReason unexpectedly already defined")

ET.indent(tree, space=" ")
tree.write(SETTINGS, encoding="utf-8", xml_declaration=True)

# Real pixels, not a 1080p render upscaled on presentation:
# QHD mode allocates a 1440-high native 3D framebuffer at ANY viewport aspect.
# A 16:9 scene produces EXACTLY 2560 x 1440; 21:9 keeps 1440 rows with a
# larger width. Screen UI, HUD, FOV and final presentation remain window based.
old_dimensions = '''        const U32 out_height = llmax(1U, (U32)((F64)target * height / width + 0.5));'''
new_dimensions = '''        // KAMI true QHD: native 1440 source rows, not a 1080p upscale.
        // Preserve the 3D viewport aspect ratio to prevent camera crop/stretch.
        // Other manual targets remain width-based for backwards compatibility.
        const U32 source_width = width;
        const U32 source_height = height;
        const U32 out_height = target == 2560U ? 1440U
            : llmax(1U, (U32)((F64)target * source_height / source_width + 0.5));
        const U32 target_width = target == 2560U
            ? llmax(1U, (U32)((F64)source_width * out_height / source_height + 0.5))
            : target;'''
replace_one(PIPELINE, old_dimensions, new_dimensions)
replace_one(
    PIPELINE,
    '''        if (target <= 7680 && target <= limit && out_height <= limit &&
            target <= (U32)llmax(0, viewport_limit[0]) &&
            out_height <= (U32)llmax(0, viewport_limit[1]))''',
    '''        if (target <= 7680 && target_width <= limit && out_height <= limit &&
            target_width <= (U32)llmax(0, viewport_limit[0]) &&
            out_height <= (U32)llmax(0, viewport_limit[1]))'''
)
replace_one(
    PIPELINE,
    '''            width = target;
            height = out_height;
            return;''',
    '''            width = target_width;
            height = out_height;
            return;'''
)
replace_one(
    PIPELINE,
    '''        LL_WARNS() << "Kombatant live render size exceeds GPU limits; using window resolution." << LL_ENDL;
        gSavedSettings.setU32("KombatantRenderWidth", 0);''',
    '''        LL_WARNS() << "Kombatant live render size exceeds GPU limits; using window resolution." << LL_ENDL;
        if (target == 2560U)
            gSavedSettings.setString("KAMI1440FallbackReason",
                "True 1440p unavailable: GL framebuffer or viewport limit; using native window pixels.");
        gSavedSettings.setU32("KombatantRenderWidth", 0);'''
)
replace_one(
    PIPELINE,
    '''        // Disable the oversized target so resize does not retry it every frame.
        gSavedSettings.setU32("KombatantRenderWidth", 0);''',
    '''        // Disable the oversized target so resize does not retry it every frame.
        if (gSavedSettings.getU32("KombatantRenderWidth") == 2560U)
            gSavedSettings.setString("KAMI1440FallbackReason",
                "True 1440p allocation failed; using window-native resolution.");
        gSavedSettings.setU32("KombatantRenderWidth", 0);'''
)

# Avoid a full native-QHD copy pass when Glow, Sepia, Greyscale and
# Posterization are all disabled. This shader normally combines glow and
# optional color effects; with all four neutral it is an identity operation.
# Preserve the original path whenever one of those effects is enabled.
replace_one(
    PIPELINE,
    '''    generateGlow(&mPostPingMap);

    LLRenderTarget* sourceBuffer = &mPostPingMap;
    LLRenderTarget* targetBuffer = &mPostPongMap;

    //BD - Volumetric Lighting
    renderVolumetric(sourceBuffer, sourceBuffer);
    
    combineGlow(sourceBuffer, targetBuffer);
    std::swap(sourceBuffer, targetBuffer);''',
    '''    // The combine-glow shader is an identity copy when all four features
    // are neutral. Avoid an unnecessary full-QHD GPU read/write and glow clear.
    // RenderNumColors <= 2 is neutral in glowcombineF.glsl.
    const bool needsGlowComposite = sRenderGlow ||
        RenderGreyscaleStrength > 0.f || RenderSepiaStrength > 0.f ||
        RenderNumColors > 2U;
    if (needsGlowComposite)
        generateGlow(&mPostPingMap);

    LLRenderTarget* sourceBuffer = &mPostPingMap;
    LLRenderTarget* targetBuffer = &mPostPongMap;

    // Preserve independent volumetric lighting behavior.
    renderVolumetric(sourceBuffer, sourceBuffer);

    if (needsGlowComposite)
    {
        combineGlow(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }'''
)

# Report ACTUAL framebuffer allocation, rather than the dropdown choice.
# If the requested QHD world buffer isn't 1440 high, say so explicitly.
replace_one(
    DISPLAY,
    '''            if (requested == 3840 && gpu_width != 3840)''',
    '''            if (requested == 2560 && gpu_height != 1440)
                resolution_status += "True 1440p requested; actual GPU target differs.";
            else if (requested == 2560 && gpu_width == 2560)
                resolution_status += "TRUE QHD 2560 x 1440 world pixels (no upscale).";
            else if (requested == 2560)
                resolution_status += "TRUE 1440 source rows; aspect ratio preserved (no upscale).";
            else if (requested == 3840 && gpu_width != 3840)'''
)
replace_one(
    DISPLAY,
    '''            else
                resolution_status += "Window-native world rendering.";
            if (gSavedSettings.getString("KAMILiveResolutionStatus") != resolution_status)''',
    '''            else
                resolution_status += "Window-native world rendering.";
            const std::string qhd_failure = gSavedSettings.getString("KAMI1440FallbackReason");
            if (!qhd_failure.empty())
                resolution_status += " " + qhd_failure;
            if (gSavedSettings.getString("KAMILiveResolutionStatus") != resolution_status)'''
)

# Keep source resolution selection explicit in the existing Fire Garden UI.
ui = ET.parse(PANEL)
nodes = [e for e in ui.getroot().iter() if e.get("name") == "kd_resolution"]
if len(nodes) != 1:
    raise RuntimeError("Existing live render resolution control missing")
selector = nodes[0]
if selector.get("control_name") != "KombatantRenderWidth":
    raise RuntimeError("Resolution control no longer bound to real framebuffer")
items = [e for e in selector if e.tag == "combo_box.item"]
existing_values = {item.get("value") for item in items}
if existing_values != {"0", "3840", "7680"}:
    raise RuntimeError(f"Unexpected live-render modes: {existing_values}")
qhd = ET.Element("combo_box.item")
qhd.set("label", "True 1440p core — 2560 x 1440 at 16:9")
qhd.set("value", "2560")
selector.insert(1, qhd)
for item in items:
    if item.get("value") == "0":
        item.set("label", "Window native — emergency FPS fallback")
selector.set(
    "tool_tip",
    "True 1440p renders 1440 real GPU pixel rows, keeping the world aspect ratio. "
    "On 16:9 that is 2560 x 1440, even on a 1080p monitor (downsampled to display). "
    "This costs 78% more pixels than 1080p. "
    "Window native is available manually for low-end hardware. "
    "No automatic viewport rescaling, no upscaling, and no forced FPS cap."
)
status = [e for e in ui.getroot().iter() if e.get("name") == "kd_resolution_note"]
if len(status) != 1 or status[0].get("control_name") != "KAMILiveResolutionStatus":
    raise RuntimeError("Live resolution readout no longer bound to actual GPU target")
status[0].set("tool_tip",
    "Reports actual allocated GPU world pixels, not just the requested resolution. "
    "True 1440p means actual framebuffer height of 1440. If unsupported, the reason is shown.")
ET.indent(ui, space=" ")
ui.write(PANEL, encoding="utf-8", xml_declaration=True)

# PBR and optional effects remain native, never force old broken auto resize.
pipeline = PIPELINE.read_text(encoding="utf-8")
assert "target == 2560U ? 1440U" in pipeline
assert "const U32 target_width = target == 2560U" in pipeline
assert "glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport_limit)" in pipeline
assert "gPipeline.requestResizeScreenTexture();" not in pipeline[
    pipeline.index("static void kombatantScreenSize"):
    pipeline.index("void LLPipeline::resizeScreenTexture")
]
assert "KAMISmartBackup" in (ROOT / "indra/newview/kami_smart_engine.h").read_text(encoding="utf-8")
print("KAMI true native QHD framebuffer integration staged (2560x1440 at 16:9).")
