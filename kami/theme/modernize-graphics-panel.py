#!/usr/bin/env python3
"""Modernize KAMI's native Graphics tab without replacing renderer settings.

Preserves original control_name, selection lists, callbacks, collapse toggles
and scroll container. The result is a reorganized visual hierarchy with room
for existing labels, plus the opt-in KAMI ALIVE control. No new GPU passes.
Run after KAMI ALIVE's settings integration on the native Windows CI path.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
GRAPHICS = ROOT / "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree = ET.parse(GRAPHICS)
root = tree.getroot()
assert root.tag == "panel" and root.get("name") == "Display panel"
all_nodes = list(root.iter())


def one(tag, name):
    items = [n for n in root.iter(tag) if n.get("name") == name]
    if len(items) != 1:
        raise RuntimeError(f"KAMI Graphics missing or duplicate {tag}:{name} ({len(items)})")
    return items[0]


# Maintain every functional input and its control binding, even where the
# original XML uses repeated or legacy-themed visible labels.
input_tags = {"check_box", "button", "combo_box", "slider", "spin_ctrl",
              "radio_group", "text_editor", "color_swatch", "scroll_list"}
def bindings():
    return [(e.tag, e.get("name"), e.get("control_name"))
            for e in root.iter() if e.tag in input_tags and e.get("control_name")]
before = bindings()

root.set("label", "KAMI | Graphics + Frame Pacing")
headers = {
    "title_layout_panel": "KAMI  /  DISPLAY, DETAIL & FPS",
    "deferred_layout": "KAMI  /  LIGHTING, SHADOWS & REFLECTIONS",
    "post_layout": "KAMI  /  OPTIONAL POST-FX & COMPATIBILITY",
}
for layout, caption in headers.items():
    section = one("layout_panel", layout)
    texts = [n for n in section.iter("text") if n.get("name") == "general_label"]
    assert len(texts) == 1, f"Missing named section header in {layout}"
    header = texts[0]
    header.text = caption
    header.set("font", "SansSerifBold")
    header.set("text_color", "0.82 0.92 1 1")
    header.set("width", "465")
    header.set("height", "18")

# Existing collapsible sections remain distinct; keep them optional and
# preserve their exact visibility setting keys/controls.
assert one("layout_panel", "basic_layout_panel").get("visibility_control") == "PrefsViewerVisible"
assert one("layout_panel", "advanced_layout_panel").get("visibility_control") == "PrefsDeferredVisible"
assert one("layout_panel", "finetuning_layout_panel").get("visibility_control") == "PrefsPostVisible"

section_names = {
    "Memory": "TEXTURE MEMORY & STREAMING",
    "Quality": "SCENE DETAIL & MATERIAL QUALITY",
    "Performance": "FRAME PACING & AVATAR COMPLEXITY",
    "Shadows": "SHADOWS & DISTANCE",
    "Screen Space Reflections": "REFLECTIONS / SCREEN SPACE",
    "High Dynamic Range (HDR)": "HDR & LIGHTING RANGE",
    "Reflection Probes": "REFLECTION PROBES",
    "Screen Space Ambient Occlusion": "CONTACT SHADING & AO",
    "Depth of Field": "CAMERA FOCUS / DEPTH OF FIELD",
    "Volumetric Lighting": "VOLUMETRIC LIGHT (OPTIONAL)",
    "Motion Blur": "MOTION BLUR (OPTIONAL)",
    "Tone Mapping": "COLOR & TONE MAPPING",
    "Post Processing Glow": "LEGACY GLOW (OPTIONAL)",
    "Post Effects": "LEGACY SHADER EFFECTS",
}
found = set()
for e in root.iter("text"):
    old = (e.text or "").strip()
    if old in section_names:
        e.text = section_names[old]
        e.set("font", "SansSerifBold")
        e.set("text_color", "0.66 0.86 0.98 1")
        e.set("width", "465")
        found.add(old)
if set(section_names) != found:
    raise RuntimeError("Graphics sections changed upstream: " +
                       repr(sorted(set(section_names) - found)))

# Most old checkboxes had a 115px click/label region for sentences longer
# than the available width. Enlarge only full-row, left-anchored controls;
# inline controls in other positions retain their original geometry.
expanded = 0
for e in root.iter("check_box"):
    if e.get("left") == "10" and e.get("width") == "115":
        e.set("width", "425")
        expanded += 1
assert expanded >= 20

# The ALIVE beta is visible in Graphics as well as the Workspaces menu.
# It binds the same setting; there is no second feature instance.
basic = one("layout_panel", "basic_layout_panel")
if any(e.get("name") == "kami_alive_material_mode" for e in root.iter()):
    raise RuntimeError("KAMI ALIVE graphics toggle already exists")
alive = ET.Element("check_box", {
    "name": "kami_alive_material_mode",
    "label": "KAMI ALIVE  |  Adaptive Material Response (Beta)",
    "control_name": "KAMIAliveEnabled",
    "layout": "topleft",
    "left": "10", "top": "8", "height": "20", "width": "475",
    "tool_tip": "Optional local enhancement for smooth metallic PBR surfaces. "
                "Other materials are unchanged; effect reduces under sustained load.",
})
hint = ET.Element("text", {
    "name": "kami_alive_beta_description",
    "layout": "topleft", "left": "28", "top_pad": "2",
    "height": "21", "width": "458", "font": "SansSerifSmall",
    "text_color": "0.62 0.78 0.9 1",
})
hint.text = "Off by default  |  No extra post-processing pass  |  CPU frame-time governor"
basic.insert(0, alive)
basic.insert(1, hint)
basic.set("height", str(int(basic.get("height")) + 54))
stack = one("layout_stack", "gfx_stack")
stack.set("height", str(int(stack.get("height")) + 54))

after = bindings()
assert len(after) == len(before) + 1
assert all(entry in after for entry in before), "A legacy renderer control was removed"
assert after.count(("check_box", "kami_alive_material_mode", "KAMIAliveEnabled")) == 1

ET.indent(tree, space=" ")
tree.write(GRAPHICS, encoding="utf-8", xml_declaration=True)
ET.parse(GRAPHICS)
print(f"PASS KAMI graphics control deck: 3 themed sections, "
      f"{len(section_names)} renamed subheaders, {expanded} widened "
      "checkboxes, and one opt-in ALIVE control. All original bindings retained.")
