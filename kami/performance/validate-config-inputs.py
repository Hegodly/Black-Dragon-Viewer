#!/usr/bin/env python3
"""Verify KAMI native LLSD settings and line-delimited keyboard mapping.

Black Dragon deliberately stores one <llsd> root per controls.xml line.
Parsing all records as one XML tree destroys this valid native keybind format.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
settings = ET.parse(ROOT / "indra/newview/app_settings/settings_blackdragon.xml")
assert settings.getroot().tag == "llsd" and settings.getroot().find("map") is not None
# Validate each named setting independently so CI identifies the source key,
# not just the malformed text. Never silently coerce float values to integers.
root = settings.getroot().find("map")
entries = list(root)
assert len(entries) % 2 == 0, "Settings key/value pairs are truncated"
for i in range(0, len(entries), 2):
    key_element, setting = entries[i], entries[i + 1]
    assert key_element.tag == "key" and setting.tag == "map", ("Invalid settings structure", i)
    key = key_element.text or "<unnamed>"
    for element in setting.iter("integer"):
        literal = (element.text or "").strip()
        try:
            int(literal)
        except ValueError as exc:
            raise AssertionError(
                f"Invalid integer LLSD literal in setting {key}: {literal!r}. "
                "Use <real> for F32 values."
            ) from exc

controls = ROOT / "indra/newview/app_settings/controls.xml"
rows = [row.strip() for row in controls.read_text(encoding="utf-8-sig").splitlines() if row.strip()]
assert len(rows) > 100, "Keyboard records truncated"
slots = []
for line, row in enumerate(rows, 1):
    node = ET.fromstring(row)
    assert node.tag == "llsd" and len(node) == 1 and node[0].tag == "map", line
    fields = list(node[0])
    assert len(fields) % 2 == 0, line
    mapping = {fields[i].text: fields[i + 1].text
               for i in range(0, len(fields), 2)}
    assert mapping.get("function"), ("Missing keybinding function", line)
    slots.append(int(mapping["slot"]))
assert slots == list(range(len(slots))), "Keyboard slots duplicated or missing"
print("PASS native settings LLSD numeric types")
print(f"PASS {len(rows)} original keyboard LLSD records, order preserved")
