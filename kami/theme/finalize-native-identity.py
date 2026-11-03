#!/usr/bin/env python3
"""Finish visible KAMI-only labeling without altering SL protocol or credits.

Runs after feature integrations. Names, callbacks, settings, filenames, paths,
copyright and About/Third-party attribution are intentionally never rewritten.
"""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
XUI = ROOT / "indra/newview/skins/default/xui/en"
PUBLIC_FIELDS = {
    "label", "title", "tool_tip", "tool_tip_text", "tooltip", "description",
    "placeholder_text", "short_title", "prompt", "help_text", "label_text",
    "header_text", "text",
}
VISIBLE_TEXT = {"text", "string", "text_editor", "static_text"}
PATTERNS = (
    (re.compile(r"\\bBlack[ -]?Dragon\\b", re.I), "KAMI"),
    (re.compile(r"\\bKombatant\\b", re.I), "KAMI"),
)
# Keep source acknowledgement intact wherever it appears (and all About pages).
ATTRIBUTION = re.compile(r"niranv|copyright|licen[cs]e|attribution|based on|original author", re.I)
EXEMPT = {"floater_about.xml", "floater_about_land.xml"}

def rebrand(text):
    if not text or ATTRIBUTION.search(text):
        return text
    for regex, replacement in PATTERNS:
        text = regex.sub(replacement, text)
    return text

def finalize_tree(root):
    changes = 0
    for element in root.iter():
        # Internal menu IDs (e.g. Dragon.*) must remain unchanged.
        if element.tag == "menu" and element.get("name") == "Me" and element.get("label") == "Dragon":
            element.set("label", "KAMI")
            changes += 1
        for key in PUBLIC_FIELDS:
            if key in element.attrib:
                old = element.attrib[key]
                new = rebrand(old)
                if new != old:
                    element.attrib[key] = new
                    changes += 1
        if element.tag in VISIBLE_TEXT and element.text:
            old = element.text
            new = rebrand(old)
            if new != old:
                element.text = new
                changes += 1
    return changes

def control_signature(root):
    # Changes must be exclusively visible text; this includes callbacks,
    # settings and all XUI control identifiers.
    return [(e.tag, tuple(sorted((k, v) for k, v in e.attrib.items()
                                  if k not in PUBLIC_FIELDS)),
             tuple((c.tag, tuple(sorted(c.attrib.items())))
                   for c in e if "." in c.tag))
            for e in root.iter()]

def main():
    changed_files = 0
    changed_labels = 0
    for path in sorted(XUI.glob("*.xml")):
        if path.name in EXEMPT:
            continue
        tree = ET.parse(path)
        root = tree.getroot()
        before = control_signature(root)
        changed = finalize_tree(root)
        assert before == control_signature(root), f"Control IDs changed: {path}"
        if changed:
            ET.indent(tree, space=" ")
            tree.write(path, encoding="utf-8", xml_declaration=True)
            ET.parse(path)
            changed_files += 1
            changed_labels += changed
    print(f"PASS: KAMI-visible labels finalized in {changed_files} XUI files ({changed_labels} text changes); original action IDs untouched")

if __name__ == "__main__":
    main()
