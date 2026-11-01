#!/usr/bin/env python3
"""Modernize the existing 2,000+ line Graphics panel without replacing renderer controls.

Run after the native KAMI Workspaces and KAMI ALIVE steps. Preserve existing
XUI bindings, callbacks, sliders, options, visibility controls, values and
legacy-specific tooltips; the display layout is the only part changed here.
No added render passes, default graphics changes, or shader reloads.
"""
from collections import Counter
from pathlib import Path
import xml.etree.ElementTree as ET

R = Path(__file__).resolve().parents[2]
P = R / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree = ET.parse(P)
root = tree.getroot()
assert root.tag == "panel" and root.get("name") == "Display panel"

stack = next((x for x in root.iter("layout_stack") if x.get("name") == "gfx_stack"), None)
if stack is None:
    raise RuntimeError("KAMI Graphics: original graphics layout stack not found")
if any(e.get("name") == "kami_graphics_intro_card" for e in stack):
    raise RuntimeError("KAMI Graphics: control deck is already installed")

# Verify every old interactive element is carried over intact. We can restyle
# labels and buttons, but never replace the 555 control objects or their ids.
def control_signature():
    return Counter(
        (x.tag, x.get("name"), x.get("control_name"),
         tuple((child.tag, child.get("function"), child.get("parameter"))
               for child in x if child.tag.endswith(".commit_callback")))
        for x in root.iter()
        if x.tag in ("check_box", "combo_box", "slider", "button")
        and not x.get("name", "").startswith("kami_alive_")
    )

original_controls = control_signature()
original_count = sum(original_controls.values())
assert original_count >= 200, f"Graphics panel unexpectedly small ({original_count})"

# Compact fixed-height native hero card, before the preserved accordion groups.
card = ET.Element("layout_panel", {
    "name": "kami_graphics_intro_card",
    "follows": "top|left|right", "layout": "topleft",
    "height": "159", "width": "515",
    "auto_resize": "false", "background_visible": "true",
    "bg_alpha_color": "0.045 0.068 0.11 0.94",
})
def title(name, value, top, *, color="0.89 0.94 1 1", font="SansSerif"):
    e = ET.SubElement(card, "text", {
        "name": name, "layout": "topleft", "follows": "top|left",
        "left": "20", "top": str(top), "width": "465", "height": "23",
        "font": font, "text_color": color, "mouse_opaque": "false",
    })
    e.text = value

title("kami_graphics_brand", "KAMI  |  GRAPHICS CONTROL DECK", 12,
      font="SansSerifBoldLarge", color="0.67 0.84 1 1")
title("kami_graphics_subtitle", "CLIENT-SIDE GRAPHICS  /  FRAME-TIME AWARE", 38,
      color="0.68 0.77 0.88 1")
ET.SubElement(card, "check_box", {
    "name": "kami_alive_enabled", "control_name": "KAMIAliveEnabled",
    "label": "KAMI ALIVE: adaptive material response (beta)",
    "left": "20", "top": "69", "width": "465", "height": "24",
    "tool_tip": "Locally refines polished PBR metals. Off by default. Does not change SL assets, scripts or other viewers.",
})
ET.SubElement(card, "slider", {
    "name": "kami_alive_strength", "control_name": "KAMIAliveStrength",
    "label": "MATERIAL RESPONSE",
    "left": "20", "top": "105", "width": "435", "height": "17",
    "label_width": "175", "can_edit_text": "true",
    "min_val": "0", "max_val": "1", "increment": "0.05",
    "decimal_digits": "2",
    "tool_tip": "Maximum enhancement. Actual amount backs off under sustained CPU frame-time load.",
})
title("kami_graphics_frame_note",
      "GPU-only surface work; reduced smoothly on sustained CPU frame delays.", 135,
      color="0.67 0.77 0.88 1")

stack.insert(0, card)
root.set("label", "KAMI Graphics")
root.set("width", root.get("width", "510"))

# Replace the Black-Dragon-looking expansion bars with native KAMI styling.
# The original Pref.Tab callbacks and Prefs*Visible toggle settings survive.
groups = (
    ("title_layout_panel", "basic_expand", "01  |  DISPLAY  /  DETAIL  /  FPS"),
    ("deferred_layout", "advanced_expand", "02  |  SHADOWS  /  LIGHTING  /  REFLECTIONS"),
    ("post_layout", "finetuning_expand", "03  |  IMAGE FINISH  /  POST FX"),
)
for section, button_name, caption in groups:
    panels = [x for x in stack if x.tag == "layout_panel" and x.get("name") == section]
    if len(panels) != 1:
        raise RuntimeError(f"KAMI Graphics: missing section {section}")
    panel = panels[0]
    panel.set("height", "35")
    panel.set("background_visible", "true")
    panel.set("bg_alpha_color", "0.09 0.13 0.20 0.87")
    buttons = [x for x in panel if x.tag == "button" and x.get("name") == button_name]
    labels = [x for x in panel if x.tag == "text" and x.get("name") == "general_label"]
    if len(buttons) != 1 or len(labels) != 1:
        raise RuntimeError(f"KAMI Graphics: missing accordion controls for {section}")
    button = buttons[0]
    button.set("height", "31")
    button.set("width", "493")
    button.set("image_unselected", "PushButton_Off")
    button.set("image_selected", "PushButton_Selected")
    button.set("image_hover_unselected", "Toolbar_Middle_Over")
    button.set("image_hover_selected", "Toolbar_Middle_Over")
    # Native viewer arrow assets provide a consistent, stateful chevron:
    # right when closed, down when expanded. Keep the same toggle control
    # and Pref.Tab callback, rather than adding a second clickable element.
    button.set("image_overlay", "Accordion_ArrowClosed_Off")
    button.set("image_overlay_selected", "Accordion_ArrowOpened_Off")
    button.set("image_overlay_alignment", "right")
    button.set("tool_tip", "Expand or collapse KAMI graphics settings")
    label = labels[0]
    label.text = caption
    label.set("width", "445")
    label.set("height", "20")
    label.set("top_delta", "7")
    label.set("font", "SansSerifBold")
    label.set("text_color", "0.82 0.91 1 1")
    label.set("mouse_opaque", "false")

