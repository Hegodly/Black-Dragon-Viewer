#!/usr/bin/env python3
"""Validate KAMI settings and both supported Black Dragon keyboard LLSD formats.

Old Black Dragon installations store one LLSD <map> document per line.
Creator Studio writes one valid LLSD <array> document containing those maps.
The viewer accepts both to protect existing resident key customizations.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def map_values(node, label, allow_duplicate_settings=False):
    require(node.tag == "map", f"{label}: expected LLSD map")
    children = list(node)
    require(len(children) % 2 == 0, f"{label}: truncated LLSD pairs")
    values = {}
    for i in range(0, len(children), 2):
        key, value = children[i], children[i + 1]
        require(key.tag == "key" and bool(key.text), f"{label}: invalid key at pair {i // 2}")
        if key.text in values:
            require(allow_duplicate_settings, f"{label}: duplicate key {key.text}")
            # Old Black Dragon settings contain historical duplicate names.
            # Preserve the viewer\'s last-definition-wins behavior and warn.
            print(f"WARN: {label}: duplicate legacy key {key.text}; last value used")
        values[key.text] = value
    return values


def read_binding_maps(path):
    data = path.read_text(encoding="utf-8-sig")
    try:
        document = ET.fromstring(data)
    except ET.ParseError:
        # Historical Black Dragon files contain multiple top-level LLSD
        # documents. Only fall back to that format, never silently drop rows.
        maps = []
        for line_no, line in enumerate(data.splitlines(), 1):
            if not line.strip():
                continue
            try:
                document = ET.fromstring(line)
            except ET.ParseError as exc:
                raise AssertionError(f"Legacy keyboard record {line_no}: malformed LLSD") from exc
            require(document.tag == "llsd" and len(document) == 1,
                    f"Legacy keyboard record {line_no}: expected LLSD root")
            require(document[0].tag == "map",
                    f"Legacy keyboard record {line_no}: expected map")
            maps.append(document[0])
        return maps, "legacy multi-document"
    require(document.tag == "llsd" and len(document) == 1,
            "Keyboard bindings: expected single LLSD root")
    child = document[0]
    if child.tag == "array":
        maps = list(child)
    elif child.tag == "map":
        maps = [child]
    else:
        raise AssertionError("Keyboard bindings: expected array or map")
    require(all(node.tag == "map" for node in maps),
            "Keyboard bindings: array contains non-map record")
    return maps, "single-document"


settings = ET.parse(ROOT / "indra/newview/app_settings/settings_blackdragon.xml").getroot()
require(settings.tag == "llsd" and len(settings) == 1,
        "Native settings: invalid LLSD root")
setting_values = map_values(settings[0], "Native settings", allow_duplicate_settings=True)
for key, definition in setting_values.items():
    require(definition.tag == "map", f"Native setting {key}: expected map")
    for element in definition.iter("integer"):
        literal = (element.text or "").strip()
        try:
            int(literal)
        except ValueError as exc:
            raise AssertionError(
                f"Native setting {key}: invalid integer {literal!r}; use <real> for floats"
            ) from exc

controls = ROOT / "indra/newview/app_settings/controls.xml"
records, format_name = read_binding_maps(controls)
require(len(records) > 100, "Keyboard bindings: records unexpectedly truncated")
slots = []
third_person_defaults = {}
for index, record in enumerate(records):
    values = map_values(record, f"Keyboard record {index}")
    require("function" in values and (values["function"].text or "").strip(),
            f"Keyboard record {index}: missing function")
    require("key" in values and "mask" in values and "mode" in values and "slot" in values,
            f"Keyboard record {index}: incomplete binding")
    try:
        mode = int(values["mode"].text or "")
        slot = int(values["slot"].text or "")
    except ValueError as exc:
        raise AssertionError(f"Keyboard record {index}: invalid mode or slot") from exc
    require(0 <= mode < 5, f"Keyboard record {index}: invalid mode {mode}")
    if mode == 0:
        key_name = (values["key"].text or "").strip()
        mask_name = (values["mask"].text or "").strip()
        binding = (key_name, mask_name)
        require(binding not in third_person_defaults,
                f"Third-person keyboard: duplicate {binding}")
        third_person_defaults[binding] = values["function"].text
    slots.append(slot)
require(slots == list(range(len(records))),
        "Keyboard bindings: duplicate, missing or out-of-order slots")

for key_name, mask_name, expected in (
    ("W", "NONE", "push_forward"), ("S", "NONE", "push_backward"),
    ("A", "NONE", "turn_left"), ("D", "NONE", "turn_right"),
    ("Left", "NONE", "turn_left"), ("Right", "NONE", "turn_right"),
    ("A", "SHIFT", "slide_left"), ("D", "SHIFT", "slide_right"),
):
    actual = third_person_defaults.get((key_name, mask_name))
    require(actual == expected,
            f"Third-person {key_name}/{mask_name}: expected {expected}, got {actual}")

print(f"PASS settings LLSD numeric types ({len(setting_values)} keys)")
print(f"PASS keyboard LLSD {format_name} ({len(records)} records); slots preserved")
