#!/usr/bin/env python3
"""Fail the KAMI Windows build if removed legacy avatar lighting returns."""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
FILES = (
    "indra/newview/app_settings/settings_blackdragon.xml",
    "indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml",
    "indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl",
    "indra/newview/lldrawpoolavatar.cpp",
)
REMOVED = (
    "KAMIAvatarFillStrength",
    "KAMIAvatarSoftShading",
    "kami_avatar_fill",
    "kami_avatar_soft_shading",
    "kami_legacy_fill",
    "kami_legacy_soft",
    "kami_avatarlight_heading",
    "kami_avatarlight_warning",
)
for filename in FILES:
    text = (ROOT / filename).read_text(encoding="utf-8-sig")
    for key in REMOVED:
        if key in text:
            raise AssertionError(f"Removed avatar lighting remains in {filename}: {key}")
    if filename.endswith(".xml"):
        ET.parse(ROOT / filename)

panel = (ROOT / FILES[1]).read_text(encoding="utf-8-sig")
assert "RenderDeferredSSAO" in panel, "Built-in contact shading was lost"
assert "RenderVolumetricLighting" in panel, "Built-in volumetric lighting was lost"
print("PASS: experimental legacy avatar lighting removed; existing graphics preserved.")