# Modernize the existing combo-box arrows without replacing their item lists,
# Ticker/default/warning icons, keyboard navigation, or saved-setting bindings.
# The original transparent button hit target remains the same size and position.
dropdown_arrows = 0
for arrow in root.iter("combo_box.drop_down_button"):
    arrow.set("image_overlay", "Arrow_Down")
    arrow.set("image_overlay_alignment", "center")
    dropdown_arrows += 1
assert dropdown_arrows >= 30, (
    f"KAMI Graphics: legacy dropdown arrows unexpectedly missing ({dropdown_arrows})"
)

# Existing subgroup separators become visual Kami navigation, keeping their
# top_pad/position anchors so every following control remains where expected.
subgroups = {
    "Memory": "GPU  /  MEMORY",
    "Quality": "WORLD  /  DETAIL QUALITY",
    "Performance": "PERFORMANCE  /  AVATARS",
    "Shadows": "LIGHTING  /  SHADOWS",
    "Screen Space Reflections": "REFLECTIONS  /  SCREEN SPACE",
    "Reflection Probes": "REFLECTIONS  /  ENVIRONMENT PROBES",
    "Volumetric Lighting": "LIGHTING  /  ATMOSPHERE",
    "Post Processing Glow": "IMAGE FINISH  /  NATIVE GLOW",
    "Post Effects": "IMAGE FINISH  /  EFFECTS",
}
seen = set()
for e in root.iter("text"):
    if e.get("name") != "separator_label":
        continue
    name = (e.text or "").strip()
    if name in subgroups:
        seen.add(name)
        e.text = subgroups[name]
        e.set("font", "SansSerifBold")
        e.set("text_color", "0.66 0.81 0.97 1")
        e.set("height", "17")
# Some headers were already renamed by the preceding Kami Graphics pass.
# Require a meaningful selection, not legacy labels that no longer exist.
already_themed = {(e.text or "").strip() for e in root.iter("text")
                  if e.get("name") == "separator_label"}
# These headings are already styled in modernize-graphics-panel.py,
# which runs immediately before this script. Accept that exact case.
expected_prior = {
    "TEXTURE MEMORY & STREAMING",
    "SCENE DETAIL & MATERIAL QUALITY",
    "FRAME PACING & AVATAR COMPLEXITY",
    "LEGACY SHADER EFFECTS",
}
assert len(seen) >= 3 or expected_prior.issubset(already_themed), (
    f"KAMI Graphics: missing both legacy and KAMI-styled separators ({len(seen)})"
)

# Replace glyph-heavy legacy reset captions; do not change Pref.Default.
reset_count = 0
for button in root.iter("button"):
    if button.get("name") == "default":
        button.set("label", "RESET")
        button.set("font", "SansSerifSmall")
        reset_count += 1
assert reset_count >= 6, f"KAMI Graphics: reset controls missing ({reset_count})"

# Shorter, clearer labels; semantic control names and callbacks are unchanged.
labels = {
    "RenderVSyncEnable": "VSync / Frame Pacing",
    "CompressTextures": "GPU Texture Compression (OpenGL)",
    "AutomaticMemoryManagement": "Automatic Texture Memory",
    "DynamicLOD": "Adaptive Object Detail",
    "RenderDeferredLights": "Dynamic World Lights",
    "RenderDisableVintageMode": "Advanced PBR Rendering",
    "RenderGlowHDR": "HDR Native Glow",
}
for el in root.iter("check_box"):
    name = el.get("name")
    if name in labels:
        el.set("label", labels[name])
assert control_signature() == original_controls, "KAMI Graphics accidentally changed an existing control or binding"

ET.indent(tree, space=" ")
tree.write(P, encoding="utf-8", xml_declaration=True)
ET.parse(P)
print(f"PASS KAMI Graphics: {original_count} original interactive controls and {reset_count} reset callbacks preserved; 3 Kami chevron accordions, {dropdown_arrows} dropdown arrows and ALIVE card installed.")
