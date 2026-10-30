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


def map_values(node, label):
    require(node.tag == "map", f"{label}: expected LLSD map")
    children = list(node)
    require(len(children) % 2 == 0, f"{label}: truncated LLSD pairs")
    values = {}
    for i in range(0, len(children), 2):
        key, value = children[i], children[i + 1]
        require(key.tag == "key" and bool(key.text), f"{label}: invalid key at pair {i // 2}")
        require(key.text not in values, f"{label}: duplicate key {key.text}")
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
setting_values = map_values(settings[0], "Native settings")
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
    slots.append(slot)
require(slots == list(range(len(records))),
        "Keyboard bindings: duplicate, missing or out-of-order slots")

print(f"PASS settings LLSD numeric types ({len(setting_values)} keys)")
print(f"PASS keyboard LLSD {format_name} ({len(records)} records); slots preserved")
